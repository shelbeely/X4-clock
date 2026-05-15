#pragma once
/*
 * clock_app.h — Built-in clock application for Xteink X4
 *
 * Displays the current time (relative to boot) with a swappable face.
 * One built-in C++ face ("Digital") is always available.  Additional faces
 * can be placed at /faces/<name>.js on the SD card.
 *
 * Button map while the clock is running:
 *   LEFT / RIGHT  — cycle through faces
 *   CONFIRM       — show battery percentage overlay
 *   BACK          — return to the app launcher (picker)
 *   POWER         — enter deep sleep
 *
 * This function returns only when BTN_BACK is pressed.
 * On BTN_POWER it calls esp_deep_sleep_start() and never returns.
 *
 * JS face contract (place at /faces/<name>.js):
 *   function setup() { … }   // called once after load (optional)
 *   function draw()  { … }   // called every second; use system.millis()
 *                            // for timing; call display.partialRefresh()
 *                            // inside draw() when you update the display.
 *                            // Do NOT register input.onButton() — the clock
 *                            // app owns all button handling.
 */

void clock_app_run();
