/*
 * js_calendar.cpp — calendar.* core firmware API for Xteink X4
 *
 * Reads /calendar/events.json from SD at boot into a C-side static cache.
 * The cache persists across JS context reloads so clock faces and apps share
 * the same calendar data without hitting the SD card on every draw().
 *
 * Event JSON format:
 *   [{"id":1,"title":"…","start":1716000000,"end":1716003600,"desc":"…"}, …]
 *
 *   id    — unique integer identifier
 *   title — event name (max 64 chars)
 *   start — Unix timestamp (seconds) when the event begins
 *   end   — Unix timestamp (seconds) when it ends; 0 = no fixed end
 *   desc  — optional description (max 128 chars)
 *
 * JS API (global object available in all contexts):
 *   calendar.count()                       → int   (total events in cache)
 *   calendar.get(idx)                      → {id,title,start,end,desc} or null
 *   calendar.upcoming()                    → int   (# events with start >= now)
 *   calendar.add(title,start,end,desc)     → bool  (appends + saves to SD)
 *   calendar.remove(id)                    → bool  (removes by id + saves to SD)
 *   calendar.reload()                      → void  (re-reads from SD)
 *
 * "upcoming" requires system.time() to be set (via system.setTime or
 * system.syncTime); returns 0 if the clock hasn't been synchronised.
 */

#include "js_calendar.h"
#include "js_system.h"
#include "drivers/sdcard.h"
#include "mquickjs.h"
#include <Arduino.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define CAL_MAX_EVENTS  32
#define CAL_TITLE_MAX   64
#define CAL_DESC_MAX    128
#define CAL_FILE        "/calendar/events.json"
#define CAL_RAW_MAX     8192

// ---------------------------------------------------------------------------
// C-side cache
// ---------------------------------------------------------------------------

struct CalEvent {
    int32_t  id;
    char     title[CAL_TITLE_MAX];
    uint32_t start;
    uint32_t end;
    char     desc[CAL_DESC_MAX];
};

static CalEvent s_events[CAL_MAX_EVENTS];
static int      s_count   = 0;
static int32_t  s_next_id = 1;  // increments on every calendar.add()

// ---------------------------------------------------------------------------
// Minimal JSON helpers
// ---------------------------------------------------------------------------

static void cjson_str(const char *src, const char *key, char *out, int out_len) {
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

static bool cjson_uint(const char *src, const char *key, uint32_t *out) {
    char pat[80];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    const char *p = strstr(src, pat);
    if (!p) return false;
    p += strlen(pat);
    while (*p == ' ') p++;
    if (*p < '0' || *p > '9') return false;
    uint32_t v = 0;
    while (*p >= '0' && *p <= '9') { v = v * 10 + (uint32_t)(*p - '0'); p++; }
    *out = v;
    return true;
}

// ---------------------------------------------------------------------------
// Array parser
// ---------------------------------------------------------------------------

static void parse_events(const char *json) {
    s_count   = 0;
    s_next_id = 1;

    const char *p = strchr(json, '[');
    if (!p) return;
    p++;

    static char obj_buf[CAL_TITLE_MAX + CAL_DESC_MAX + 128];

    while (s_count < CAL_MAX_EVENTS) {
        p = strchr(p, '{');
        if (!p) break;

        const char *start = p;
        int depth = 0;
        const char *q = p;
        while (*q) {
            if (*q == '{') depth++;
            else if (*q == '}') { if (--depth == 0) break; }
            q++;
        }
        if (!*q) break;

        size_t obj_len = (size_t)(q - start + 1);
        if (obj_len >= sizeof(obj_buf)) { p = q + 1; continue; }

        strncpy(obj_buf, start, obj_len);
        obj_buf[obj_len] = '\0';

        CalEvent &ev = s_events[s_count];
        uint32_t uid = 0;
        cjson_uint(obj_buf, "id", &uid);
        ev.id = (int32_t)uid;
        cjson_str(obj_buf,  "title", ev.title, CAL_TITLE_MAX);
        cjson_uint(obj_buf, "start", &ev.start);
        cjson_uint(obj_buf, "end",   &ev.end);
        cjson_str(obj_buf,  "desc",  ev.desc,  CAL_DESC_MAX);

        if (ev.id >= s_next_id) s_next_id = ev.id + 1;
        s_count++;
        p = q + 1;
    }
}

// ---------------------------------------------------------------------------
// Serialise cache to SD
// ---------------------------------------------------------------------------

static bool save_events() {
    if (!sdcard_available()) return false;
    sdcard_wake();

    // Max bytes: 32 × (64+128+128+50) ≈ 11 840; round up to 12 KB
    static char buf[12288];
    int pos = 0;
    buf[pos++] = '[';
    for (int i = 0; i < s_count; i++) {
        if (i > 0) buf[pos++] = ',';
        pos += snprintf(buf + pos, (int)sizeof(buf) - pos,
                        "{\"id\":%d,\"title\":\"%s\","
                        "\"start\":%u,\"end\":%u,\"desc\":\"%s\"}",
                        (int)s_events[i].id, s_events[i].title,
                        (unsigned)s_events[i].start,
                        (unsigned)s_events[i].end,
                        s_events[i].desc);
    }
    buf[pos++] = ']';
    buf[pos]   = '\0';

    int fh = sd_open(CAL_FILE, 'w');
    bool ok = false;
    if (fh >= 0) {
        sd_write(fh, buf, (size_t)pos);
        sd_close(fh);
        ok = true;
    }
    sdcard_sleep();
    return ok;
}

// ---------------------------------------------------------------------------
// Public C API
// ---------------------------------------------------------------------------

void calendar_init() {
    s_count   = 0;
    s_next_id = 1;
    if (!sdcard_available()) return;

    int32_t sz = sd_size(CAL_FILE);
    if (sz <= 0) return;

    size_t fsz = (sz > CAL_RAW_MAX) ? CAL_RAW_MAX : (size_t)sz;
    static char raw[CAL_RAW_MAX + 1];

    int fh = sd_open(CAL_FILE, 'r');
    if (fh < 0) return;
    int n = sd_read(fh, raw, fsz);
    sd_close(fh);
    if (n <= 0) return;
    raw[n] = '\0';

    parse_events(raw);
    Serial.printf("[calendar] loaded %d event(s)\n", s_count);
}

// ---------------------------------------------------------------------------
// JS bindings
// ---------------------------------------------------------------------------

extern "C" {

// calendar.count() → int
JSValue js_x4_calendar_count(JSContext *ctx, JSValue *this_val,
                              int argc, JSValue *argv) {
    return JS_NewInt32(ctx, s_count);
}

// calendar.get(idx) → {id, title, start, end, desc}  or  null
JSValue js_x4_calendar_get(JSContext *ctx, JSValue *this_val,
                            int argc, JSValue *argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "calendar.get(idx)");
    int idx = 0;
    if (JS_ToInt32(ctx, &idx, argv[0])) return JS_EXCEPTION;
    if (idx < 0 || idx >= s_count) return JS_NULL;

    const CalEvent &ev = s_events[idx];

    JSGCRef obj_ref;
    JSValue *obj_ptr = JS_PushGCRef(ctx, &obj_ref);
    *obj_ptr = JS_NewObject(ctx);

    JS_SetPropertyStr(ctx, *obj_ptr, "id",    JS_NewInt32(ctx, ev.id));
    JS_SetPropertyStr(ctx, *obj_ptr, "title", JS_NewString(ctx, ev.title));
    JS_SetPropertyStr(ctx, *obj_ptr, "start", JS_NewUint32(ctx, ev.start));
    JS_SetPropertyStr(ctx, *obj_ptr, "end",   JS_NewUint32(ctx, ev.end));
    JS_SetPropertyStr(ctx, *obj_ptr, "desc",  JS_NewString(ctx, ev.desc));

    JSValue result = *obj_ptr;
    JS_PopGCRef(ctx, &obj_ref);
    return result;
}

// calendar.upcoming() → int
// Returns the count of events whose start timestamp >= current time.
// Returns 0 if the system clock has not been set (system.time() == 0).
JSValue js_x4_calendar_upcoming(JSContext *ctx, JSValue *this_val,
                                 int argc, JSValue *argv) {
    uint32_t now = js_system_time_sec();
    if (now == 0) return JS_NewInt32(ctx, 0);

    int count = 0;
    for (int i = 0; i < s_count; i++) {
        if (s_events[i].start >= now) count++;
    }
    return JS_NewInt32(ctx, count);
}

// calendar.add(title, start, end, desc) → bool
JSValue js_x4_calendar_add(JSContext *ctx, JSValue *this_val,
                            int argc, JSValue *argv) {
    if (argc < 2) return JS_ThrowTypeError(ctx,
                        "calendar.add(title, start[, end[, desc]])");

    if (s_count >= CAL_MAX_EVENTS) return JS_NewBool(0);

    JSCStringBuf tbuf;
    const char *title = JS_ToCString(ctx, argv[0], &tbuf);
    if (!title) return JS_EXCEPTION;

    double start_d = 0.0, end_d = 0.0;
    if (JS_ToNumber(ctx, &start_d, argv[1])) return JS_EXCEPTION;
    if (argc >= 3) JS_ToNumber(ctx, &end_d, argv[2]);

    const char *desc = "";
    JSCStringBuf dbuf;
    if (argc >= 4) {
        desc = JS_ToCString(ctx, argv[3], &dbuf);
        if (!desc) return JS_EXCEPTION;
    }

    CalEvent &ev = s_events[s_count];
    ev.id    = s_next_id++;
    ev.start = (uint32_t)start_d;
    ev.end   = (uint32_t)end_d;
    strncpy(ev.title, title, CAL_TITLE_MAX - 1);
    ev.title[CAL_TITLE_MAX - 1] = '\0';
    strncpy(ev.desc, desc, CAL_DESC_MAX - 1);
    ev.desc[CAL_DESC_MAX - 1] = '\0';
    s_count++;

    bool ok = save_events();
    return JS_NewBool(ok ? 1 : 0);
}

// calendar.remove(id) → bool
JSValue js_x4_calendar_remove(JSContext *ctx, JSValue *this_val,
                               int argc, JSValue *argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "calendar.remove(id)");

    int id = 0;
    if (JS_ToInt32(ctx, &id, argv[0])) return JS_EXCEPTION;

    for (int i = 0; i < s_count; i++) {
        if (s_events[i].id == id) {
            for (int j = i; j < s_count - 1; j++) s_events[j] = s_events[j + 1];
            s_count--;
            bool ok = save_events();
            return JS_NewBool(ok ? 1 : 0);
        }
    }
    return JS_NewBool(0);  // not found
}

// calendar.reload() → void
JSValue js_x4_calendar_reload(JSContext *ctx, JSValue *this_val,
                               int argc, JSValue *argv) {
    calendar_init();
    return JS_UNDEFINED;
}

} // extern "C"
