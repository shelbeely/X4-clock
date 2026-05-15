/*
 * app_loader.cpp — SD card app scanner and JS lifecycle manager
 */

#include "app_loader.h"
#include "js_engine.h"
#include "js_input.h"
#include "js_system.h"
#include "drivers/display.h"
#include "drivers/buttons.h"
#include "drivers/sdcard.h"
#include "builtin/clock_app.h"

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define APPS_DIR          "/apps"
#define DEFAULT_FILE      "/apps/default.txt"
#define MAX_APPS          16
#define WATCHDOG_MS       5000   // reset app if loop() blocks for >5 s

// ---------------------------------------------------------------------------
// App list
// ---------------------------------------------------------------------------

static char  s_app_names[MAX_APPS][256];
static char  s_app_paths[MAX_APPS][300];
static bool  s_app_is_bytecode[MAX_APPS];
static int   s_app_count  = 0;
static int   s_app_cursor = 0;   // selection index in the picker UI

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------

static void show_splash();
static void show_error(const char *msg);
static void scan_apps();
static int  find_default_app();
static void show_picker();
static void launch_app(int idx);
static bool load_file_to_buf(const char *path, uint8_t **out_buf, size_t *out_len);

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void app_loader_init() {
    s_app_count  = 0;
    s_app_cursor = 0;
}

void app_loader_run() {
    show_splash();

    // Slot 0 is always the built-in clock app.  An empty path signals
    // launch_app() to call clock_app_run() instead of loading a file.
    strncpy(s_app_names[0], "Clock (built-in)", 255);
    s_app_names[0][255]   = '\0';
    s_app_paths[0][0]     = '\0';
    s_app_is_bytecode[0]  = false;
    s_app_count           = 1;

    if (sdcard_available()) {
        scan_apps();   // appends SD apps starting at s_app_names[s_app_count]
    }

    // Default to the built-in clock (idx 0) unless default.txt says otherwise
    int idx = find_default_app();
    if (idx < 0) idx = 0;

    // Keep launching apps until a fatal error
    for (;;) {
        launch_app(idx);
        // If launch_app returns and there are multiple choices, show the picker
        if (s_app_count > 1) {
            show_picker();
            idx = s_app_cursor;
        }
        // With only the built-in clock available the loop just relaunches it
    }
}

// ---------------------------------------------------------------------------
// Splash screen
// ---------------------------------------------------------------------------

static void show_splash() {
    display_clear();
    display_print(200, 180, "Xteink X4",     3);
    display_print(280, 260, "Base Firmware", 2);
    display_print(300, 320, "v1.0",          1);
    display_refresh();
    delay(1500);
}

// ---------------------------------------------------------------------------
// Error screen
// ---------------------------------------------------------------------------

static void show_error(const char *msg) {
    display_clear();
    display_print(20, 40,  "JS Error", 2);
    display_print(20, 100, msg,        1);
    display_print(20, 420, "Press any button to continue.", 1);
    display_partial_refresh();

    // Wait for any button
    while (!buttons_available()) delay(50);
    buttons_dequeue();
}

// ---------------------------------------------------------------------------
// App scanning
// ---------------------------------------------------------------------------

static void scan_apps() {
    static char  names[MAX_APPS][256];
    static bool  is_dirs[MAX_APPS];
    static uint32_t szs[MAX_APPS];

    int n = sd_list_dir(APPS_DIR, names, is_dirs, szs, MAX_APPS - s_app_count);
    // s_app_count is NOT reset here — we append to existing entries

    for (int i = 0; i < n && s_app_count < MAX_APPS; i++) {
        if (is_dirs[i]) continue;

        const char *nm = names[i];
        size_t len = strlen(nm);

        bool is_js  = (len > 3  && strcmp(nm + len - 3, ".js")  == 0);
        bool is_app = (len > 4  && strcmp(nm + len - 4, ".app") == 0);

        if (!is_js && !is_app) continue;

        strncpy(s_app_names[s_app_count], nm, 255);
        s_app_names[s_app_count][255] = '\0';

        snprintf(s_app_paths[s_app_count], sizeof(s_app_paths[0]),
                 "%s/%s", APPS_DIR, nm);

        s_app_is_bytecode[s_app_count] = is_app;
        s_app_count++;
    }
}

// ---------------------------------------------------------------------------
// Default app resolution
// ---------------------------------------------------------------------------

static int find_default_app() {
    // If there is exactly one app, use it
    if (s_app_count == 1) return 0;

    // Check for a default.txt file
    if (!sd_exists(DEFAULT_FILE)) return -1;

    int fh = sd_open(DEFAULT_FILE, 'r');
    if (fh < 0) return -1;

    char buf[260] = {};
    int n = sd_read(fh, buf, sizeof(buf) - 1);
    sd_close(fh);
    if (n <= 0) return -1;

    // Strip trailing whitespace / newline
    for (int i = n - 1; i >= 0; i--) {
        if (buf[i] == '\n' || buf[i] == '\r' || buf[i] == ' ')
            buf[i] = '\0';
        else
            break;
    }

    for (int i = 0; i < s_app_count; i++) {
        if (strcmp(s_app_names[i], buf) == 0) return i;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// App picker UI
// ---------------------------------------------------------------------------

static void draw_picker() {
    display_clear();
    display_print(20, 30, "Select App", 2);

    for (int i = 0; i < s_app_count; i++) {
        int y = 100 + i * 30;
        if (i == s_app_cursor) {
            display_draw_rect(10, y - 2, EPD_WIDTH - 20, 26, true);
            // Print in white by using inverted rect — workaround: just indent
            char label[270];
            snprintf(label, sizeof(label), "> %s", s_app_names[i]);
            display_print(20, y, label, 1);
        } else {
            display_print(30, y, s_app_names[i], 1);
        }
    }
    display_print(20, 440, "Left/Right: scroll   Confirm: launch", 1);
    display_partial_refresh();
}

static void show_picker() {
    draw_picker();

    for (;;) {
        ButtonEvent ev = buttons_dequeue();
        if (ev == BTN_NONE) { delay(20); continue; }

        if (ev == BTN_RIGHT) {
            if (s_app_cursor < s_app_count - 1) s_app_cursor++;
            draw_picker();
        } else if (ev == BTN_LEFT) {
            if (s_app_cursor > 0) s_app_cursor--;
            draw_picker();
        } else if (ev == BTN_CONFIRM) {
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// File loading
// ---------------------------------------------------------------------------

static bool load_file_to_buf(const char *path, uint8_t **out_buf, size_t *out_len) {
    int32_t sz = sd_size(path);
    if (sz <= 0) return false;

    // Allocate on C heap (not JS heap) — freed after bytecode is loaded
    uint8_t *buf = (uint8_t *)malloc((size_t)sz + 1);
    if (!buf) return false;

    int fh = sd_open(path, 'r');
    if (fh < 0) { free(buf); return false; }

    size_t total = 0;
    while (total < (size_t)sz) {
        int n = sd_read(fh, (char *)(buf + total),
                        (size_t)(SD_CHUNK_SIZE) < (size_t)(sz - total)
                            ? (size_t)(SD_CHUNK_SIZE)
                            : (size_t)(sz - total));
        if (n <= 0) break;
        total += n;
    }
    sd_close(fh);

    buf[total] = '\0';  // null-terminate for JS source
    *out_buf = buf;
    *out_len = total;
    return true;
}

// ---------------------------------------------------------------------------
// App launch
// ---------------------------------------------------------------------------

static void launch_app(int idx) {
    // Empty path → built-in clock app (no file to load)
    if (s_app_paths[idx][0] == '\0') {
        clock_app_run();
        return;
    }

    const char *path = s_app_paths[idx];
    const char *name = s_app_names[idx];

    Serial.printf("[loader] launching %s\n", path);
    js_system_set_app_name(name);

    uint8_t *buf = nullptr;
    size_t   len = 0;

    if (!load_file_to_buf(path, &buf, &len)) {
        show_error("Failed to read app file");
        return;
    }

    JSContext *ctx = js_engine_new_context();
    if (!ctx) {
        free(buf);
        show_error("JS context creation failed");
        return;
    }

    bool ok;
    if (s_app_is_bytecode[idx]) {
        // .app — mquickjs 32-bit bytecode (compiled with: mqjs -m32 -o app.app app.js)
        // IMPORTANT: buf must remain valid until the context is destroyed because
        // JS_LoadBytecode stores a direct pointer into the buffer.
        ok = js_engine_run_bytecode(ctx, buf, len);
        // buf intentionally NOT freed here — see free() after js_engine_destroy_context
    } else {
        // .js — JavaScript source; buf is fully consumed by JS_Eval and can be freed now
        ok = js_engine_run_source(ctx, (const char *)buf, len, name);
        free(buf);
        buf = nullptr;
    }

    if (!ok) {
        js_engine_destroy_context(ctx);
        if (buf) { free(buf); }
        show_error("App failed to load");
        return;
    }

    // Call setup()
    js_engine_call_func(ctx, "setup");

    // Main loop
    uint32_t last_loop_start = millis();
    for (;;) {
        // Dispatch button events to the registered input.onButton() callback.
        // Power button is dispatched here like any other button — if the JS
        // callback calls system.sleep(), esp_deep_sleep_start() is invoked
        // and this loop never resumes.  If no callback is registered the
        // event is consumed and we fall through to the legacy power check.
        js_input_dispatch_events(ctx);

        // Call loop()
        uint32_t now = millis();
        last_loop_start = now;
        js_engine_call_func(ctx, "loop");

        // Watchdog: if loop() took too long something is wrong
        uint32_t elapsed = millis() - last_loop_start;
        if (elapsed > WATCHDOG_MS) {
            Serial.printf("[loader] watchdog: loop() took %u ms\n", elapsed);
            show_error("App watchdog: loop() too slow");
            break;
        }

        // Check for power-button long-press when no JS callback consumed it.
        // If the JS app registered input.onButton() and called system.sleep(),
        // esp_deep_sleep_start() already fired above.  This is a fallback for
        // apps that don't handle the power button in JS.
        if (buttons_available()) {
            ButtonEvent ev = buttons_dequeue();
            if (ev == BTN_POWER) break;  // exit to launcher / fallback
            // Put it back? No — just drop it (power = exit)
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }

    js_engine_destroy_context(ctx);
    // Free bytecode buffer now that the context (and all references into buf) is gone.
    // For source apps buf was already freed to nullptr after JS_Eval.
    if (buf) { free(buf); }
}
