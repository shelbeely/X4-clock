/*
 * js_weather.cpp — weather.* core firmware API for Xteink X4
 *
 * Maintains a C-side cache of OpenWeatherMap data.  The cache persists
 * across JS context reloads so clock faces can call weather.temp() etc.
 * without triggering a network request on every draw().
 *
 * Configuration is read from /config/settings.json at boot:
 *   {"owm_key":"…", "city":"London", "tz_offset":1, …}
 *
 * "tz_offset" (integer hours) is a boot-time fallback.  Once
 * weather.refresh() succeeds the OWM response provides a "timezone" field
 * (integer seconds) which supersedes tz_offset and is automatically applied
 * via configTime() so that NTP syncs with the correct local timezone.
 *
 * JS API (global object available in all contexts):
 *   weather.refresh()           → bool  (sync HTTP GET; requires WiFi; ~1–3 s)
 *   weather.valid()             → bool  (true if cache is populated)
 *   weather.temp()              → number  (°C)
 *   weather.humidity()          → int    (%)
 *   weather.condition()         → string  (e.g. "clear sky")
 *   weather.city()              → string  (city name from API response)
 *   weather.age()               → int    (ms since last refresh, or -1)
 *   weather.tz()                → int    (UTC offset seconds; 0 if unknown)
 *   weather.setLocation(city)   → bool   (update city + persist to SD)
 *   weather.location()          → string (currently configured city)
 */

#include "js_weather.h"
#include "drivers/sdcard.h"
#include "mquickjs.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define OWM_URL_FMT \
    "http://api.openweathermap.org/data/2.5/weather?q=%s&appid=%s&units=metric"
#define WEATHER_TIMEOUT_MS    10000
#define WEATHER_BODY_MAX      4096
#define WEATHER_SETTINGS_MAX  1024
#define SETTINGS_PATH         "/config/settings.json"

// ---------------------------------------------------------------------------
// C-side weather cache
// ---------------------------------------------------------------------------

static char    s_owm_key[64]     = {};
static char    s_owm_city[64]    = {};   // configured city (from settings.json or setLocation)
static float   s_temp            = 0.0f;
static int     s_humidity        = 0;
static char    s_condition[64]   = {};
static char    s_city_name[64]   = {};   // city name returned by OWM API
static bool    s_valid           = false;
static uint32_t s_last_ms        = 0;
// UTC offset in seconds.  Populated from:
//   1. /config/settings.json "tz_offset" (hours) at boot — coarse fallback
//   2. OWM "timezone" field (seconds) on each successful refresh — precise
static int32_t s_tz_offset_sec   = 0;

// ---------------------------------------------------------------------------
// Minimal JSON helpers (local to this translation unit)
// ---------------------------------------------------------------------------

static void wjson_str(const char *src, const char *key, char *out, int out_len) {
    char pat[80];
    snprintf(pat, sizeof(pat), "\"%s\":\"", key);
    const char *p = strstr(src, pat);
    if (!p) { out[0] = '\0'; return; }
    p += strlen(pat);
    int n = 0;
    while (*p && *p != '"' && n < out_len - 1) {
        if (*p == '\\' && *(p + 1)) { p++; }
        out[n++] = *p++;
    }
    out[n] = '\0';
}

// Finds "key":NUMBER (integer or float, may be negative) and returns the
// value as a double.  Returns false if the key is absent.
static bool wjson_num(const char *src, const char *key, double *out) {
    char pat[80];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    const char *p = strstr(src, pat);
    if (!p) return false;
    p += strlen(pat);
    while (*p == ' ') p++;
    if ((*p < '0' || *p > '9') && *p != '-') return false;
    char *ep = nullptr;
    *out = strtod(p, &ep);
    return (ep != p);
}

// ---------------------------------------------------------------------------
// Serialize updated settings back to SD
// (reads the existing file, patches city + tz_offset, writes back)
// ---------------------------------------------------------------------------

static bool save_settings_city(const char *city, int32_t tz_sec) {
    if (!sdcard_available()) return false;
    sdcard_wake();

    // Read existing settings
    static char existing[WEATHER_SETTINGS_MAX + 1];
    existing[0] = '\0';
    if (sd_exists(SETTINGS_PATH)) {
        int32_t sz = sd_size(SETTINGS_PATH);
        if (sz > 0 && sz <= WEATHER_SETTINGS_MAX) {
            int fh = sd_open(SETTINGS_PATH, 'r');
            if (fh >= 0) {
                int n = sd_read(fh, existing, (size_t)sz);
                sd_close(fh);
                if (n > 0) existing[n] = '\0';
                else       existing[0] = '\0';
            }
        }
    }

    // Extract other fields to preserve them
    char owm_key[64]  = {};
    double rotation   = 0.0;
    double refresh_ms = 20.0;

    wjson_str(existing, "owm_key", owm_key, sizeof(owm_key));
    wjson_num(existing, "rotation",   &rotation);
    wjson_num(existing, "refresh_ms", &refresh_ms);

    // If owm_key is still empty use the in-memory key
    if (owm_key[0] == '\0') {
        strncpy(owm_key, s_owm_key, sizeof(owm_key) - 1);
    }

    // tz_offset stored as integer hours (rounded from seconds)
    int tz_hours = (int)(tz_sec / 3600);

    static char buf[WEATHER_SETTINGS_MAX];
    int len = snprintf(buf, sizeof(buf),
        "{\"rotation\":%d,\"refresh_ms\":%d,"
        "\"tz_offset\":%d,"
        "\"owm_key\":\"%s\","
        "\"city\":\"%s\"}",
        (int)rotation, (int)refresh_ms,
        tz_hours, owm_key, city);

    bool ok = false;
    if (len > 0 && len < (int)sizeof(buf)) {
        int fh = sd_open(SETTINGS_PATH, 'w');
        if (fh >= 0) {
            sd_write(fh, buf, (size_t)len);
            sd_close(fh);
            ok = true;
        }
    }
    sdcard_sleep();
    return ok;
}

// ---------------------------------------------------------------------------
// Public C API
// ---------------------------------------------------------------------------

void weather_init() {
    s_valid           = false;
    s_owm_key[0]      = '\0';
    s_owm_city[0]     = '\0';
    s_tz_offset_sec   = 0;

    if (!sdcard_available()) return;
    if (!sd_exists(SETTINGS_PATH)) return;

    int32_t sz = sd_size(SETTINGS_PATH);
    if (sz <= 0 || sz > WEATHER_SETTINGS_MAX) return;

    static char buf[WEATHER_SETTINGS_MAX + 1];
    int fh = sd_open(SETTINGS_PATH, 'r');
    if (fh < 0) return;
    int n = sd_read(fh, buf, (size_t)sz);
    sd_close(fh);
    if (n <= 0) return;
    buf[n] = '\0';

    wjson_str(buf, "owm_key", s_owm_key,  sizeof(s_owm_key));
    wjson_str(buf, "city",    s_owm_city, sizeof(s_owm_city));

    // Seed timezone from settings.json "tz_offset" (hours) as a boot fallback
    double tz_h = 0.0;
    if (wjson_num(buf, "tz_offset", &tz_h)) {
        s_tz_offset_sec = (int32_t)((int)tz_h * 3600);
    }

    Serial.printf("[weather] config: city=%s key_len=%d tz_offset_h=%d\n",
                  s_owm_city, (int)strlen(s_owm_key), (int)(s_tz_offset_sec / 3600));
}

int32_t weather_tz_offset_sec() {
    return s_tz_offset_sec;
}

// ---------------------------------------------------------------------------
// JS bindings
// ---------------------------------------------------------------------------

extern "C" {

// weather.refresh() → bool
// Synchronous HTTP GET to OpenWeatherMap.  Caller must ensure WiFi is
// connected.  Blocks for up to WEATHER_TIMEOUT_MS.
// On success: updates the weather cache AND applies the OWM-provided timezone
// via configTime() so NTP will use the correct local offset.
JSValue js_x4_weather_refresh(JSContext *ctx, JSValue *this_val,
                               int argc, JSValue *argv) {
    if (s_owm_key[0] == '\0' || s_owm_city[0] == '\0') {
        Serial.println("[weather] refresh: no API key or city configured");
        return JS_NewBool(0);
    }

    char url[256];
    snprintf(url, sizeof(url), OWM_URL_FMT, s_owm_city, s_owm_key);

    HTTPClient http;
    http.setTimeout(WEATHER_TIMEOUT_MS);

    if (!http.begin(url)) {
        Serial.println("[weather] refresh: http.begin failed");
        return JS_NewBool(0);
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[weather] refresh: HTTP %d\n", code);
        http.end();
        return JS_NewBool(0);
    }

    static char body[WEATHER_BODY_MAX + 1];
    WiFiClient *stream = http.getStreamPtr();
    size_t total = 0;
    if (stream) {
        while (total < WEATHER_BODY_MAX && stream->available()) {
            int c = stream->read();
            if (c < 0) break;
            body[total++] = (char)c;
        }
    }
    body[total] = '\0';
    http.end();

    // Parse standard weather fields
    double d = 0.0;
    if (wjson_num(body, "temp",     &d)) s_temp     = (float)d;
    if (wjson_num(body, "humidity", &d)) s_humidity = (int)d;
    wjson_str(body, "description", s_condition, sizeof(s_condition));
    wjson_str(body, "name",        s_city_name, sizeof(s_city_name));

    // Parse "timezone": UTC offset in seconds (top-level field in OWM response)
    double tz_d = 0.0;
    if (wjson_num(body, "timezone", &tz_d)) {
        int32_t new_tz = (int32_t)tz_d;
        if (new_tz != s_tz_offset_sec) {
            s_tz_offset_sec = new_tz;
            Serial.printf("[weather] timezone updated: %d s (UTC%+.1f h)\n",
                          new_tz, (float)new_tz / 3600.0f);
        }
        // Re-apply NTP with the correct timezone so system.time() returns
        // local time after the next NTP sync.  Using pool.ntp.org and
        // time.nist.gov — reliable public servers suitable for global use.
        configTime((long)s_tz_offset_sec, 0, "pool.ntp.org", "time.nist.gov");
    }

    s_valid   = true;
    s_last_ms = millis();

    Serial.printf("[weather] refresh OK: %s %.1fC %d%% %s  tz=%ds\n",
                  s_city_name, s_temp, s_humidity, s_condition, s_tz_offset_sec);
    return JS_NewBool(1);
}

// weather.valid() → bool
JSValue js_x4_weather_valid(JSContext *ctx, JSValue *this_val,
                             int argc, JSValue *argv) {
    return JS_NewBool(s_valid ? 1 : 0);
}

// weather.temp() → float (°C)
JSValue js_x4_weather_temp(JSContext *ctx, JSValue *this_val,
                            int argc, JSValue *argv) {
    return JS_NewFloat64(ctx, (double)s_temp);
}

// weather.humidity() → int (%)
JSValue js_x4_weather_humidity(JSContext *ctx, JSValue *this_val,
                                int argc, JSValue *argv) {
    return JS_NewInt32(ctx, s_humidity);
}

// weather.condition() → string (e.g. "clear sky")
JSValue js_x4_weather_condition(JSContext *ctx, JSValue *this_val,
                                 int argc, JSValue *argv) {
    return JS_NewString(ctx, s_valid ? s_condition : "");
}

// weather.city() → string (city name from API response)
JSValue js_x4_weather_city(JSContext *ctx, JSValue *this_val,
                            int argc, JSValue *argv) {
    return JS_NewString(ctx, s_valid ? s_city_name : "");
}

// weather.age() → int (ms since last refresh, or -1 if never refreshed)
JSValue js_x4_weather_age(JSContext *ctx, JSValue *this_val,
                           int argc, JSValue *argv) {
    if (!s_valid) return JS_NewInt32(ctx, -1);
    return JS_NewInt32(ctx, (int32_t)(millis() - s_last_ms));
}

// weather.tz() → int (UTC offset in seconds)
// Returns the offset from the last OWM response, or the fallback value read
// from /config/settings.json at boot.  Returns 0 if neither is available.
JSValue js_x4_weather_tz(JSContext *ctx, JSValue *this_val,
                          int argc, JSValue *argv) {
    return JS_NewInt32(ctx, s_tz_offset_sec);
}

// weather.setLocation(city) → bool
// Updates the in-memory city name and writes the change to
// /config/settings.json so it persists across reboots.
// Does NOT fetch weather data — call weather.refresh() afterwards.
JSValue js_x4_weather_setLocation(JSContext *ctx, JSValue *this_val,
                                   int argc, JSValue *argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "weather.setLocation(city)");

    JSCStringBuf cbuf;
    const char *city = JS_ToCString(ctx, argv[0], &cbuf);
    if (!city || city[0] == '\0') return JS_NewBool(0);

    strncpy(s_owm_city, city, sizeof(s_owm_city) - 1);
    s_owm_city[sizeof(s_owm_city) - 1] = '\0';

    bool ok = save_settings_city(s_owm_city, s_tz_offset_sec);
    Serial.printf("[weather] setLocation: city=%s saved=%d\n", s_owm_city, (int)ok);
    return JS_NewBool(ok ? 1 : 0);
}

// weather.location() → string (currently configured city)
JSValue js_x4_weather_location(JSContext *ctx, JSValue *this_val,
                                int argc, JSValue *argv) {
    return JS_NewString(ctx, s_owm_city);
}

} // extern "C"
