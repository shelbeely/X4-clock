/*
 * js_system.cpp — system.* JavaScript bindings for Xteink X4
 *
 * Exposes: millis, battery%, deep-sleep, light-sleep, serial log, app name,
 *          batteryLow, setIdleTimeout, setRefreshInterval.
 */

#include "js_system.h"
#include "drivers/battery.h"
#include "drivers/display.h"
#include "bsp/x4_pins.h"
#include "mquickjs.h"
#include <Arduino.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <time.h>
#include <sys/time.h>

// Current app filename — set by app_loader before launching each app
static char s_app_name[64] = "unknown";

// Idle-sleep timeout: overridable by JS via system.setIdleTimeout(ms).
// Initialised to the compile-time default from x4_pins.h.
static uint32_t s_idle_timeout_ms = IDLE_SLEEP_MS;

// Loop sleep interval: overridable by JS via system.setRefreshInterval(ms).
// Initialised to the compile-time default from x4_pins.h.
static uint32_t s_loop_sleep_ms = LOOP_SLEEP_MS;

void js_system_set_app_name(const char *name) {
    strncpy(s_app_name, name ? name : "unknown", sizeof(s_app_name) - 1);
    s_app_name[sizeof(s_app_name) - 1] = '\0';
}

uint32_t js_system_idle_timeout_ms() {
    return s_idle_timeout_ms;
}

uint32_t js_system_loop_sleep_ms() {
    return s_loop_sleep_ms;
}

uint32_t js_system_time_sec() {
    time_t t = time(nullptr);
    // Return 0 if the clock has never been set (epoch < year 2001 threshold)
    return (t > 1000000000L) ? (uint32_t)t : 0;
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
// Hibernates the display first to minimise power draw, then enters deep sleep.
// Wakeup is also triggered by the power button (GPIO3 LOW).
JSValue js_x4_system_sleep(JSContext *ctx, JSValue *this_val,
                            int argc, JSValue *argv) {
    int ms = 0;
    if (argc >= 1) JS_ToInt32(ctx, &ms, argv[0]);

    // Hibernate display before sleeping to minimise power consumption
    display_hibernate();

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

// system.lightSleep(ms)  — enter light sleep for up to ms milliseconds
// Unlike deep sleep, light sleep preserves RAM and peripheral state.
// Wakeup sources: timer (ms > 0), power button (GPIO3 LOW).
// Returns actual sleep duration in milliseconds.
JSValue js_x4_system_lightSleep(JSContext *ctx, JSValue *this_val,
                                 int argc, JSValue *argv) {
    int ms = 0;
    if (argc >= 1) JS_ToInt32(ctx, &ms, argv[0]);

    // Enable power button wakeup
    gpio_wakeup_enable((gpio_num_t)PIN_BTN_POWER, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();

    if (ms > 0) {
        esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000ULL);
    }

    uint32_t before = millis();
    esp_light_sleep_start();
    uint32_t elapsed = millis() - before;

    return JS_NewInt32(ctx, (int32_t)elapsed);
}

// system.setIdleTimeout(ms)  — set auto-sleep idle timeout (0 = disable)
JSValue js_x4_system_setIdleTimeout(JSContext *ctx, JSValue *this_val,
                                     int argc, JSValue *argv) {
    int ms = 0;
    if (argc >= 1) JS_ToInt32(ctx, &ms, argv[0]);
    s_idle_timeout_ms = (uint32_t)(ms > 0 ? ms : 0);
    return JS_UNDEFINED;
}

// system.setRefreshInterval(ms)  — set loop sleep interval (default 20 ms)
// Lower values → more responsive but higher CPU usage / battery drain.
// Higher values → lower power but less frequent loop() calls.
JSValue js_x4_system_setRefreshInterval(JSContext *ctx, JSValue *this_val,
                                         int argc, JSValue *argv) {
    int ms = 0;
    if (argc >= 1) JS_ToInt32(ctx, &ms, argv[0]);
    if (ms < 1)  ms = 1;
    if (ms > 60000) ms = 60000;   // cap at 60 s
    s_loop_sleep_ms = (uint32_t)ms;
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

// system.time()  → int (Unix timestamp, seconds since 1970-01-01 UTC)
// Returns 0 if the clock has not been set via system.setTime() or
// system.syncTime().
JSValue js_x4_system_time(JSContext *ctx, JSValue *this_val,
                           int argc, JSValue *argv) {
    return JS_NewUint32(ctx, js_system_time_sec());
}

// system.setTime(unix_ts)  — manually set the real-time clock
// unix_ts: seconds since 1970-01-01 UTC (e.g. obtained from a time server)
JSValue js_x4_system_setTime(JSContext *ctx, JSValue *this_val,
                              int argc, JSValue *argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "system.setTime(unix_ts)");
    double ts = 0.0;
    if (JS_ToNumber(ctx, &ts, argv[0])) return JS_EXCEPTION;
    struct timeval tv = { (time_t)(uint32_t)ts, 0 };
    settimeofday(&tv, nullptr);
    return JS_UNDEFINED;
}

// system.syncTime([tz_offset_hours])  → bool
// Synchronise the RTC via NTP.  Requires WiFi to be connected.
// tz_offset_hours: integer UTC offset (e.g. -5 for EST, 2 for CEST).
// Blocks for up to 10 seconds waiting for NTP response.
// Returns true on success.
JSValue js_x4_system_syncTime(JSContext *ctx, JSValue *this_val,
                               int argc, JSValue *argv) {
    int tz = 0;
    if (argc >= 1) JS_ToInt32(ctx, &tz, argv[0]);

    long tz_offset_sec = (long)tz * 3600L;
    configTime(tz_offset_sec, 0, "pool.ntp.org", "time.nist.gov");

    uint32_t start = millis();
    while (time(nullptr) < 1000000000L && millis() - start < 10000) {
        delay(100);
    }

    bool ok = (time(nullptr) >= 1000000000L);
    Serial.printf("[system] syncTime: %s (ts=%u)\n",
                  ok ? "OK" : "failed", (unsigned)time(nullptr));
    return JS_NewBool(ok ? 1 : 0);
}

} // extern "C"
