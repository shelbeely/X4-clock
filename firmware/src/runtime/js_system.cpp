/*
 * js_system.cpp — system.* JavaScript bindings for Xteink X4
 *
 * Exposes: millis, battery%, deep-sleep, serial log, app name,
 *          batteryLow, setIdleTimeout.
 */

#include "js_system.h"
#include "drivers/battery.h"
#include "bsp/x4_pins.h"
#include "mquickjs.h"
#include <Arduino.h>
#include <esp_sleep.h>

// Current app filename — set by app_loader before launching each app
static char s_app_name[64] = "unknown";

// Idle-sleep timeout: overridable by JS via system.setIdleTimeout(ms).
// Initialised to the compile-time default from x4_pins.h.
static uint32_t s_idle_timeout_ms = IDLE_SLEEP_MS;

void js_system_set_app_name(const char *name) {
    strncpy(s_app_name, name ? name : "unknown", sizeof(s_app_name) - 1);
    s_app_name[sizeof(s_app_name) - 1] = '\0';
}

uint32_t js_system_idle_timeout_ms() {
    return s_idle_timeout_ms;
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

// system.batteryLow()  → bool
// Returns true when the battery is below BAT_LOW_PCT and the device is not
// charging.  Face scripts can use this to show a low-battery indicator.
JSValue js_x4_system_batteryLow(JSContext *ctx, JSValue *this_val,
                                 int argc, JSValue *argv) {
    bool low = (!battery_charging() && battery_percent() <= BAT_LOW_PCT);
    return JS_NewBool(low ? 1 : 0);
}

// system.sleep(ms)  — enter deep sleep; ms is the max wakeup timeout (0 = indefinite)
// Wakeup is also triggered by the power button (GPIO3 LOW).
JSValue js_x4_system_sleep(JSContext *ctx, JSValue *this_val,
                            int argc, JSValue *argv) {
    int ms = 0;
    if (argc >= 1) JS_ToInt32(ctx, &ms, argv[0]);

    // Enable ext0 wakeup on power button (GPIO3, active LOW)
    esp_deep_sleep_enable_gpio_wakeup(1ULL << 3, ESP_GPIO_WAKEUP_GPIO_LOW);

    if (ms > 0) {
        esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000ULL);
    }

    Serial.println("[JS] entering deep sleep");
    Serial.flush();
    esp_deep_sleep_start();
    // never returns
    return JS_UNDEFINED;
}

// system.setIdleTimeout(ms)  — set auto-sleep idle timeout (0 = disable)
JSValue js_x4_system_setIdleTimeout(JSContext *ctx, JSValue *this_val,
                                     int argc, JSValue *argv) {
    int ms = 0;
    if (argc >= 1) JS_ToInt32(ctx, &ms, argv[0]);
    s_idle_timeout_ms = (uint32_t)(ms > 0 ? ms : 0);
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
