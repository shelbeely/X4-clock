/*
 * js_display.cpp — display.* JavaScript bindings for Xteink X4
 *
 * Every function is declared extern "C" to match the linkage expected by the
 * function table generated in x4_stdlib.h.  All functions follow the
 * mquickjs JSCFunction signature:
 *
 *   JSValue fn(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
 *
 * Note: JSValue is a 32-bit scalar on ESP32-C3 (JS_PTR64 is not defined).
 * The compacting GC can move objects during any JS allocation call, so local
 * JSValue variables must only be used *between* API calls (never stored
 * across one).  For temporary string conversions we use stack-allocated
 * JSCStringBuf which mquickjs keeps valid until the next allocation.
 */

#include "js_display.h"
#include "drivers/display.h"
#include "mquickjs.h"
#include <Arduino.h>

extern "C" {

JSValue js_x4_display_clear(JSContext *ctx, JSValue *this_val,
                             int argc, JSValue *argv) {
    display_clear();
    return JS_UNDEFINED;
}

// display.print(x, y, text, size)  — size 1–4
JSValue js_x4_display_print(JSContext *ctx, JSValue *this_val,
                             int argc, JSValue *argv) {
    if (argc < 3) return JS_ThrowTypeError(ctx, "display.print(x,y,text[,size])");

    int x = 0, y = 0, sz = 1;
    if (JS_ToInt32(ctx, &x, argv[0])) return JS_EXCEPTION;
    if (JS_ToInt32(ctx, &y, argv[1])) return JS_EXCEPTION;

    JSCStringBuf buf;
    const char *text = JS_ToCString(ctx, argv[2], &buf);
    if (!text) return JS_EXCEPTION;

    if (argc >= 4) JS_ToInt32(ctx, &sz, argv[3]);
    if (sz < 1) sz = 1;
    if (sz > 4) sz = 4;

    display_print((int16_t)x, (int16_t)y, text, (uint8_t)sz);
    return JS_UNDEFINED;
}

// display.drawRect(x, y, w, h, filled)
JSValue js_x4_display_drawRect(JSContext *ctx, JSValue *this_val,
                                int argc, JSValue *argv) {
    if (argc < 4) return JS_ThrowTypeError(ctx, "display.drawRect(x,y,w,h[,filled])");

    int x = 0, y = 0, w = 0, h = 0;
    if (JS_ToInt32(ctx, &x, argv[0])) return JS_EXCEPTION;
    if (JS_ToInt32(ctx, &y, argv[1])) return JS_EXCEPTION;
    if (JS_ToInt32(ctx, &w, argv[2])) return JS_EXCEPTION;
    if (JS_ToInt32(ctx, &h, argv[3])) return JS_EXCEPTION;

    bool filled = false;
    if (argc >= 5) filled = (bool)JS_VALUE_GET_SPECIAL_VALUE(argv[4]);

    display_draw_rect((int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, filled);
    return JS_UNDEFINED;
}

// display.drawBitmap(x, y, sdPath)
JSValue js_x4_display_drawBitmap(JSContext *ctx, JSValue *this_val,
                                  int argc, JSValue *argv) {
    if (argc < 3) return JS_ThrowTypeError(ctx, "display.drawBitmap(x,y,path)");

    int x = 0, y = 0;
    if (JS_ToInt32(ctx, &x, argv[0])) return JS_EXCEPTION;
    if (JS_ToInt32(ctx, &y, argv[1])) return JS_EXCEPTION;

    JSCStringBuf buf;
    const char *path = JS_ToCString(ctx, argv[2], &buf);
    if (!path) return JS_EXCEPTION;

    display_draw_bitmap((int16_t)x, (int16_t)y, path);
    return JS_UNDEFINED;
}

JSValue js_x4_display_refresh(JSContext *ctx, JSValue *this_val,
                               int argc, JSValue *argv) {
    display_refresh();
    return JS_UNDEFINED;
}

JSValue js_x4_display_partialRefresh(JSContext *ctx, JSValue *this_val,
                                      int argc, JSValue *argv) {
    display_partial_refresh();
    return JS_UNDEFINED;
}

JSValue js_x4_display_width(JSContext *ctx, JSValue *this_val,
                             int argc, JSValue *argv) {
    return JS_NewInt32(ctx, display_width());
}

JSValue js_x4_display_height(JSContext *ctx, JSValue *this_val,
                              int argc, JSValue *argv) {
    return JS_NewInt32(ctx, display_height());
}

// display.hibernate() — put the SSD1677 into lowest-power standby
JSValue js_x4_display_hibernate(JSContext *ctx, JSValue *this_val,
                                  int argc, JSValue *argv) {
    display_hibernate();
    return JS_UNDEFINED;
}

// display.wake() — exit standby before issuing drawing commands
JSValue js_x4_display_wake(JSContext *ctx, JSValue *this_val,
                             int argc, JSValue *argv) {
    display_wake();
    return JS_UNDEFINED;
}

// display.setRotation(r) — set screen rotation 0–3
JSValue js_x4_display_setRotation(JSContext *ctx, JSValue *this_val,
                                   int argc, JSValue *argv) {
    int r = 0;
    if (argc >= 1) JS_ToInt32(ctx, &r, argv[0]);
    display_set_rotation((uint8_t)r);
    return JS_UNDEFINED;
}

// display.rotation() → int (0–3)
JSValue js_x4_display_rotation(JSContext *ctx, JSValue *this_val,
                                int argc, JSValue *argv) {
    return JS_NewInt32(ctx, (int32_t)display_get_rotation());
}

} // extern "C"

// The closing brace above ends extern "C". The new bindings are added below.
