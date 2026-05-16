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
#include "runtime/js_system.h"
#include "bsp/x4_pins.h"

#include <Arduino.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
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
// Graceful deep sleep helper — shows a message, persists it on e-ink, sleeps
// ---------------------------------------------------------------------------

static void graceful_deep_sleep(const char *line1, const char *line2) {
    unload_face();
    display_clear();
    if (line1) display_print(200, 200, line1, 3);
    if (line2) display_print(240, 290, line2, 2);
    display_refresh();   // full refresh so message persists on e-ink after power-off
    Serial.flush();
    esp_deep_sleep_enable_gpio_wakeup(1ULL << PIN_BTN_POWER, ESP_GPIO_WAKEUP_GPIO_LOW);
    esp_deep_sleep_start();
    /* never reached */
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

    // Put the SD card to sleep after face loading — it won't be needed again
    // until the user switches faces.
    if (sdcard_available()) sdcard_sleep();

    uint32_t last_draw_ms        = 0;
    bool     force_draw          = true;
    uint32_t last_interaction_ms = millis();  // for idle-sleep tracking

    // One-time light-sleep wakeup source: power button (GPIO3 LOW).
    // Timer wakeup is configured per-iteration below.
    gpio_wakeup_enable((gpio_num_t)PIN_BTN_POWER, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();

    for (;;) {
        uint32_t now = millis();

        // --- Trigger draw() every DRAW_INTERVAL_MS (or immediately when forced) ---
        if (force_draw || (now - last_draw_ms >= DRAW_INTERVAL_MS)) {
            force_draw   = false;
            last_draw_ms = now;

            if (s_faces[s_face_idx].is_builtin) {
                builtin_digital_draw();
            } else if (s_face_ctx) {
                js_engine_call_func(s_face_ctx, "draw");
            }

            // Hibernate display on battery to minimise idle current.
            // display_clear() / display_print() call display_wake() automatically
            // before the next draw cycle.
            if (!battery_charging()) {
                display_hibernate();
            }

            // Low-battery protective shutdown (check after each draw cycle)
            if (!battery_charging() && battery_percent() <= BAT_LOW_PCT) {
                graceful_deep_sleep("Low Battery", "Sleeping...");
                /* never reached */
            }
        }

        // --- Process one button event per iteration ---
        ButtonEvent ev = buttons_dequeue();

        switch (ev) {
            case BTN_LEFT:
                sdcard_wake();
                s_face_idx = (s_face_idx == 0) ? (s_face_count - 1)
                                               : (s_face_idx - 1);
                load_face(s_face_idx);
                show_face_name(s_faces[s_face_idx].name);
                if (sdcard_available()) sdcard_sleep();
                delay(700);
                force_draw   = true;
                last_draw_ms = 0;
                last_interaction_ms = millis();
                break;

            case BTN_RIGHT:
                sdcard_wake();
                s_face_idx = (s_face_idx + 1) % s_face_count;
                load_face(s_face_idx);
                show_face_name(s_faces[s_face_idx].name);
                if (sdcard_available()) sdcard_sleep();
                delay(700);
                force_draw   = true;
                last_draw_ms = 0;
                last_interaction_ms = millis();
                break;

            case BTN_CONFIRM: {
                char batStr[24];
                snprintf(batStr, sizeof(batStr), "Batt: %d%%", battery_percent());
                display_print(20, 450, batStr, 1);
                display_partial_refresh();
                if (!battery_charging()) display_hibernate();
                last_interaction_ms = millis();
                break;
            }

            case BTN_BACK:
                // Return to app_loader → picker
                unload_face();
                if (sdcard_available()) sdcard_wake();
                return;

            case BTN_POWER:
                graceful_deep_sleep("Sleeping...", nullptr);
                /* never reached */
                break;

            default:
                break;
        }

        // --- Auto-sleep on battery after inactivity ---
        uint32_t idle_ms = js_system_idle_timeout_ms();
        if (!battery_charging() && idle_ms > 0 &&
                (millis() - last_interaction_ms >= idle_ms)) {
            graceful_deep_sleep("Idle timeout", "Sleeping...");
            /* never reached */
        }

        // --- Sleep until next draw deadline ---
        bool on_battery = !battery_charging();
        if (on_battery && !force_draw) {
            uint32_t elapsed = millis() - last_draw_ms;
            if (elapsed < DRAW_INTERVAL_MS) {
                uint64_t sleep_us = (uint64_t)(DRAW_INTERVAL_MS - elapsed) * 1000ULL;
                esp_sleep_enable_timer_wakeup(sleep_us);
                esp_light_sleep_start();
            }
        } else {
            delay(50);
        }
    }
}
