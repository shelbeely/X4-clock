/*
 * js_http_server.cpp — HTTP server bindings for Xteink X4
 *
 * Wraps the Arduino WebServer library to expose a route-based HTTP server
 * to JavaScript apps.
 *
 * JS API:
 *   server.begin(port)                    → void  — start server on port
 *   server.stop()                         → void  — stop server
 *   server.onRequest(path, fn)            → void  — register route handler
 *   server.send(code, contentType, body)  → void  — send HTTP response
 *   server.handleClient()                 → void  — process pending requests
 *
 * Route handlers are called synchronously from within handleClient() with
 * the HTTP method and request body as arguments:
 *   function handler(method, body) {
 *     server.send(200, "text/plain", "OK");
 *   }
 *
 * Up to SERVER_MAX_ROUTES routes can be registered.  Only one request
 * can be handled at a time (serial processing).
 */

#include "mquickjs.h"
#include <Arduino.h>
#include <WebServer.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define SERVER_MAX_ROUTES  8

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------

static WebServer *s_server = nullptr;
static JSContext *s_ctx    = nullptr;

struct RouteSlot {
    char    path[64];
    JSGCRef cb_ref;
    JSValue *cb_ptr;
    bool    active;
};

static RouteSlot s_routes[SERVER_MAX_ROUTES];

// ---------------------------------------------------------------------------
// Route dispatcher — called by WebServer from within handleClient()
// ---------------------------------------------------------------------------

static void dispatch_route(int idx) {
    if (!s_ctx || !s_routes[idx].active || !s_routes[idx].cb_ptr) return;
    if (JS_IsUndefined(*s_routes[idx].cb_ptr)) return;

    JSContext *ctx = s_ctx;

    // Build method string
    String method_str;
    switch (s_server->method()) {
        case HTTP_GET:    method_str = "GET";    break;
        case HTTP_POST:   method_str = "POST";   break;
        case HTTP_PUT:    method_str = "PUT";    break;
        case HTTP_DELETE: method_str = "DELETE"; break;
        default:          method_str = "OTHER";  break;
    }

    // Get request body (plain arg for POST/PUT, or empty)
    String body_str = s_server->arg("plain");

    if (JS_StackCheck(ctx, 4)) {
        Serial.println("[server] dispatch: stack overflow");
        s_server->send(500, "text/plain", "Stack overflow");
        return;
    }

    JSValue method_val = JS_NewString(ctx, method_str.c_str());
    JSValue body_val   = JS_NewString(ctx, body_str.c_str());

    JS_PushArg(ctx, method_val);
    JS_PushArg(ctx, body_val);
    JS_PushArg(ctx, *s_routes[idx].cb_ptr);
    JS_PushArg(ctx, JS_UNDEFINED);   // this
    JSValue ret = JS_Call(ctx, 2);
    if (JS_IsException(ret)) {
        Serial.println("[server] route handler threw an exception");
        // Send a 500 if nothing was sent by the handler
    }
}

// ---------------------------------------------------------------------------
// Route registration helper — adds a C++ lambda for each route
// ---------------------------------------------------------------------------

static void register_c_route(int idx) {
    if (!s_server) return;
    s_server->on(s_routes[idx].path, [idx]() {
        dispatch_route(idx);
    });
}

// ---------------------------------------------------------------------------
// JS bindings
// ---------------------------------------------------------------------------

extern "C" {

// server.begin(port)
JSValue js_x4_server_begin(JSContext *ctx, JSValue *this_val,
                            int argc, JSValue *argv) {
    int port = 80;
    if (argc >= 1) JS_ToInt32(ctx, &port, argv[0]);

    if (s_server) {
        s_server->stop();
        delete s_server;
    }

    // Clear route table when restarting
    for (int i = 0; i < SERVER_MAX_ROUTES; i++) {
        s_routes[i].active  = false;
        s_routes[i].cb_ptr  = nullptr;
        s_routes[i].path[0] = '\0';
    }

    s_server = new WebServer(port);
    s_ctx    = ctx;
    s_server->begin();
    Serial.printf("[server] started on port %d\n", port);
    return JS_UNDEFINED;
}

// server.stop()
JSValue js_x4_server_stop(JSContext *ctx, JSValue *this_val,
                           int argc, JSValue *argv) {
    if (s_server) {
        s_server->stop();
        delete s_server;
        s_server = nullptr;
    }
    s_ctx = nullptr;
    Serial.println("[server] stopped");
    return JS_UNDEFINED;
}

// server.onRequest(path, fn)
JSValue js_x4_server_onRequest(JSContext *ctx, JSValue *this_val,
                                int argc, JSValue *argv) {
    if (argc < 2) return JS_ThrowTypeError(ctx, "server.onRequest(path, fn)");
    if (!JS_IsFunction(ctx, argv[1]))
        return JS_ThrowTypeError(ctx, "server.onRequest: second argument must be a function");
    if (!s_server)
        return JS_ThrowTypeError(ctx, "server.onRequest: call server.begin() first");

    JSCStringBuf pbuf;
    const char *path = JS_ToCString(ctx, argv[0], &pbuf);
    if (!path) return JS_EXCEPTION;

    // Find existing slot for this path, or allocate new slot
    int slot = -1;
    for (int i = 0; i < SERVER_MAX_ROUTES; i++) {
        if (s_routes[i].active && strncmp(s_routes[i].path, path, 63) == 0) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        for (int i = 0; i < SERVER_MAX_ROUTES; i++) {
            if (!s_routes[i].active) { slot = i; break; }
        }
    }
    if (slot < 0)
        return JS_ThrowTypeError(ctx, "server.onRequest: too many routes (max 8)");

    strncpy(s_routes[slot].path, path, 63);
    s_routes[slot].path[63] = '\0';

    if (s_routes[slot].cb_ptr == nullptr) {
        s_routes[slot].cb_ptr = JS_AddGCRef(ctx, &s_routes[slot].cb_ref);
    }
    *s_routes[slot].cb_ptr = argv[1];
    s_routes[slot].active  = true;

    register_c_route(slot);
    return JS_UNDEFINED;
}

// server.send(code, contentType, body)
JSValue js_x4_server_send(JSContext *ctx, JSValue *this_val,
                           int argc, JSValue *argv) {
    if (!s_server) return JS_UNDEFINED;

    int code = 200;
    if (argc >= 1) JS_ToInt32(ctx, &code, argv[0]);

    JSCStringBuf tbuf, bbuf;
    const char *content_type = "text/plain";
    const char *body         = "";

    if (argc >= 2) {
        const char *t = JS_ToCString(ctx, argv[1], &tbuf);
        if (t) content_type = t;
    }
    if (argc >= 3) {
        const char *b = JS_ToCString(ctx, argv[2], &bbuf);
        if (b) body = b;
    }

    s_server->send(code, content_type, body);
    return JS_UNDEFINED;
}

// server.handleClient()
JSValue js_x4_server_handleClient(JSContext *ctx, JSValue *this_val,
                                   int argc, JSValue *argv) {
    if (s_server) {
        s_ctx = ctx;   // keep context current for route dispatch
        s_server->handleClient();
    }
    return JS_UNDEFINED;
}

} // extern "C"
