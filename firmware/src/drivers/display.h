#pragma once
/*
 * display.h — E-paper display driver for Xteink X4
 *
 * Wraps GxEPD2 (SSD1677 GDEQ0426T82, 800×480 B&W) with page-mode rendering
 * so only ~4 KB of framebuffer is held in RAM at a time.
 *
 * All co-ordinate and size parameters use the physical display orientation:
 *   x: 0 (left) → 799 (right)
 *   y: 0 (top)  → 479 (bottom)
 */

#include <Arduino.h>
#include <SPI.h>

void display_init(SPIClass &spi);

// Screen operations
void display_clear();                                      // fill with white
void display_refresh();                                    // full 3.5 s refresh
void display_partial_refresh();                            // partial 0.42 s refresh

// Drawing (buffered — call display_refresh / display_partial_refresh to flush)
void display_print(int16_t x, int16_t y,
                   const char *text, uint8_t size_factor);
void display_draw_rect(int16_t x, int16_t y,
                       int16_t w, int16_t h, bool filled);
void display_draw_bitmap(int16_t x, int16_t y, const char *sd_path);

// Dimensions
int16_t display_width();
int16_t display_height();

// Power management
// display_hibernate() sends the SSD1677 into its lowest-power standby (few µA).
// display_wake() exits standby; must be called before any drawing operation
// after a hibernate.  display_clear() calls display_wake() automatically, so
// callers that always start with display_clear() do not need explicit wake calls.
void display_hibernate();
void display_wake();
