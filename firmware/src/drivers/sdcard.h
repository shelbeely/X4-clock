#pragma once
/*
 * sdcard.h — microSD card driver for Xteink X4
 *
 * Initialises SdFat on the shared SPI bus and exposes a minimal file API
 * suited to chunk-by-chunk reads.  All paths are absolute from the SD root,
 * e.g. "/apps/clock.js".
 *
 * At most SD_MAX_OPEN_FILES may be open simultaneously.
 * File handles are small integers [0, SD_MAX_OPEN_FILES).
 */

#include <Arduino.h>
#include <SPI.h>

#define SD_MAX_OPEN_FILES 4
#define SD_CHUNK_SIZE     4096

// Returns true if the SD card was found and mounted successfully.
bool sdcard_init(SPIClass &spi);
bool sdcard_available();

// --- File operations ---
// Returns handle ≥ 0 on success, -1 on error.
// mode: 'r' = read, 'w' = write (create/truncate), 'a' = append
int  sd_open (const char *path, char mode);
int  sd_read (int handle, char *buf, size_t len);   // returns bytes read
int  sd_write(int handle, const char *data, size_t len);
bool sd_close(int handle);
bool sd_seek (int handle, uint32_t offset);
int32_t sd_size(const char *path);                  // -1 if not found
bool sd_exists(const char *path);

// --- Directory listing ---
// Fills 'names' with up to max_count null-terminated filenames from 'dir_path'.
// Returns the number of entries written.  Skips hidden files (leading dot).
int  sd_list_dir(const char *dir_path,
                 char names[][256], bool is_dir[], uint32_t sizes[],
                 int max_count);
