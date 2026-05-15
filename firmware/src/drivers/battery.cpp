/*
 * battery.cpp — Battery level driver for Xteink X4
 *
 * The cell voltage is divided by a 2×10 kΩ resistor ladder before reaching
 * GPIO0.  We read the ADC, undo the divide-by-two, then map the resulting
 * millivolt value onto the 0–100 % scale for a typical Li-ion cell.
 */

#include "battery.h"
#include "bsp/x4_pins.h"

static uint8_t  s_cached_pct = 0;
static uint32_t s_last_read  = 0;

void battery_init() {
    analogSetAttenuation(BAT_ADC_ATTEN);
    pinMode(PIN_BAT_ADC, INPUT);
    pinMode(PIN_USB_DETECT, INPUT);
    s_last_read = 0;   // force a fresh read on first call
}

uint8_t battery_percent() {
    uint32_t now = millis();
    if (now - s_last_read < BAT_CACHE_MS && s_last_read != 0) {
        return s_cached_pct;
    }

    // ESP32-C3 ADC resolution: 12-bit (0–4095), Vref ≈ 3.1 V at 11 dB
    // ADC counts → millivolts (approximate linear mapping)
    int raw = analogRead(PIN_BAT_ADC);
    uint32_t adc_mv = (uint32_t)raw * 3100 / 4095;

    // Undo the voltage divider: Vbat = 2 × Vadc
    uint32_t vbat_mv = adc_mv * 2;

    // Clamp and map to percentage
    if (vbat_mv >= BAT_FULL_MV) {
        s_cached_pct = 100;
    } else if (vbat_mv <= BAT_EMPTY_MV) {
        s_cached_pct = 0;
    } else {
        s_cached_pct = (uint8_t)(
            (vbat_mv - BAT_EMPTY_MV) * 100 / (BAT_FULL_MV - BAT_EMPTY_MV)
        );
    }

    s_last_read = now;
    return s_cached_pct;
}

bool battery_charging() {
    return digitalRead(PIN_USB_DETECT) == HIGH;
}
