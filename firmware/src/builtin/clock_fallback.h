#pragma once
/*
 * clock_fallback.h — Pure C++ fallback clock for Xteink X4
 *
 * Shown when the SD card is absent or /apps/ contains no runnable apps.
 * Displays time-since-boot in HH:MM format and shows a hint to insert an
 * SD card.  The power button long-press enters deep sleep as normal.
 *
 * This is a blocking function; it never returns unless the power button is
 * held long enough to trigger deep sleep, or until the device is reset.
 */

#include "drivers/display.h"
#include "drivers/buttons.h"
#include "drivers/battery.h"
#include <Arduino.h>
#include <esp_sleep.h>
#include "bsp/x4_pins.h"

static inline void clock_fallback_run() {
    // Helper: zero-pad integer to two digits
    auto pad2 = [](char *out, int n) {
        out[0] = '0' + (n / 10);
        out[1] = '0' + (n % 10);
        out[2] = '\0';
    };

    int lastMinute = -1;

    display_clear();
    display_print(20, 440, "Insert SD card with /apps/ to load apps.", 1);
    display_refresh();

    for (;;) {
        uint32_t ms       = millis();
        int totalSeconds  = (int)(ms / 1000);
        int hours         = (totalSeconds / 3600) % 24;
        int minutes       = (totalSeconds / 60)   % 60;
        int seconds       = totalSeconds % 60;

        if (minutes != lastMinute) {
            lastMinute = minutes;

            char hstr[4], mstr[4];
            pad2(hstr, hours);
            pad2(mstr, minutes);

            char timeStr[8];
            snprintf(timeStr, sizeof(timeStr), "%s:%s", hstr, mstr);

            char batStr[24];
            snprintf(batStr, sizeof(batStr), "Batt: %d%%", battery_percent());

            display_clear();
            display_print(120, 170, timeStr, 4);      // large centred time
            display_print(300, 300, batStr,  1);
            display_print(20,  440,
                          "Insert SD card with /apps/ to load apps.", 1);
            display_partial_refresh();
        }

        // Check buttons
        ButtonEvent ev = buttons_dequeue();

        if (ev == BTN_CONFIRM) {
            // Show battery on confirm button
            char batStr[24];
            snprintf(batStr, sizeof(batStr), "Batt: %d%%", battery_percent());
            display_print(300, 300, batStr, 1);
            display_partial_refresh();
        } else if (ev == BTN_POWER) {
            // Long-press detected by buttons.cpp — enter deep sleep
            display_clear();
            display_print(280, 220, "Sleeping...", 2);
            display_partial_refresh();
            delay(500);

            esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_BTN_POWER, 0);
            esp_deep_sleep_start();
        }

        delay(200);
    }
}
