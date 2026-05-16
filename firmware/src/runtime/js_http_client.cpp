/*
 * js_http_client.cpp — HTTP client bindings for Xteink X4
 *
 * Provides synchronous (http.get) and pseudo-async (http.getAsync) HTTP GET.
 *
 * http.get(url) blocks until the request completes or times out.
 *
 * http.getAsync(url, cb) starts the request in a background FreeRTOS task and
 * returns immediately.  The callback cb(errorMsg, body) is invoked on the next
 * js_http_poll() call once the response is ready.  Only one async request can
 * be in flight at a time; subsequent calls are rejected until the previous one
 * completes.
 *
 * HTTP response bodies are capped at HTTP_MAX_BODY_BYTES to stay within the
 * 64 KB JS heap budget.
 */

#include "js_http_client.h"
#include "mquickjs.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define HTTP_MAX_BODY_BYTES   4096
#define HTTP_TIMEOUT_MS       10000
#define HTTP_TASK_STACK_SIZE  8192
#define ASYNC_CLEANUP_TIMEOUT_MS   500
#define ASYNC_CLEANUP_POLL_MS       10

// ---------------------------------------------------------------------------
// Async request state (shared between the FreeRTOS task and the main thread)
// ---------------------------------------------------------------------------

// Async result buffer — written by background task, read by main thread.
// Volatile ensures the compiler does not cache the flag across context switches.
static volatile bool s_async_done    = false;
static volatile bool s_async_active  = false;
static int           s_async_code    = 0;
static char          s_async_body[HTTP_MAX_BODY_BYTES + 1];
static char          s_async_url[512];

static JSGCRef  s_async_cb_ref;
static JSValue *s_async_cb_ptr = nullptr;

static TaskHandle_t s_async_task_handle = nullptr;

// ---------------------------------------------------------------------------
// Background HTTP task (no JS calls — not thread-safe)
// ---------------------------------------------------------------------------

static void http_fetch_task(void *param) {
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);

    s_async_code    = -1;
    s_async_body[0] = '\0';

    if (http.begin(s_async_url)) {
        int code = http.GET();
        s_async_code = code;
        if (code == HTTP_CODE_OK) {
            // Collect up to HTTP_MAX_BODY_BYTES characters
            WiFiClient *stream = http.getStreamPtr();
            if (stream) {
                size_t total = 0;
                while (total < HTTP_MAX_BODY_BYTES && stream->available()) {
                    int c = stream->read();
                    if (c < 0) break;
                    s_async_body[total++] = (char)c;
                }
                s_async_body[total] = '\0';
            }
        }
        http.end();
    }

    s_async_done         = true;
    s_async_task_handle  = nullptr;
    vTaskDelete(nullptr);
}

// ---------------------------------------------------------------------------
// Synchronous GET
// ---------------------------------------------------------------------------

extern "C" {

// http.get(url)  → string body, or "" on error
JSValue js_x4_http_get(JSContext *ctx, JSValue *this_val,
                        int argc, JSValue *argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "http.get(url)");

    JSCStringBuf ubuf;
    const char *url = JS_ToCString(ctx, argv[0], &ubuf);
    if (!url) return JS_EXCEPTION;

    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);

    if (!http.begin(url)) {
        return JS_NewStringLen(ctx, "", 0);
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        return JS_NewStringLen(ctx, "", 0);
    }

    // Read up to HTTP_MAX_BODY_BYTES
    static char s_sync_body[HTTP_MAX_BODY_BYTES + 1];
    WiFiClient *stream = http.getStreamPtr();
    size_t total = 0;
    if (stream) {
        while (total < HTTP_MAX_BODY_BYTES && stream->available()) {
            int c = stream->read();
            if (c < 0) break;
            s_sync_body[total++] = (char)c;
        }
    }
    s_sync_body[total] = '\0';
    http.end();

    return JS_NewStringLen(ctx, s_sync_body, total);
}

// http.getAsync(url, callback)  → void
// callback signature: function(error, body)
//   error: "" on success, error message string on failure
//   body:  response body string on success, "" on failure
JSValue js_x4_http_getAsync(JSContext *ctx, JSValue *this_val,
                             int argc, JSValue *argv) {
    if (argc < 2) return JS_ThrowTypeError(ctx, "http.getAsync(url, callback)");
    if (!JS_IsFunction(ctx, argv[1]))
        return JS_ThrowTypeError(ctx, "http.getAsync: second argument must be a function");

    // Reject if a request is already in flight
    if (s_async_active) {
        return JS_ThrowTypeError(ctx, "http.getAsync: request already in progress");
    }

    JSCStringBuf ubuf;
    const char *url = JS_ToCString(ctx, argv[0], &ubuf);
    if (!url) return JS_EXCEPTION;

    strncpy(s_async_url, url, sizeof(s_async_url) - 1);
    s_async_url[sizeof(s_async_url) - 1] = '\0';

    // Store callback in a GC-tracked reference
    if (s_async_cb_ptr == nullptr) {
        s_async_cb_ptr = JS_AddGCRef(ctx, &s_async_cb_ref);
    }
    *s_async_cb_ptr = argv[1];

    s_async_done   = false;
    s_async_active = true;
    s_async_code   = -1;
    s_async_body[0] = '\0';

    xTaskCreate(http_fetch_task, "http_get", HTTP_TASK_STACK_SIZE, nullptr, 1, &s_async_task_handle);

    return JS_UNDEFINED;
}

} // extern "C"

// ---------------------------------------------------------------------------
// Async poll — called from app_loader after each loop() iteration
// ---------------------------------------------------------------------------

void js_http_poll(JSContext *ctx) {
    if (!s_async_active || !s_async_done) return;

    s_async_active = false;
    s_async_done   = false;

    if (!s_async_cb_ptr || JS_IsUndefined(*s_async_cb_ptr)) return;

    // Build error and body arguments
    JSValue err_val, body_val;
    if (s_async_code == HTTP_CODE_OK) {
        err_val  = JS_NewStringLen(ctx, "", 0);
        body_val = JS_NewString(ctx, s_async_body);
    } else {
        char errbuf[32];
        snprintf(errbuf, sizeof(errbuf), "HTTP %d", s_async_code);
        err_val  = JS_NewString(ctx, s_async_code < 0 ? "connect failed" : errbuf);
        body_val = JS_NewStringLen(ctx, "", 0);
    }

    if (JS_StackCheck(ctx, 4)) {
        Serial.println("[http] poll: stack overflow");
        return;
    }

    JS_PushArg(ctx, err_val);
    JS_PushArg(ctx, body_val);
    JS_PushArg(ctx, *s_async_cb_ptr);
    JS_PushArg(ctx, JS_UNDEFINED);  // this
    JSValue ret = JS_Call(ctx, 2);
    if (JS_IsException(ret)) {
        Serial.println("[http] async callback threw an exception");
    }
}

void js_http_reset() {
    // If a task is running, wait for it to finish (up to ASYNC_CLEANUP_TIMEOUT_MS)
    if (s_async_task_handle) {
        uint32_t deadline = millis() + ASYNC_CLEANUP_TIMEOUT_MS;
        while (s_async_task_handle && millis() < deadline) {
            vTaskDelay(pdMS_TO_TICKS(ASYNC_CLEANUP_POLL_MS));
        }
    }
    s_async_active  = false;
    s_async_done    = false;
    s_async_cb_ptr  = nullptr;
    memset(&s_async_cb_ref, 0, sizeof(s_async_cb_ref));
}
