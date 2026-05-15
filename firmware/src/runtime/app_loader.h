#pragma once
/*
 * app_loader.h — SD card app scanner and JS lifecycle manager
 *
 * Boot sequence:
 *   1. Show brief splash screen
 *   2. Scan /apps/ on the SD card for *.js and *.app files
 *   3. If exactly one app found (or default.txt names one) → auto-launch
 *   4. If multiple apps → show selection list (Left/Right to navigate, Confirm)
 *   5. If SD absent or /apps/ empty → launch built-in C fallback clock
 *
 * App lifecycle contract (JS side):
 *   function setup() {}       // called once
 *   function loop()  {}       // called repeatedly
 *   // input.onButton(fn) for button events
 */

#include <Arduino.h>

void app_loader_init();
void app_loader_run();   // blocks; returns only on fatal error
