/*
 * display.cpp — GxEPD2 driver wrapper for Xteink X4
 *
 * Uses GxEPD2_BW in page-mode (EPD_PAGE_ROWS rows per page = ~4 KB buffer)
 * so the full 48 KB framebuffer is never held in RAM simultaneously.
 *
 * SPI bus is shared with the SD card; GxEPD2 manages its own CS line.
 */

#include "display.h"
#include "bsp/x4_pins.h"
#include "sdcard.h"

#include <GxEPD2_BW.h>
#include <GxEPD2_426_GDEQ0426T82.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

// ---------------------------------------------------------------------------
// Page-mode display instance
// EPD_PAGE_ROWS controls the RAM buffer:  800/8 * 40 = 4 000 bytes
// ---------------------------------------------------------------------------
static GxEPD2_BW<GxEPD2_426_GDEQ0426T82, EPD_PAGE_ROWS> epd(
    GxEPD2_426_GDEQ0426T82(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY)
);

// Tracks whether any drawing commands have been issued since the last refresh
static bool _dirty = false;

// ---------------------------------------------------------------------------
// Internal: choose font based on caller's size_factor (1–4)
// ---------------------------------------------------------------------------
static void set_font(uint8_t size_factor) {
    switch (size_factor) {
        case 4:  epd.setFont(&FreeSansBold24pt7b); break;
        case 3:  epd.setFont(&FreeSansBold18pt7b); break;
        case 2:  epd.setFont(&FreeSansBold9pt7b);  break;
        default: epd.setFont(nullptr); break;   // built-in 5×7 bitmap font
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void display_init(SPIClass &spi) {
    epd.epd2.selectSPI(spi, SPISettings(EPD_SPI_MHZ * 1000000UL, MSBFIRST, SPI_MODE0));
    epd.init(115200, true, 2, false);
    epd.setRotation(0);         // portrait: (0,0) = top-left
    epd.setTextColor(GxEPD_BLACK);
    epd.setTextWrap(false);
    _dirty = false;
}

void display_clear() {
    epd.setFullWindow();
    epd.firstPage();
    do {
        epd.fillScreen(GxEPD_WHITE);
    } while (epd.nextPage());
    _dirty = false;
}

void display_refresh() {
    // Force a full update so partial ghosts are cleared periodically
    epd.display();
    _dirty = false;
}

void display_partial_refresh() {
    epd.displayWindow(0, 0, EPD_WIDTH, EPD_HEIGHT, true);
    _dirty = false;
}

void display_print(int16_t x, int16_t y, const char *text, uint8_t size_factor) {
    if (!text) return;
    set_font(size_factor);

    epd.setPartialWindow(0, 0, EPD_WIDTH, EPD_HEIGHT);
    epd.firstPage();
    do {
        epd.setCursor(x, y);
        epd.print(text);
    } while (epd.nextPage());

    // Reset to default font
    epd.setFont(nullptr);
    _dirty = true;
}

void display_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, bool filled) {
    epd.setPartialWindow(x, y, w, h);
    epd.firstPage();
    do {
        if (filled) {
            epd.fillRect(x, y, w, h, GxEPD_BLACK);
        } else {
            epd.drawRect(x, y, w, h, GxEPD_BLACK);
        }
    } while (epd.nextPage());
    _dirty = true;
}

/*
 * display_draw_bitmap — Draw a 1-bit Windows BMP file from the SD card.
 *
 * Reads the BMP header to extract width/height then streams pixel rows into
 * the display page buffer.  Reads in SD_CHUNK_SIZE byte chunks to stay within
 * the 4 KB working-memory budget.
 *
 * Only 1-bit-per-pixel BMP files are supported (as produced by e.g. GIMP or
 * ImageMagick with `-type bilevel`).
 */
void display_draw_bitmap(int16_t dx, int16_t dy, const char *sd_path) {
    if (!sdcard_available() || !sd_path) return;

    int fh = sd_open(sd_path, 'r');
    if (fh < 0) return;

    // Read BMP file header (14 bytes) + DIB header (40 bytes)
    uint8_t hdr[54];
    if (sd_read(fh, (char *)hdr, sizeof(hdr)) != sizeof(hdr)) {
        sd_close(fh);
        return;
    }

    // Verify BMP signature
    if (hdr[0] != 'B' || hdr[1] != 'M') { sd_close(fh); return; }

    uint32_t px_offset = hdr[10] | (hdr[11] << 8) | (hdr[12] << 16) | (hdr[13] << 24);
    int32_t  bmp_w     = hdr[18] | (hdr[19] << 8) | (hdr[20] << 16) | (hdr[21] << 24);
    int32_t  bmp_h     = hdr[22] | (hdr[23] << 8) | (hdr[24] << 16) | (hdr[25] << 24);
    uint16_t bit_count = hdr[28] | (hdr[29] << 8);

    if (bit_count != 1) { sd_close(fh); return; }  // only 1-bit supported

    bool flip = bmp_h > 0;                           // positive height = bottom-up
    if (bmp_h < 0) bmp_h = -bmp_h;

    // BMP rows are padded to 4-byte boundaries
    uint32_t row_bytes = ((bmp_w + 31) / 32) * 4;
    // For an 800 px wide 1-bit image: row_bytes = ((800+31)/32)*4 = 100 bytes.
    // Buffer is sized generously (128) to handle any width up to 1024 px.

    sd_seek(fh, px_offset);

    static uint8_t row_buf[128];  // max 128 bytes per row (1024 px wide)

    epd.setPartialWindow(dx, dy, (int16_t)bmp_w, (int16_t)bmp_h);
    epd.firstPage();
    do {
        for (int32_t row = 0; row < bmp_h; row++) {
            int32_t src_row = flip ? (bmp_h - 1 - row) : row;
            sd_seek(fh, px_offset + src_row * row_bytes);
            sd_read(fh, (char *)row_buf,
                    row_bytes < sizeof(row_buf) ? row_bytes : sizeof(row_buf));

            for (int32_t col = 0; col < bmp_w; col++) {
                bool black = !((row_buf[col / 8] >> (7 - (col % 8))) & 1);
                epd.drawPixel(dx + (int16_t)col, dy + (int16_t)row,
                              black ? GxEPD_BLACK : GxEPD_WHITE);
            }
        }
    } while (epd.nextPage());

    sd_close(fh);
    _dirty = true;
}

int16_t display_width()  { return EPD_WIDTH;  }
int16_t display_height() { return EPD_HEIGHT; }
