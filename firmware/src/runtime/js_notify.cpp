/*
 * js_notify.cpp — notify.* core firmware API for Xteink X4
 *
 * Reads /notifications/pending.json from SD at boot into a C-side static
 * cache.  The cache persists across JS context reloads, so clock faces and
 * apps can access notification data without touching the SD card on every
 * draw().
 *
 * JSON format:
 *   [{"title":"…","time":"09:00","body":"…"}, …]
 *
 * JS API (global object available in all contexts):
 *   notify.count()        → int
 *   notify.get(idx)       → {title, time, body}  or  null if out of range
 *   notify.dismiss(idx)   → void  (removes item, persists updated JSON to SD)
 *   notify.reload()       → void  (re-reads file from SD)
 */

#include "js_notify.h"
#include "drivers/sdcard.h"
#include "mquickjs.h"
#include <Arduino.h>
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define NOTIFY_MAX_ITEMS  16
#define NOTIFY_TITLE_MAX  64
#define NOTIFY_BODY_MAX   128
#define NOTIFY_TIME_MAX   32
#define NOTIFY_FILE       "/notifications/pending.json"
#define NOTIFY_RAW_MAX    8192

// ---------------------------------------------------------------------------
// C-side cache
// ---------------------------------------------------------------------------

struct NotifyItem {
    char title[NOTIFY_TITLE_MAX];
    char body[NOTIFY_BODY_MAX];
    char time_str[NOTIFY_TIME_MAX];
};

static NotifyItem s_items[NOTIFY_MAX_ITEMS];
static int        s_count = 0;

// ---------------------------------------------------------------------------
// Minimal JSON string-field extractor
// Finds "key":"VALUE" and copies VALUE into out (NUL-terminated).
// Handles a single level of backslash escape for \" and \\.
// ---------------------------------------------------------------------------

static void json_str(const char *src, const char *key, char *out, int out_len) {
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

// ---------------------------------------------------------------------------
// Parse a JSON array of notification objects
// ---------------------------------------------------------------------------

static void parse_items(const char *json) {
    s_count = 0;
    const char *p = strchr(json, '[');
    if (!p) return;
    p++;

    // Temporary object buffer — large enough for one item
    static char obj_buf[NOTIFY_TITLE_MAX + NOTIFY_BODY_MAX + NOTIFY_TIME_MAX + 64];

    while (s_count < NOTIFY_MAX_ITEMS) {
        p = strchr(p, '{');
        if (!p) break;

        // Find matching closing brace (track nesting depth)
        const char *start = p;
        int depth = 0;
        const char *q = p;
        while (*q) {
            if (*q == '{') depth++;
            else if (*q == '}') { if (--depth == 0) break; }
            q++;
        }
        if (!*q) break;  // malformed JSON

        size_t obj_len = (size_t)(q - start + 1);
        if (obj_len >= sizeof(obj_buf)) { p = q + 1; continue; }

        strncpy(obj_buf, start, obj_len);
        obj_buf[obj_len] = '\0';

        json_str(obj_buf, "title", s_items[s_count].title, NOTIFY_TITLE_MAX);
        json_str(obj_buf, "body",  s_items[s_count].body,  NOTIFY_BODY_MAX);
        json_str(obj_buf, "time",  s_items[s_count].time_str, NOTIFY_TIME_MAX);

        s_count++;
        p = q + 1;
    }
}

// ---------------------------------------------------------------------------
// Serialize the current cache back to /notifications/pending.json on SD
// ---------------------------------------------------------------------------

static bool save_items() {
    if (!sdcard_available()) return false;
    sdcard_wake();

    // Maximum serialized size: NOTIFY_MAX_ITEMS × (field max sizes + JSON markup)
    static char buf[NOTIFY_MAX_ITEMS * (NOTIFY_TITLE_MAX + NOTIFY_BODY_MAX + NOTIFY_TIME_MAX + 50) + 8];

    int pos = 0;
    buf[pos++] = '[';
    for (int i = 0; i < s_count; i++) {
        if (i > 0) buf[pos++] = ',';
        pos += snprintf(buf + pos, (int)sizeof(buf) - pos,
                        "{\"title\":\"%s\",\"time\":\"%s\",\"body\":\"%s\"}",
                        s_items[i].title, s_items[i].time_str, s_items[i].body);
    }
    buf[pos++] = ']';
    buf[pos]   = '\0';

    int fh = sd_open(NOTIFY_FILE, 'w');
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

void notify_init() {
    s_count = 0;
    if (!sdcard_available()) return;

    int32_t sz = sd_size(NOTIFY_FILE);
    if (sz <= 0) return;

    size_t fsz = (sz > NOTIFY_RAW_MAX) ? NOTIFY_RAW_MAX : (size_t)sz;
    static char raw[NOTIFY_RAW_MAX + 1];

    int fh = sd_open(NOTIFY_FILE, 'r');
    if (fh < 0) return;
    int n = sd_read(fh, raw, fsz);
    sd_close(fh);
    if (n <= 0) return;
    raw[n] = '\0';

    parse_items(raw);
    Serial.printf("[notify] loaded %d item(s)\n", s_count);
}

// ---------------------------------------------------------------------------
// JS bindings
// ---------------------------------------------------------------------------

extern "C" {

// notify.count() → int
JSValue js_x4_notify_count(JSContext *ctx, JSValue *this_val,
                            int argc, JSValue *argv) {
    return JS_NewInt32(ctx, s_count);
}

// notify.get(idx) → {title, time, body}  or  null
JSValue js_x4_notify_get(JSContext *ctx, JSValue *this_val,
                          int argc, JSValue *argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "notify.get(idx)");

    int idx = 0;
    if (JS_ToInt32(ctx, &idx, argv[0])) return JS_EXCEPTION;
    if (idx < 0 || idx >= s_count) return JS_NULL;

    JSGCRef obj_ref;
    JSValue *obj_ptr = JS_PushGCRef(ctx, &obj_ref);
    *obj_ptr = JS_NewObject(ctx);

    JS_SetPropertyStr(ctx, *obj_ptr, "title", JS_NewString(ctx, s_items[idx].title));
    JS_SetPropertyStr(ctx, *obj_ptr, "time",  JS_NewString(ctx, s_items[idx].time_str));
    JS_SetPropertyStr(ctx, *obj_ptr, "body",  JS_NewString(ctx, s_items[idx].body));

    JSValue result = *obj_ptr;
    JS_PopGCRef(ctx, &obj_ref);
    return result;
}

// notify.dismiss(idx) → void
// Removes the notification at idx and persists the updated list to SD.
JSValue js_x4_notify_dismiss(JSContext *ctx, JSValue *this_val,
                              int argc, JSValue *argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "notify.dismiss(idx)");

    int idx = 0;
    if (JS_ToInt32(ctx, &idx, argv[0])) return JS_EXCEPTION;
    if (idx < 0 || idx >= s_count) return JS_UNDEFINED;

    // Shift items down to fill the gap
    for (int i = idx; i < s_count - 1; i++) {
        s_items[i] = s_items[i + 1];
    }
    s_count--;
    save_items();
    return JS_UNDEFINED;
}

// notify.reload() → void
// Re-reads /notifications/pending.json from SD into the C-side cache.
JSValue js_x4_notify_reload(JSContext *ctx, JSValue *this_val,
                             int argc, JSValue *argv) {
    notify_init();
    return JS_UNDEFINED;
}

} // extern "C"
