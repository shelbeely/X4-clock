/*
 * js_reminder.cpp — reminder.* core firmware API for Xteink X4
 *
 * Reads /reminders/pending.json from SD at boot into a C-side static cache.
 * The cache persists across JS context reloads so clock faces and apps share
 * the same reminder data without hitting the SD card on every draw().
 *
 * Reminder JSON format:
 *   [{"id":1,"title":"…","time":1716001200,"body":"…","recurring":86400}, …]
 *
 *   id        — unique integer identifier
 *   title     — reminder name (max 64 chars)
 *   time      — Unix timestamp (seconds) when the reminder fires
 *   body      — optional detail text (max 128 chars)
 *   recurring — repeat interval in seconds; 0 = one-shot
 *
 * JS API (global object available in all contexts):
 *   reminder.count()                      → int   (total reminders in cache)
 *   reminder.get(idx)                     → {id,title,time,body,recurring} or null
 *   reminder.due()                        → int   (# reminders with time <= now)
 *   reminder.dismiss(id)                  → void  (one-shot: remove;
 *                                                   recurring: advance time
 *                                                   by recurring interval)
 *   reminder.add(title,time,body,recur)   → bool  (appends + saves to SD)
 *   reminder.remove(id)                   → bool  (removes by id + saves to SD)
 *   reminder.reload()                     → void  (re-reads from SD)
 *
 * "due" and "dismiss" require system.time() to be set; due() returns 0 if
 * the clock has not been synchronised.
 */

#include "js_reminder.h"
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

#define REM_MAX_ITEMS  16
#define REM_TITLE_MAX  64
#define REM_BODY_MAX   128
#define REM_FILE       "/reminders/pending.json"
#define REM_RAW_MAX    4096

// ---------------------------------------------------------------------------
// C-side cache
// ---------------------------------------------------------------------------

struct Reminder {
    int32_t  id;
    char     title[REM_TITLE_MAX];
    uint32_t time;       // Unix timestamp (seconds)
    char     body[REM_BODY_MAX];
    uint32_t recurring;  // repeat interval in seconds; 0 = one-shot
};

static Reminder s_items[REM_MAX_ITEMS];
static int      s_count   = 0;
static int32_t  s_next_id = 1;

// ---------------------------------------------------------------------------
// Minimal JSON helpers
// ---------------------------------------------------------------------------

static void rjson_str(const char *src, const char *key, char *out, int out_len) {
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

static bool rjson_uint(const char *src, const char *key, uint32_t *out) {
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

static void parse_reminders(const char *json) {
    s_count   = 0;
    s_next_id = 1;

    const char *p = strchr(json, '[');
    if (!p) return;
    p++;

    static char obj_buf[REM_TITLE_MAX + REM_BODY_MAX + 128];

    while (s_count < REM_MAX_ITEMS) {
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

        Reminder &r = s_items[s_count];
        uint32_t uid = 0;
        rjson_uint(obj_buf, "id", &uid);
        r.id = (int32_t)uid;
        rjson_str(obj_buf,  "title", r.title, REM_TITLE_MAX);
        rjson_uint(obj_buf, "time",  &r.time);
        rjson_str(obj_buf,  "body",  r.body,  REM_BODY_MAX);
        rjson_uint(obj_buf, "recurring", &r.recurring);

        if (r.id >= s_next_id) s_next_id = r.id + 1;
        s_count++;
        p = q + 1;
    }
}

// ---------------------------------------------------------------------------
// Serialise cache to SD
// ---------------------------------------------------------------------------

static bool save_reminders() {
    if (!sdcard_available()) return false;
    sdcard_wake();

    // REM_SAVE_BUFFER_SIZE = REM_MAX_ITEMS × (fields + JSON markup) + brackets
    // = 16 × (64 + 128 + 128 + 60) + 2 = 6082 → rounded up to 6144
    static const int REM_SAVE_BUFFER_SIZE = 6144;
    static char buf[REM_SAVE_BUFFER_SIZE];
    int pos = 0;
    buf[pos++] = '[';
    for (int i = 0; i < s_count; i++) {
        if (i > 0) buf[pos++] = ',';
        pos += snprintf(buf + pos, REM_SAVE_BUFFER_SIZE - pos,
                        "{\"id\":%d,\"title\":\"%s\","
                        "\"time\":%u,\"body\":\"%s\",\"recurring\":%u}",
                        (int)s_items[i].id, s_items[i].title,
                        (unsigned)s_items[i].time,
                        s_items[i].body,
                        (unsigned)s_items[i].recurring);
    }
    buf[pos++] = ']';
    buf[pos]   = '\0';

    int fh = sd_open(REM_FILE, 'w');
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

void reminder_init() {
    s_count   = 0;
    s_next_id = 1;
    if (!sdcard_available()) return;

    int32_t sz = sd_size(REM_FILE);
    if (sz <= 0) return;

    size_t fsz = (sz > REM_RAW_MAX) ? REM_RAW_MAX : (size_t)sz;
    static char raw[REM_RAW_MAX + 1];

    int fh = sd_open(REM_FILE, 'r');
    if (fh < 0) return;
    int n = sd_read(fh, raw, fsz);
    sd_close(fh);
    if (n <= 0) return;
    raw[n] = '\0';

    parse_reminders(raw);
    Serial.printf("[reminder] loaded %d item(s)\n", s_count);
}

// ---------------------------------------------------------------------------
// JS bindings
// ---------------------------------------------------------------------------

extern "C" {

// reminder.count() → int
JSValue js_x4_reminder_count(JSContext *ctx, JSValue *this_val,
                              int argc, JSValue *argv) {
    return JS_NewInt32(ctx, s_count);
}

// reminder.get(idx) → {id, title, time, body, recurring}  or  null
JSValue js_x4_reminder_get(JSContext *ctx, JSValue *this_val,
                            int argc, JSValue *argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "reminder.get(idx)");
    int idx = 0;
    if (JS_ToInt32(ctx, &idx, argv[0])) return JS_EXCEPTION;
    if (idx < 0 || idx >= s_count) return JS_NULL;

    const Reminder &r = s_items[idx];

    JSGCRef obj_ref;
    JSValue *obj_ptr = JS_PushGCRef(ctx, &obj_ref);
    *obj_ptr = JS_NewObject(ctx);

    JS_SetPropertyStr(ctx, *obj_ptr, "id",        JS_NewInt32(ctx, r.id));
    JS_SetPropertyStr(ctx, *obj_ptr, "title",     JS_NewString(ctx, r.title));
    JS_SetPropertyStr(ctx, *obj_ptr, "time",      JS_NewUint32(ctx, r.time));
    JS_SetPropertyStr(ctx, *obj_ptr, "body",      JS_NewString(ctx, r.body));
    JS_SetPropertyStr(ctx, *obj_ptr, "recurring", JS_NewUint32(ctx, r.recurring));

    JSValue result = *obj_ptr;
    JS_PopGCRef(ctx, &obj_ref);
    return result;
}

// reminder.due() → int
// Returns the count of reminders whose time <= current Unix time.
// Returns 0 if the system clock has not been synchronised.
JSValue js_x4_reminder_due(JSContext *ctx, JSValue *this_val,
                            int argc, JSValue *argv) {
    uint32_t now = js_system_time_sec();
    if (now == 0) return JS_NewInt32(ctx, 0);

    int count = 0;
    for (int i = 0; i < s_count; i++) {
        if (s_items[i].time <= now) count++;
    }
    return JS_NewInt32(ctx, count);
}

// reminder.dismiss(id) → void
// One-shot reminder: removed from cache and SD.
// Recurring reminder: time is advanced by recurring seconds and saved.
JSValue js_x4_reminder_dismiss(JSContext *ctx, JSValue *this_val,
                                int argc, JSValue *argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "reminder.dismiss(id)");
    int id = 0;
    if (JS_ToInt32(ctx, &id, argv[0])) return JS_EXCEPTION;

    for (int i = 0; i < s_count; i++) {
        if (s_items[i].id == id) {
            if (s_items[i].recurring > 0) {
                // Advance to next occurrence
                s_items[i].time += s_items[i].recurring;
            } else {
                // Remove one-shot reminder
                for (int j = i; j < s_count - 1; j++) s_items[j] = s_items[j + 1];
                s_count--;
            }
            save_reminders();
            return JS_UNDEFINED;
        }
    }
    return JS_UNDEFINED;  // id not found — no-op
}

// reminder.add(title, time, body, recurring) → bool
JSValue js_x4_reminder_add(JSContext *ctx, JSValue *this_val,
                            int argc, JSValue *argv) {
    if (argc < 2) return JS_ThrowTypeError(ctx,
                        "reminder.add(title, time[, body[, recurring]])");

    if (s_count >= REM_MAX_ITEMS) return JS_NewBool(0);

    JSCStringBuf tbuf;
    const char *title = JS_ToCString(ctx, argv[0], &tbuf);
    if (!title) return JS_EXCEPTION;

    double time_d = 0.0;
    if (JS_ToNumber(ctx, &time_d, argv[1])) return JS_EXCEPTION;

    const char *body = "";
    JSCStringBuf bbuf;
    if (argc >= 3) {
        body = JS_ToCString(ctx, argv[2], &bbuf);
        if (!body) return JS_EXCEPTION;
    }

    double rec_d = 0.0;
    if (argc >= 4) JS_ToNumber(ctx, &rec_d, argv[3]);

    Reminder &r  = s_items[s_count];
    r.id         = s_next_id++;
    r.time       = (uint32_t)time_d;
    r.recurring  = (uint32_t)rec_d;
    strncpy(r.title, title, REM_TITLE_MAX - 1);
    r.title[REM_TITLE_MAX - 1] = '\0';
    strncpy(r.body,  body,  REM_BODY_MAX - 1);
    r.body[REM_BODY_MAX - 1] = '\0';
    s_count++;

    bool ok = save_reminders();
    return JS_NewBool(ok ? 1 : 0);
}

// reminder.remove(id) → bool
JSValue js_x4_reminder_remove(JSContext *ctx, JSValue *this_val,
                               int argc, JSValue *argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "reminder.remove(id)");
    int id = 0;
    if (JS_ToInt32(ctx, &id, argv[0])) return JS_EXCEPTION;

    for (int i = 0; i < s_count; i++) {
        if (s_items[i].id == id) {
            for (int j = i; j < s_count - 1; j++) s_items[j] = s_items[j + 1];
            s_count--;
            bool ok = save_reminders();
            return JS_NewBool(ok ? 1 : 0);
        }
    }
    return JS_NewBool(0);
}

// reminder.reload() → void
JSValue js_x4_reminder_reload(JSContext *ctx, JSValue *this_val,
                               int argc, JSValue *argv) {
    reminder_init();
    return JS_UNDEFINED;
}

} // extern "C"
