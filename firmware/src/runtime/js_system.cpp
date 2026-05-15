/*
 * js_system.cpp — system.* JavaScript bindings for Xteink X4
 *
 * Exposes: millis, battery%, deep-sleep, serial log, app name.
 */

#include "js_system.h"
#include "drivers/battery.h"
#include "mquickjs.h"
#include <Arduino.h>
#include <esp_sleep.h>

// Current app filename — set by app_loader before launching each app
static char s_app_name[64] = "unknown";

void js_system_set_app_name(const char *name) {
    strncpy(s_app_name, name ? name : "unknown", sizeof(s_app_name) - 1);
    s_app_name[sizeof(s_app_name) - 1] = '\0';
}

extern "C" {

// system.millis()  → int
JSValue js_x4_system_millis(JSContext *ctx, JSValue *this_val,
                             int argc, JSValue *argv) {
    return JS_NewInt64(ctx, (int64_t)millis());
}

// system.battery()  → int (0–100)
JSValue js_x4_system_battery(JSContext *ctx, JSValue *this_val,
                              int argc, JSValue *argv) {
    return JS_NewInt32(ctx, (int32_t)battery_percent());
}

// system.sleep(ms)  — enter deep sleep; ms is the max wakeup timeout (0 = indefinite)
// Wakeup is also triggered by the power button (GPIO3 LOW).
JSValue js_x4_system_sleep(JSContext *ctx, JSValue *this_val,
                            int argc, JSValue *argv) {
    int ms = 0;
    if (argc >= 1) JS_ToInt32(ctx, &ms, argv[0]);

    // Enable ext0 wakeup on power button (GPIO3, active LOW)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)3, 0);

    if (ms > 0) {
        esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000ULL);
    }

    Serial.println("[JS] entering deep sleep");
    Serial.flush();
    esp_deep_sleep_start();
    // never returns
    return JS_UNDEFINED;
}

// system.log(msg)  — print to Serial
JSValue js_x4_system_log(JSContext *ctx, JSValue *this_val,
                          int argc, JSValue *argv) {
    if (argc < 1) return JS_UNDEFINED;
    JSCStringBuf buf;
    const char *msg = JS_ToCString(ctx, argv[0], &buf);
    if (msg) Serial.println(msg);
    return JS_UNDEFINED;
}

// system.appName()  → string
JSValue js_x4_system_appName(JSContext *ctx, JSValue *this_val,
                              int argc, JSValue *argv) {
    return JS_NewString(ctx, s_app_name);
}

} // extern "C"
