/*
 * buttons.cpp — ADC-ladder + power-button driver for Xteink X4
 *
 * A FreeRTOS task polls GPIO1 (4 nav buttons), GPIO2 (2 volume buttons) and
 * GPIO3 (power button) every BTN_POLL_MS ms and posts ButtonEvent codes to a
 * 16-entry queue.  Debounce: a button must stay pressed for BTN_DEBOUNCE_MS
 * consecutive polls before it fires, and must be released before it can fire
 * again.  A power-button hold ≥ BTN_POWER_LONG_MS sends BTN_POWER once,
 * allowing the app loader to trigger deep sleep.
 */

#include "buttons.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static QueueHandle_t s_btn_queue = nullptr;

// ---------------------------------------------------------------------------
// ADC row reading helpers
// ---------------------------------------------------------------------------

static ButtonEvent decode_row1(int adc) {
    if (adc >= BTN_RIGHT_MIN  && adc <= BTN_RIGHT_MAX)   return BTN_RIGHT;
    if (adc >= BTN_LEFT_MIN   && adc <= BTN_LEFT_MAX)    return BTN_LEFT;
    if (adc >= BTN_CONFIRM_MIN && adc <= BTN_CONFIRM_MAX) return BTN_CONFIRM;
    if (adc >= BTN_BACK_MIN   && adc <= BTN_BACK_MAX)    return BTN_BACK;
    return BTN_NONE;
}

static ButtonEvent decode_row2(int adc) {
    if (adc < BTN_ROW2_THRESHOLD)                        return BTN_NONE;
    if (adc <= BTN_VOLDOWN_MAX)                          return BTN_VOLDOWN;
    if (adc >= BTN_VOLUP_MIN && adc <= BTN_VOLUP_MAX)   return BTN_VOLUP;
    return BTN_NONE;
}

// ---------------------------------------------------------------------------
// Poll task
// ---------------------------------------------------------------------------

static void button_task(void *param) {
    // Debounce state per logical button
    ButtonEvent last_event = BTN_NONE;
    uint32_t    press_start = 0;
    bool        fired       = false;
    bool        power_fired = false;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(BTN_POLL_MS));

        // --- Row 1 (ADC GPIO1) ---
        int adc1 = analogRead(PIN_BTN_ROW1);
        ButtonEvent ev1 = decode_row1(adc1);

        // --- Row 2 (ADC GPIO2) ---
        int adc2 = analogRead(PIN_BTN_ROW2);
        ButtonEvent ev2 = decode_row2(adc2);

        // --- Power button (GPIO3 digital, active LOW) ---
        bool pwr_pressed = (digitalRead(PIN_BTN_POWER) == LOW);

        // Merge: nav/vol buttons take priority; power button is checked separately
        ButtonEvent current = (ev1 != BTN_NONE) ? ev1 : ev2;

        // ---- Debounce + fire for nav/vol ----
        if (current != BTN_NONE) {
            if (current != last_event) {
                last_event  = current;
                press_start = millis();
                fired       = false;
            } else if (!fired && (millis() - press_start >= BTN_DEBOUNCE_MS)) {
                fired = true;
                xQueueSend(s_btn_queue, &current, 0);
            }
        } else {
            last_event = BTN_NONE;
            fired      = false;
        }

        // ---- Power button ----
        static uint32_t pwr_start = 0;
        if (pwr_pressed) {
            if (pwr_start == 0) pwr_start = millis();
            if (!power_fired && (millis() - pwr_start >= BTN_POWER_LONG_MS)) {
                power_fired = true;
                ButtonEvent pwr = BTN_POWER;
                xQueueSend(s_btn_queue, &pwr, 0);
            }
        } else {
            pwr_start   = 0;
            power_fired = false;
        }
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void buttons_init() {
    analogSetAttenuation(ADC_11db);
    pinMode(PIN_BTN_ROW1, INPUT);
    pinMode(PIN_BTN_ROW2, INPUT);
    pinMode(PIN_BTN_POWER, INPUT_PULLUP);

    s_btn_queue = xQueueCreate(BTN_QUEUE_DEPTH, sizeof(ButtonEvent));

    xTaskCreate(button_task, "buttons", 2048, nullptr, 2, nullptr);
}

ButtonEvent buttons_dequeue() {
    ButtonEvent ev = BTN_NONE;
    if (s_btn_queue) xQueueReceive(s_btn_queue, &ev, 0);
    return ev;
}

bool buttons_available() {
    return s_btn_queue && uxQueueMessagesWaiting(s_btn_queue) > 0;
}
