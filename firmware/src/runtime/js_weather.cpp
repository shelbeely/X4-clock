/*
 * js_weather.cpp — weather.* core firmware API for Xteink X4
 *
 * Maintains a C-side cache of OpenWeatherMap data.  The cache persists
 * across JS context reloads so clock faces can call weather.temp() etc.
 * without triggering a network request on every draw().
 *
 * Configuration is read from /config/settings.json at boot:
 *   {"owm_key":"…", "city":"London", …}
 *
 * JS API (global object available in all contexts):
 *   weather.refresh()    → bool  (sync HTTP GET; requires WiFi; ~1–3 s)
 *   weather.valid()      → bool  (true if cache is populated)
 *   weather.temp()       → number  (°C, one decimal place)
 *   weather.humidity()   → int    (%)
 *   weather.condition()  → string  (e.g. "clear sky")
 *   weather.city()       → string  (city name from API response)
 *   weather.age()        → int    (ms since last refresh, or -1 if never)
 */

#include "js_weather.h"
#include "drivers/sdcard.h"
#include "mquickjs.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define OWM_URL_FMT \
    "http://api.openweathermap.org/data/2.5/weather?q=%s&appid=%s&units=metric"
#define WEATHER_TIMEOUT_MS  10000
#define WEATHER_BODY_MAX    4096
#define WEATHER_SETTINGS_MAX  1024
#define SETTINGS_PATH       "/config/settings.json"

// ---------------------------------------------------------------------------
// C-side weather cache
// ---------------------------------------------------------------------------

static char    s_owm_key[64]   = {};
static char    s_owm_city[64]  = {};
static float   s_temp          = 0.0f;
static int     s_humidity      = 0;
static char    s_condition[64] = {};
static char    s_city_name[64] = {};
static bool    s_valid         = false;
static uint32_t s_last_ms      = 0;

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

// Finds "key":NUMBER and returns the double value; returns false if not found.
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
// Public C API
// ---------------------------------------------------------------------------

void weather_init() {
    s_valid       = false;
    s_owm_key[0]  = '\0';
    s_owm_city[0] = '\0';

    if (!sdcard_available()) return;
    if (!sd_exists(SETTINGS_PATH)) return;

    int32_t sz = sd_size(SETTINGS_PATH);
    if (sz <= 0 || sz > WEATHER_SETTINGS_MAX) return;

    static char buf[1025];
    int fh = sd_open(SETTINGS_PATH, 'r');
    if (fh < 0) return;
    int n = sd_read(fh, buf, (size_t)sz);
    sd_close(fh);
    if (n <= 0) return;
    buf[n] = '\0';

    wjson_str(buf, "owm_key", s_owm_key,  sizeof(s_owm_key));
    wjson_str(buf, "city",    s_owm_city, sizeof(s_owm_city));

    Serial.printf("[weather] config: city=%s key_len=%d\n",
                  s_owm_city, (int)strlen(s_owm_key));
}

// ---------------------------------------------------------------------------
// JS bindings
// ---------------------------------------------------------------------------

extern "C" {

// weather.refresh() → bool
// Synchronous HTTP GET to OpenWeatherMap.  Caller must ensure WiFi is
// connected.  Blocks for up to WEATHER_TIMEOUT_MS.
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

    // Parse temperature (first "temp": in the response, inside "main":{…})
    double d = 0.0;
    if (wjson_num(body, "temp", &d))    s_temp     = (float)d;
    if (wjson_num(body, "humidity", &d)) s_humidity = (int)d;
    wjson_str(body, "description", s_condition, sizeof(s_condition));
    wjson_str(body, "name",        s_city_name, sizeof(s_city_name));

    s_valid   = true;
    s_last_ms = millis();

    Serial.printf("[weather] refresh OK: %s %.1f°C %d%% %s\n",
                  s_city_name, s_temp, s_humidity, s_condition);
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

} // extern "C"
