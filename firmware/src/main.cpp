/*
 * main.cpp — Arduino entry point for Xteink X4 base firmware
 *
 * Boot sequence:
 *   1. Serial + USB CDC
 *   2. Shared SPI bus (EPD + SD)
 *   3. Display, buttons, battery, SD card (with 3× retry)
 *   4. JS engine subsystem
 *   5. App loader (blocks in app_loader_run())
 */

#include <Arduino.h>
#include <SPI.h>

#include "bsp/x4_pins.h"
#include "drivers/display.h"
#include "drivers/buttons.h"
#include "drivers/battery.h"
#include "drivers/sdcard.h"
#include "runtime/js_engine.h"
#include "runtime/app_loader.h"

// Shared SPI bus — both the EPD and SD card attach to this instance
static SPIClass g_spi(FSPI);

void setup() {
    // --- CPU clock: 80 MHz is ample for SPI + JS and halves active current ---
    setCpuFrequencyMhz(80);

    // --- Serial / USB CDC ---
    Serial.begin(115200);
    delay(500);   // brief wait for USB CDC to enumerate
    Serial.println("\n=== Xteink X4 Base Firmware v1.0 ===");

    // --- Shared SPI ---
    g_spi.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, -1);

    // --- Hardware drivers ---
    battery_init();
    buttons_init();
    display_init(g_spi);

#ifdef POWER_SAVE_USB
    // If running on battery with no USB host, disable CDC to save power.
    // battery_init() must have run before this check.
    if (!battery_charging()) {
        Serial.println("[boot] on battery — disabling USB CDC to save power");
        Serial.flush();
        Serial.end();
    }
#endif

    Serial.print("[boot] SD card ... ");
    if (sdcard_init(g_spi)) {
        Serial.println("OK");
    } else {
        Serial.println("not found (will use fallback clock)");
    }

    Serial.printf("[boot] battery %d%%\n", battery_percent());

    // --- JS engine ---
    js_engine_init();

    // --- App loader (does not return under normal operation) ---
    app_loader_init();
}

void loop() {
    // The app_loader_run() call is made here so FreeRTOS has finished
    // starting before we enter the blocking app loop.
    app_loader_run();

    // If app_loader_run() ever returns (should not happen), restart
    Serial.println("[main] app_loader_run returned — rebooting");
    delay(1000);
    ESP.restart();
}
