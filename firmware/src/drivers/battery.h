#pragma once
/*
 * battery.h — Battery state driver for Xteink X4
 *
 * Reads the Li-ion cell voltage via the 2×10 kΩ voltage divider on GPIO0
 * and converts the result to an integer percentage (0–100).
 * The reading is cached for BAT_CACHE_MS milliseconds to avoid hammering
 * the ADC in a tight loop.
 */

#include <Arduino.h>

void battery_init();

// Returns cached percentage 0–100. Re-reads the ADC if the cache is stale.
uint8_t battery_percent();

// Returns true when USB VBUS is detected on PIN_USB_DETECT
bool battery_charging();
