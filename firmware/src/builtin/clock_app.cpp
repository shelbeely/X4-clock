/*
 * clock_app.cpp — Built-in clock application for Xteink X4
 *
 * Manages face selection and the per-second draw loop.
 * See clock_app.h for the full contract.
 */

#include "clock_app.h"
#include "drivers/display.h"
#include "drivers/buttons.h"
#include "drivers/battery.h"
#include "drivers/sdcard.h"
#include "runtime/js_engine.h"
#include "runtime/js_input.h"
#include "bsp/x4_pins.h"

#include <Arduino.h>
#include <esp_sleep.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define FACES_DIR         "/faces"
#define MAX_FACES         9       // 1 built-in + up to 8 JS faces from SD
#define DRAW_INTERVAL_MS  1000U   // call draw() every second

// ---------------------------------------------------------------------------
// Face registry
// ---------------------------------------------------------------------------

struct FaceEntry {
    char name[64];    // display name shown during face switch
    char path[300];   // SD path (e.g. "/faces/digital.js"), or "" for built-in
    bool is_builtin;
};

static FaceEntry  s_faces[MAX_FACES];
static int        s_face_count = 0;
static int        s_face_idx   = 0;

// ---------------------------------------------------------------------------
// Active JS face context (nullptr when the built-in C++ face is active)
// ---------------------------------------------------------------------------

static JSContext *s_face_ctx = nullptr;

// ---------------------------------------------------------------------------
// Built-in digital face
// ---------------------------------------------------------------------------

// Last-drawn minute; -1 forces an immediate redraw.
// Module-level so it can be reset externally (face switch).
static int s_builtin_last_minute = -1;

static void builtin_digital_draw() {
    uint32_t ms      = millis();
    int totalSec     = (int)(ms / 1000);
    int h            = (totalSec / 3600) % 24;
    int m            = (totalSec / 60)   % 60;
    int day          = totalSec / 86400;
    int bat          = battery_percent();

    if (m == s_builtin_last_minute) return;
    s_builtin_last_minute = m;

    char timeStr[8];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", h, m);

    char dayStr[20];
    snprintf(dayStr, sizeof(dayStr), "Day %d", day);

    char batStr[20];
    snprintf(batStr, sizeof(batStr), "Batt: %d%%", bat);

    display_clear();
    display_print(120, 180, timeStr, 4);
    display_print(300, 320, dayStr,  2);
    display_print(20,  450, batStr,  1);
    display_partial_refresh();
}

// ---------------------------------------------------------------------------
// Face loading / unloading
// ---------------------------------------------------------------------------

static void unload_face() {
    if (s_face_ctx) {
        js_engine_destroy_context(s_face_ctx);
        s_face_ctx = nullptr;
    }
}

// Load a JS face file into a new context.  Returns true on success.
// On failure, caller should fall back to the built-in face.
static bool load_js_face(int idx) {
    int32_t sz = sd_size(s_faces[idx].path);
    if (sz <= 0) {
        Serial.printf("[clock] face not found: %s\n", s_faces[idx].path);
        return false;
    }

    uint8_t *buf = (uint8_t *)malloc((size_t)sz + 1);
    if (!buf) {
        Serial.println("[clock] load_js_face: malloc failed");
        return false;
    }

    int fh = sd_open(s_faces[idx].path, 'r');
    if (fh < 0) { free(buf); return false; }

    size_t total = 0;
    while (total < (size_t)sz) {
        size_t bytes_to_read = ((size_t)SD_CHUNK_SIZE < (size_t)(sz - (int32_t)total))
                               ? (size_t)SD_CHUNK_SIZE
                               : (size_t)(sz - (int32_t)total);
        int n = sd_read(fh, (char *)(buf + total), bytes_to_read);
        if (n <= 0) break;
        total += (size_t)n;
    }
    sd_close(fh);
    buf[total] = '\0';

    JSContext *ctx = js_engine_new_context();
    if (!ctx) { free(buf); return false; }

    bool ok = js_engine_run_source(ctx, (const char *)buf, total, s_faces[idx].name);
    free(buf);   // source fully consumed by JS_Eval

    if (!ok) {
        js_engine_destroy_context(ctx);
        Serial.printf("[clock] face eval failed: %s\n", s_faces[idx].name);
        return false;
    }

    js_engine_call_func(ctx, "setup");
    s_face_ctx = ctx;
    return true;
}

// Switch to face at index idx.  Always succeeds: falls back to built-in on error.
static void load_face(int idx) {
    unload_face();

    if (s_faces[idx].is_builtin) {
        s_builtin_last_minute = -1;   // force immediate redraw
        return;
    }

    if (!load_js_face(idx)) {
        Serial.println("[clock] falling back to built-in face");
        s_face_idx            = 0;
        s_builtin_last_minute = -1;
    }
}

// ---------------------------------------------------------------------------
// Face scanner
// ---------------------------------------------------------------------------

static void scan_faces() {
    // Slot 0 is always the built-in digital face
    strncpy(s_faces[0].name, "Digital (built-in)", sizeof(s_faces[0].name) - 1);
    s_faces[0].name[sizeof(s_faces[0].name) - 1] = '\0';
    s_faces[0].path[0]    = '\0';
    s_faces[0].is_builtin = true;
    s_face_count = 1;

    if (!sdcard_available()) return;

    static char     names[MAX_FACES - 1][256];
    static bool     is_dirs[MAX_FACES - 1];
    static uint32_t szs[MAX_FACES - 1];

    int n = sd_list_dir(FACES_DIR, names, is_dirs, szs, MAX_FACES - 1);
    for (int i = 0; i < n && s_face_count < MAX_FACES; i++) {
        if (is_dirs[i]) continue;

        const char *nm  = names[i];
        size_t      len = strlen(nm);
        if (len <= 3 || strcmp(nm + len - 3, ".js") != 0) continue;

        // Display name: strip the ".js" extension
        int namelen = (int)len - 3;
        if (namelen >= (int)sizeof(s_faces[0].name))
            namelen = (int)sizeof(s_faces[0].name) - 1;

        strncpy(s_faces[s_face_count].name, nm, (size_t)namelen);
        s_faces[s_face_count].name[namelen] = '\0';
        s_faces[s_face_count].is_builtin    = false;

        snprintf(s_faces[s_face_count].path, sizeof(s_faces[0].path),
                 "%s/%s", FACES_DIR, nm);

        s_face_count++;
    }

    Serial.printf("[clock] %d face(s) found\n", s_face_count);
}

// ---------------------------------------------------------------------------
// Face name overlay — briefly shown when switching faces
// ---------------------------------------------------------------------------

static void show_face_name(const char *name) {
    display_clear();
    display_print(20, 200, name, 3);
    display_partial_refresh();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void clock_app_run() {
    // Reset module state
    s_face_count          = 0;
    s_face_idx            = 0;
    s_face_ctx            = nullptr;
    s_builtin_last_minute = -1;

    scan_faces();
    load_face(s_face_idx);

    uint32_t last_draw_ms = 0;
    bool     force_draw   = true;

    for (;;) {
        uint32_t now = millis();

        // Trigger draw() every DRAW_INTERVAL_MS (or immediately when forced)
        if (force_draw || (now - last_draw_ms >= DRAW_INTERVAL_MS)) {
            force_draw   = false;
            last_draw_ms = now;

            if (s_faces[s_face_idx].is_builtin) {
                builtin_digital_draw();
            } else if (s_face_ctx) {
                js_engine_call_func(s_face_ctx, "draw");
            }
        }

        // Process one button event per loop iteration
        ButtonEvent ev = buttons_dequeue();

        switch (ev) {
            case BTN_LEFT:
                s_face_idx = (s_face_idx == 0) ? (s_face_count - 1)
                                               : (s_face_idx - 1);
                load_face(s_face_idx);
                show_face_name(s_faces[s_face_idx].name);
                delay(700);
                force_draw   = true;
                last_draw_ms = 0;
                break;

            case BTN_RIGHT:
                s_face_idx = (s_face_idx + 1) % s_face_count;
                load_face(s_face_idx);
                show_face_name(s_faces[s_face_idx].name);
                delay(700);
                force_draw   = true;
                last_draw_ms = 0;
                break;

            case BTN_CONFIRM: {
                char batStr[24];
                snprintf(batStr, sizeof(batStr), "Batt: %d%%", battery_percent());
                display_print(20, 450, batStr, 1);
                display_partial_refresh();
                break;
            }

            case BTN_BACK:
                // Return to app_loader → picker
                unload_face();
                return;

            case BTN_POWER:
                unload_face();
                display_clear();
                display_print(280, 220, "Sleeping...", 2);
                display_partial_refresh();
                delay(500);
                esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BTN_POWER, 0);
                esp_deep_sleep_start();
                /* never reached */
                break;

            default:
                break;
        }

        delay(50);
    }
}
