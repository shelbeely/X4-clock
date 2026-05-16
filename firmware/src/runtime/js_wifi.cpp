/*
 * js_wifi.cpp — wifi.* JavaScript bindings for Xteink X4
 *
 * Exposes the WiFi driver (wifi_manager.h) to JavaScript.
 *
 * JS API:
 *   wifi.connect(ssid, pass)  → bool  — connect in station mode
 *   wifi.startAP(ssid, pass)  → bool  — start SoftAP
 *   wifi.disconnect()         → void  — disconnect / turn off WiFi
 *   wifi.connected()          → bool  — true when associated to an AP
 *   wifi.ip()                 → string — current IP address
 */

#include "drivers/wifi_manager.h"
#include "mquickjs.h"
#include <Arduino.h>

extern "C" {

// wifi.connect(ssid, pass)  → bool
JSValue js_x4_wifi_connect(JSContext *ctx, JSValue *this_val,
                            int argc, JSValue *argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "wifi.connect(ssid[, pass])");

    JSCStringBuf sbuf, pbuf;
    const char *ssid = JS_ToCString(ctx, argv[0], &sbuf);
    if (!ssid) return JS_EXCEPTION;

    const char *pass = "";
    if (argc >= 2) {
        pass = JS_ToCString(ctx, argv[1], &pbuf);
        if (!pass) pass = "";
    }

    bool ok = wifi_connect(ssid, pass);
    return JS_NewBool(ok ? 1 : 0);
}

// wifi.startAP(ssid, pass)  → bool
JSValue js_x4_wifi_startAP(JSContext *ctx, JSValue *this_val,
                            int argc, JSValue *argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "wifi.startAP(ssid[, pass])");

    JSCStringBuf sbuf, pbuf;
    const char *ssid = JS_ToCString(ctx, argv[0], &sbuf);
    if (!ssid) return JS_EXCEPTION;

    const char *pass = "";
    if (argc >= 2) {
        pass = JS_ToCString(ctx, argv[1], &pbuf);
        if (!pass) pass = "";
    }

    bool ok = wifi_start_ap(ssid, pass);
    return JS_NewBool(ok ? 1 : 0);
}

// wifi.disconnect()
JSValue js_x4_wifi_disconnect(JSContext *ctx, JSValue *this_val,
                               int argc, JSValue *argv) {
    wifi_disconnect();
    return JS_UNDEFINED;
}

// wifi.connected()  → bool
JSValue js_x4_wifi_connected(JSContext *ctx, JSValue *this_val,
                              int argc, JSValue *argv) {
    return JS_NewBool(wifi_connected() ? 1 : 0);
}

// wifi.ip()  → string
JSValue js_x4_wifi_ip(JSContext *ctx, JSValue *this_val,
                       int argc, JSValue *argv) {
    return JS_NewString(ctx, wifi_ip());
}

} // extern "C"
