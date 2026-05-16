/*
 * js_engine.cpp — MicroQuickJS context lifecycle
 *
 * Provides the static 64 KB memory buffer, context creation/destruction,
 * source/bytecode execution, and the stdlib shims that mquickjs expects
 * (js_gc, js_print, js_date_now, js_performance_now).
 */

#include "js_engine.h"
#include "js_input.h"
#include "js_system.h"
#include <Arduino.h>

// Forward declarations for all native binding functions referenced in x4_stdlib.h.
// Each function is defined as extern "C" in its respective runtime source file.
extern "C" {
    // stdlib shims (defined below in this file)
    extern JSCFunction js_gc;
    extern JSCFunction js_print;
    extern JSCFunction js_date_now;
    extern JSCFunction js_performance_now;
    // display.* bindings (js_display.cpp)
    extern JSCFunction js_x4_display_clear;
    extern JSCFunction js_x4_display_print;
    extern JSCFunction js_x4_display_drawRect;
    extern JSCFunction js_x4_display_drawBitmap;
    extern JSCFunction js_x4_display_refresh;
    extern JSCFunction js_x4_display_partialRefresh;
    extern JSCFunction js_x4_display_width;
    extern JSCFunction js_x4_display_height;
    extern JSCFunction js_x4_display_hibernate;
    extern JSCFunction js_x4_display_wake;
    extern JSCFunction js_x4_display_setRotation;
    extern JSCFunction js_x4_display_rotation;
    // input.* bindings (js_input.cpp)
    extern JSCFunction js_x4_input_onButton;
    // fs.* bindings (js_fs.cpp)
    extern JSCFunction js_x4_fs_open;
    extern JSCFunction js_x4_fs_read;
    extern JSCFunction js_x4_fs_write;
    extern JSCFunction js_x4_fs_close;
    extern JSCFunction js_x4_fs_seek;
    extern JSCFunction js_x4_fs_size;
    extern JSCFunction js_x4_fs_list;
    extern JSCFunction js_x4_fs_exists;
    // system.* bindings (js_system.cpp)
    extern JSCFunction js_x4_system_millis;
    extern JSCFunction js_x4_system_battery;
    extern JSCFunction js_x4_system_batteryLow;
    extern JSCFunction js_x4_system_sleep;
    extern JSCFunction js_x4_system_lightSleep;
    extern JSCFunction js_x4_system_setIdleTimeout;
    extern JSCFunction js_x4_system_setRefreshInterval;
    extern JSCFunction js_x4_system_log;
    extern JSCFunction js_x4_system_appName;
    extern JSCFunction js_x4_system_time;
    extern JSCFunction js_x4_system_setTime;
    extern JSCFunction js_x4_system_syncTime;
    // wifi.* bindings (js_wifi.cpp)
    extern JSCFunction js_x4_wifi_connect;
    extern JSCFunction js_x4_wifi_startAP;
    extern JSCFunction js_x4_wifi_disconnect;
    extern JSCFunction js_x4_wifi_connected;
    extern JSCFunction js_x4_wifi_ip;
    // http.* bindings (js_http_client.cpp)
    extern JSCFunction js_x4_http_get;
    extern JSCFunction js_x4_http_getAsync;
    // server.* bindings (js_http_server.cpp)
    extern JSCFunction js_x4_server_begin;
    extern JSCFunction js_x4_server_stop;
    extern JSCFunction js_x4_server_onRequest;
    extern JSCFunction js_x4_server_send;
    extern JSCFunction js_x4_server_handleClient;
    // notify.* bindings (js_notify.cpp)
    extern JSCFunction js_x4_notify_count;
    extern JSCFunction js_x4_notify_get;
    extern JSCFunction js_x4_notify_dismiss;
    extern JSCFunction js_x4_notify_reload;
    // weather.* bindings (js_weather.cpp)
    extern JSCFunction js_x4_weather_refresh;
    extern JSCFunction js_x4_weather_valid;
    extern JSCFunction js_x4_weather_temp;
    extern JSCFunction js_x4_weather_humidity;
    extern JSCFunction js_x4_weather_condition;
    extern JSCFunction js_x4_weather_city;
    extern JSCFunction js_x4_weather_age;
    // calendar.* bindings (js_calendar.cpp)
    extern JSCFunction js_x4_calendar_count;
    extern JSCFunction js_x4_calendar_get;
    extern JSCFunction js_x4_calendar_upcoming;
    extern JSCFunction js_x4_calendar_add;
    extern JSCFunction js_x4_calendar_remove;
    extern JSCFunction js_x4_calendar_reload;
    // reminder.* bindings (js_reminder.cpp)
    extern JSCFunction js_x4_reminder_count;
    extern JSCFunction js_x4_reminder_get;
    extern JSCFunction js_x4_reminder_due;
    extern JSCFunction js_x4_reminder_dismiss;
    extern JSCFunction js_x4_reminder_add;
    extern JSCFunction js_x4_reminder_remove;
    extern JSCFunction js_x4_reminder_reload;
}

// The generated stdlib header (produced by fetch_mquickjs.sh) — must be
// included in exactly ONE translation unit so the function table is defined.
// The extern "C" wrapper prevents C++ name-mangling of the table symbols.
extern "C" {
#include "x4_stdlib.h"
}

// ---------------------------------------------------------------------------
// Static 64 KB JS heap — single buffer reused across app runs
// ---------------------------------------------------------------------------
static uint8_t s_js_mem[JS_MEM_SIZE];

// ---------------------------------------------------------------------------
// Stdlib shims required by the x4_stdlib.h function table.
//
// These functions are referenced BY NAME in x4_stdlib.c (as strings in the
// JSPropDef macros) and appear as extern declarations in x4_stdlib.h.
// They must therefore have C linkage and exactly match the JSCFunction type.
// ---------------------------------------------------------------------------
extern "C" {

// gc() — trigger the compacting garbage collector
JSValue js_gc(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    JS_GC(ctx);
    return JS_UNDEFINED;
}

// console.log / print() — route to Arduino Serial
JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    for (int i = 0; i < argc; i++) {
        if (i != 0) Serial.print(' ');
        if (JS_IsString(ctx, argv[i])) {
            JSCStringBuf buf;
            const char *str = JS_ToCString(ctx, argv[i], &buf);
            if (str) Serial.print(str);
        } else {
            JS_PrintValue(ctx, argv[i]);
        }
    }
    Serial.println();
    return JS_UNDEFINED;
}

// Date.now() — milliseconds since boot (no RTC)
JSValue js_date_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) {
    return JS_NewInt64(ctx, (int64_t)millis());
}

// performance.now() — same as Date.now() on this platform
JSValue js_performance_now(JSContext *ctx, JSValue *this_val, int argc,
                           JSValue *argv) {
    return JS_NewInt64(ctx, (int64_t)millis());
}

} // extern "C"

// ---------------------------------------------------------------------------
// mquickjs log function — routes JS_PrintValue / JS_DumpMemory to Serial
// ---------------------------------------------------------------------------
static void js_log_func(void *opaque, const void *buf, size_t len) {
    Serial.write(reinterpret_cast<const char *>(buf), len);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void js_engine_init() {
    // Nothing global to initialise — context is created per app run
}

JSContext *js_engine_new_context() {
    JSContext *ctx = JS_NewContext(s_js_mem, sizeof(s_js_mem), &js_stdlib);
    if (!ctx) {
        Serial.println("[JS] JS_NewContext failed — out of memory?");
        return nullptr;
    }

    JS_SetLogFunc(ctx, js_log_func);
    js_input_reset();

    Serial.printf("[JS] context ready (%u B heap)\n", (unsigned)sizeof(s_js_mem));
    return ctx;
}

bool js_engine_run_source(JSContext *ctx,
                          const char *src, size_t len,
                          const char *filename) {
    JSValue val = JS_Eval(ctx, src, len, filename, 0);
    if (JS_IsException(val)) {
        js_engine_dump_exception(ctx);
        return false;
    }
    return true;
}

bool js_engine_run_bytecode(JSContext *ctx, uint8_t *buf, size_t len) {
    if (!JS_IsBytecode(buf, len)) {
        Serial.println("[JS] run_bytecode: not valid mquickjs bytecode");
        return false;
    }
    if (JS_RelocateBytecode(ctx, buf, (uint32_t)len) != 0) {
        Serial.println("[JS] run_bytecode: relocation failed");
        return false;
    }
    JSValue fn = JS_LoadBytecode(ctx, buf);
    if (JS_IsException(fn)) {
        js_engine_dump_exception(ctx);
        return false;
    }
    JSValue val = JS_Run(ctx, fn);
    if (JS_IsException(val)) {
        js_engine_dump_exception(ctx);
        return false;
    }
    return true;
}

void js_engine_destroy_context(JSContext *ctx) {
    if (!ctx) return;
    js_input_reset();
    JS_FreeContext(ctx);
}

bool js_engine_call_func(JSContext *ctx, const char *name) {
    JSGCRef global_ref, fn_ref;
    JSValue *global_ptr = JS_PushGCRef(ctx, &global_ref);
    JSValue *fn_ptr     = JS_PushGCRef(ctx, &fn_ref);

    *global_ptr = JS_GetGlobalObject(ctx);
    *fn_ptr     = JS_GetPropertyStr(ctx, *global_ptr, name);

    bool ok = true;
    if (JS_IsFunction(ctx, *fn_ptr)) {
        // Stack slots: func + this
        if (!JS_StackCheck(ctx, 2)) {
            JS_PushArg(ctx, *fn_ptr);
            JS_PushArg(ctx, *global_ptr);
            JSValue ret = JS_Call(ctx, 0);
            if (JS_IsException(ret)) {
                js_engine_dump_exception(ctx);
                ok = false;
            }
        } else {
            Serial.println("[JS] call_func: stack overflow");
            ok = false;
        }
    } else {
        ok = false;  // function not found — not an error
    }

    JS_PopGCRef(ctx, &fn_ref);
    JS_PopGCRef(ctx, &global_ref);
    return ok;
}

void js_engine_dump_exception(JSContext *ctx) {
    JSValue exc = JS_GetException(ctx);
    Serial.print("[JS] Exception: ");
    JS_PrintValueF(ctx, exc, JS_DUMP_LONG);
    Serial.println();
}
