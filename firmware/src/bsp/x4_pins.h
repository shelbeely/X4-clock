#pragma once
/*
 * Xteink X4 — Board Support Package pin definitions
 *
 * All GPIO numbers, ADC thresholds, and timing constants for the X4 hardware.
 * This is the single source of truth — every driver includes this header.
 *
 * Hardware:  ESP32-C3 (RISC-V 32-bit, 160 MHz, 16 MB flash, ~380 KB usable heap)
 * Display:   4.26" SSD1677, 800×480 B&W e-ink, SPI shared with SD card
 * Buttons:   7 total — ADC ladder on GPIO1 (4 nav) and GPIO2 (2 vol) + GPIO3 (power)
 * Battery:   650 mAh Li-ion, read via 2×10 kΩ voltage-divider on GPIO0
 */

// ---------------------------------------------------------------------------
// Shared SPI bus (EPD + SD card share SCK / MOSI / MISO)
// ---------------------------------------------------------------------------
#define PIN_SPI_SCK     8
#define PIN_SPI_MISO    7
#define PIN_SPI_MOSI    10

// ---------------------------------------------------------------------------
// E-paper display (SSD1677 GDEQ0426T82, 800×480)
// ---------------------------------------------------------------------------
#define PIN_EPD_CS      21
#define PIN_EPD_DC      4
#define PIN_EPD_RST     5
#define PIN_EPD_BUSY    6

#define EPD_WIDTH       800
#define EPD_HEIGHT      480
// Page-mode row count: 800 px / 8 bits × 40 rows = 4 000 bytes ≈ 4 KB per page
#define EPD_PAGE_ROWS   40
#define EPD_SPI_MHZ     20

// ---------------------------------------------------------------------------
// microSD card
// ---------------------------------------------------------------------------
#define PIN_SD_CS       12
#define SD_SPI_MHZ      20

// ---------------------------------------------------------------------------
// Battery ADC
// ---------------------------------------------------------------------------
#define PIN_BAT_ADC     0   // GPIO0: Vbat/2  (2×10 kΩ voltage divider)
// ADC full-scale on ESP32-C3 at 11 dB attenuation ≈ 3100 mV
// Divider factor = 2  →  Vbat_mv = adc_mv * 2
// Li-ion: 4200 mV = 100 %, 3300 mV = 0 %
#define BAT_ADC_ATTEN   ADC_11db
#define BAT_FULL_MV     4200
#define BAT_EMPTY_MV    3300
#define BAT_CACHE_MS    30000   // re-read every 30 s

// Protective deep-sleep threshold: enter sleep when battery ≤ this % and
// not charging.  Prevents over-discharge of the Li-ion cell.
#define BAT_LOW_PCT     5

// ---------------------------------------------------------------------------
// USB / charging detection
// ---------------------------------------------------------------------------
#define PIN_USB_DETECT  20  // HIGH when USB connected / charging

// Define POWER_SAVE_USB to disable the USB CDC Serial port when running on
// battery (no USB host detected 500 ms after boot).  Comment out to keep
// Serial always active (useful during development).
#define POWER_SAVE_USB  1

// ---------------------------------------------------------------------------
// Button — Row 1 (GPIO1 ADC): Right | Left | Confirm | Back
// ---------------------------------------------------------------------------
#define PIN_BTN_ROW1    1

// ADC midpoint values (12-bit, 0–4095)
#define BTN_RIGHT_VAL    3
#define BTN_LEFT_VAL     1470
#define BTN_CONFIRM_VAL  2655
#define BTN_BACK_VAL     3470

// Threshold half-width: an ADC reading is matched to the nearest value if it
// falls within ±BTN_HALF_WINDOW counts of that value's midpoint.
#define BTN_HALF_WINDOW  300

#define BTN_RIGHT_MIN    0
#define BTN_RIGHT_MAX    (BTN_RIGHT_VAL   + BTN_HALF_WINDOW)
#define BTN_LEFT_MIN     (BTN_LEFT_VAL    - BTN_HALF_WINDOW)
#define BTN_LEFT_MAX     (BTN_LEFT_VAL    + BTN_HALF_WINDOW)
#define BTN_CONFIRM_MIN  (BTN_CONFIRM_VAL - BTN_HALF_WINDOW)
#define BTN_CONFIRM_MAX  (BTN_CONFIRM_VAL + BTN_HALF_WINDOW)
#define BTN_BACK_MIN     (BTN_BACK_VAL    - BTN_HALF_WINDOW)
#define BTN_BACK_MAX     (BTN_BACK_VAL    + BTN_HALF_WINDOW)

// ---------------------------------------------------------------------------
// Button — Row 2 (GPIO2 ADC): VolDown | VolUp
// ---------------------------------------------------------------------------
#define PIN_BTN_ROW2    2

#define BTN_VOLDOWN_VAL  3
#define BTN_VOLUP_VAL    2205

#define BTN_VOLDOWN_MAX  (BTN_VOLDOWN_VAL + BTN_HALF_WINDOW)
#define BTN_VOLUP_MIN    (BTN_VOLUP_VAL   - BTN_HALF_WINDOW)
#define BTN_VOLUP_MAX    (BTN_VOLUP_VAL   + BTN_HALF_WINDOW)

// Minimum ADC reading to consider any Row-2 button pressed
#define BTN_ROW2_THRESHOLD 50

// ---------------------------------------------------------------------------
// Power button (GPIO3 digital)
// ---------------------------------------------------------------------------
#define PIN_BTN_POWER   3   // INPUT_PULLUP; LOW = pressed

#define BTN_DEBOUNCE_MS       50    // minimum press duration
#define BTN_POWER_LONG_MS     1000  // long-press threshold → deep sleep

// ---------------------------------------------------------------------------
// Battery-powered inactivity auto-sleep
// After this many ms without a button press, the device enters deep sleep when
// running on battery.  Set to 0 to disable.  Can be overridden at runtime from
// JS with system.setIdleTimeout(ms).
// ---------------------------------------------------------------------------
#define IDLE_SLEEP_MS   600000U  // 10 minutes

// ---------------------------------------------------------------------------
// Deep sleep wakeup (power button = GPIO3, active LOW)
// ---------------------------------------------------------------------------
#define WAKEUP_GPIO_MASK    (1ULL << PIN_BTN_POWER)

// ---------------------------------------------------------------------------
// Button event codes (posted to FreeRTOS queue as uint8_t)
// ---------------------------------------------------------------------------
typedef enum : uint8_t {
    BTN_NONE    = 0,
    BTN_RIGHT   = 1,
    BTN_LEFT    = 2,
    BTN_CONFIRM = 3,
    BTN_BACK    = 4,
    BTN_VOLUP   = 5,
    BTN_VOLDOWN = 6,
    BTN_POWER   = 7,
} ButtonEvent;

// Human-readable names for JS ("right", "left", …)
static const char * const BTN_NAMES[] = {
    "none", "right", "left", "confirm", "back", "volup", "voldown", "power"
};
