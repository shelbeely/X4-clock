/*
 * sdcard.cpp — SdFat driver for Xteink X4
 *
 * Mounts the microSD card on the shared SPI bus using SHARED_SPI mode so
 * the e-paper display can also use the bus.  Retries init up to 3 times at
 * boot.  Exposes an integer-handle file API (up to SD_MAX_OPEN_FILES open
 * simultaneously) that matches what the JS fs.* bindings expect.
 */

#include "sdcard.h"
#include "bsp/x4_pins.h"
#include <SdFat.h>

static SdFat  s_sd;
static SdFile s_files[SD_MAX_OPEN_FILES];
static bool   s_file_open[SD_MAX_OPEN_FILES] = {};
static bool   s_mounted = false;

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

bool sdcard_init(SPIClass &spi) {
    SdSpiConfig cfg(PIN_SD_CS, SHARED_SPI, SD_SCK_MHZ(SD_SPI_MHZ), &spi);
    for (int attempt = 0; attempt < 3; attempt++) {
        if (s_sd.begin(cfg)) {
            s_mounted = true;
            return true;
        }
        delay(200);
    }
    s_mounted = false;
    return false;
}

bool sdcard_available() { return s_mounted; }

// ---------------------------------------------------------------------------
// Handle allocation
// ---------------------------------------------------------------------------

static int alloc_handle() {
    for (int i = 0; i < SD_MAX_OPEN_FILES; i++) {
        if (!s_file_open[i]) return i;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// File operations
// ---------------------------------------------------------------------------

int sd_open(const char *path, char mode) {
    if (!s_mounted || !path) return -1;
    int h = alloc_handle();
    if (h < 0) return -1;

    oflag_t flags;
    switch (mode) {
        case 'w': flags = O_WRONLY | O_CREAT | O_TRUNC; break;
        case 'a': flags = O_WRONLY | O_CREAT | O_APPEND; break;
        default:  flags = O_RDONLY; break;
    }

    if (!s_files[h].open(path, flags)) return -1;
    s_file_open[h] = true;
    return h;
}

int sd_read(int h, char *buf, size_t len) {
    if (h < 0 || h >= SD_MAX_OPEN_FILES || !s_file_open[h]) return -1;
    int n = s_files[h].read(buf, len);
    return (n < 0) ? 0 : n;
}

int sd_write(int h, const char *data, size_t len) {
    if (h < 0 || h >= SD_MAX_OPEN_FILES || !s_file_open[h]) return -1;
    return (int)s_files[h].write(data, len);
}

bool sd_close(int h) {
    if (h < 0 || h >= SD_MAX_OPEN_FILES || !s_file_open[h]) return false;
    s_files[h].close();
    s_file_open[h] = false;
    return true;
}

bool sd_seek(int h, uint32_t offset) {
    if (h < 0 || h >= SD_MAX_OPEN_FILES || !s_file_open[h]) return false;
    return s_files[h].seekSet(offset);
}

int32_t sd_size(const char *path) {
    if (!s_mounted || !path) return -1;
    SdFile f;
    if (!f.open(path, O_RDONLY)) return -1;
    int32_t sz = (int32_t)f.fileSize();
    f.close();
    return sz;
}

bool sd_exists(const char *path) {
    if (!s_mounted || !path) return false;
    return s_sd.exists(path);
}

// ---------------------------------------------------------------------------
// Directory listing
// ---------------------------------------------------------------------------

int sd_list_dir(const char *dir_path,
                char names[][256], bool is_dir[], uint32_t sizes[],
                int max_count) {
    if (!s_mounted || !dir_path || max_count <= 0) return 0;

    SdFile dir;
    if (!dir.open(dir_path, O_RDONLY)) return 0;

    SdFile entry;
    int count = 0;
    while (count < max_count && entry.openNext(&dir, O_RDONLY)) {
        char name[256];
        entry.getName(name, sizeof(name));

        // Skip hidden entries
        if (name[0] != '.') {
            strncpy(names[count], name, 255);
            names[count][255] = '\0';
            is_dir[count]     = entry.isDir();
            sizes[count]      = entry.isDir() ? 0 : entry.fileSize();
            count++;
        }
        entry.close();
    }
    dir.close();
    return count;
}
