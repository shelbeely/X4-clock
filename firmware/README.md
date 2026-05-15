# Xteink X4 — Firmware

Base firmware for the Xteink X4 "Pebble Watch" device.

## Hardware Summary

| Component | Detail |
|-----------|--------|
| MCU | ESP32-C3, single-core RISC-V @ 160 MHz |
| RAM | ~380 KB usable heap (no PSRAM) |
| Flash | 16 MB SPI |
| Display | 4.26" SSD1677, 800×480 B&W e-ink, SPI shared with SD |
| Storage | microSD on shared SPI bus |
| Buttons | 7 total: 4 nav (ADC GPIO1), 2 vol (ADC GPIO2), 1 power (GPIO3 digital) |
| Battery | 650 mAh, read via ADC GPIO0 (÷2 voltage divider) |
| USB detect | GPIO20 HIGH = charging |

## Prerequisites

- [PlatformIO](https://platformio.org/) (CLI or VS Code extension)
- GCC (for the mquickjs stdlib build step — any version supporting C99)
- `git`

## Setup

### 1. Fetch and prepare mquickjs

mquickjs is not a PlatformIO registry package — it must be downloaded and its
custom stdlib header generated before building.

```bash
cd firmware/
bash scripts/fetch_mquickjs.sh
```

This will:
1. Clone `github.com/bellard/mquickjs` (shallow)
2. Build the `x4_stdlib` host tool from `scripts/x4_stdlib.c`
3. Generate `lib/mquickjs/src/x4_stdlib.h` and `lib/mquickjs/src/mquickjs_atom.h`
4. Copy all mquickjs `.c`/`.h` source files to `lib/mquickjs/src/`

Re-run the script whenever `scripts/x4_stdlib.c` changes (e.g., after adding
a new JS API function).

### 2. Build

```bash
cd firmware/
pio run
```

### 3. Flash

```bash
pio run -t upload
```

### 4. Monitor

```bash
pio device monitor
```

## SD Card Layout

```
SD:/
├── apps/
│   ├── clock.js           ← default clock app (copy from apps/)
│   ├── default.txt        ← optional: name of app to auto-launch
│   └── <your apps>
└── faces/
    ├── digital.js         ← built-in digital face (JS version)
    ├── minimal.js         ← time-only minimal face
    ├── seconds.js         ← HH:MM:SS seconds face
    ├── status.js          ← info-dense border + day/battery strip
    ├── roman.js           ← Roman numeral clock
    ├── world_clock.js     ← two-timezone side-by-side display
    └── <your faces>
```

Copy the example clock app:

```bash
cp ../apps/clock.js <SD_MOUNT>/apps/clock.js
```

## Compiling Apps to Bytecode

The firmware supports both `.js` source files and `.app` pre-compiled
mquickjs bytecode files.  Bytecode offers faster startup:

```bash
# Build mqjs on your host machine
git clone https://github.com/bellard/mquickjs.git /tmp/mqjs
cd /tmp/mqjs && make mqjs

# Compile an app for ESP32-C3 (32-bit RISC-V)
/tmp/mqjs/mqjs -m32 -o clock.app clock.js
```

## Repository Layout

```
firmware/
├── platformio.ini           # PlatformIO config
├── partitions.csv           # 16 MB dual-OTA + SPIFFS layout
├── lib/
│   └── mquickjs/
│       ├── library.json     # PlatformIO library descriptor
│       └── src/             # Populated by fetch_mquickjs.sh
│           ├── mquickjs.c   # Engine
│           ├── x4_stdlib.h  # GENERATED — stdlib + function table
│           └── ...
├── scripts/
│   ├── x4_stdlib.c          # Custom stdlib definition (edit to add JS APIs)
│   └── fetch_mquickjs.sh    # One-time setup script
└── src/
    ├── bsp/
    │   └── x4_pins.h        # All GPIO / ADC defines + thresholds
    ├── drivers/
    │   ├── display.cpp/h    # GxEPD2 page-mode wrapper
    │   ├── buttons.cpp/h    # ADC ladder + power button + debounce
    │   ├── battery.cpp/h    # Battery % from ADC
    │   └── sdcard.cpp/h     # SdFat init + file helpers
    ├── runtime/
    │   ├── js_engine.cpp/h  # mquickjs context lifecycle
    │   ├── js_display.cpp/h # JS: display.* bindings
    │   ├── js_input.cpp/h   # JS: input.onButton() event queue
    │   ├── js_fs.cpp/h      # JS: fs.*  bindings
    │   ├── js_system.cpp/h  # JS: system.* bindings + gc()
    │   └── app_loader.cpp/h # SD scan, .js/.app loader, lifecycle
    ├── builtin/
    │   ├── clock_app.h      # Built-in clock app public API
    │   └── clock_app.cpp    # Built-in clock app + face loader
    └── main.cpp
```

## Memory Budget

| Component | RAM |
|-----------|-----|
| mquickjs runtime | ~60 KB |
| JS context buffer (hard limit) | 64 KB (static array) |
| GxEPD2 page buffer (page-mode) | ~4 KB |
| SdFat + file buffers | ~8 KB |
| Arduino / FreeRTOS overhead | ~50 KB |
| **Total** | **~186 KB** (within 380 KB) |

## Clock Faces

The built-in clock app loads swappable faces from `/faces/` on the SD card.
A face is a `.js` file that exports:

```js
function setup() { /* initialise — called once after load */ }
function draw()  { /* called every second by the clock app */ }
```

Place faces at `/faces/<name>.js`.  The clock app scans this directory at boot
and makes all discovered faces available via the LEFT/RIGHT buttons.

The built-in C++ face (`Digital`) is always slot 0 and works without an SD card.

See [../apps/README.md](../apps/README.md) for the complete face developer guide.

---

## Adding New JavaScript APIs

1. Write the C implementation in `src/runtime/js_*.cpp` as `extern "C"` functions
   with the signature `JSValue fn(JSContext *, JSValue *, int, JSValue *)`
2. Declare the function in `firmware/scripts/x4_stdlib.c` using the
   `JS_CFUNC_DEF` macro inside the appropriate property table
3. Re-run `bash scripts/fetch_mquickjs.sh` to regenerate `x4_stdlib.h`
4. Rebuild with `pio run`

## JavaScript Runtime: MicroQuickJS

This firmware uses [MicroQuickJS](https://github.com/bellard/mquickjs) by
Fabrice Bellard — a minimal JavaScript engine designed for embedded targets.

Key properties:
- **Fixed memory buffer** — the JS context receives exactly 64 KB; no `malloc`
- **Compacting GC** — no reference counting; `gc()` is always available
- **32-bit bytecode** — compiled with `mqjs -m32` for RISC-V/ARM32 targets
- **~2 KB code footprint** — much smaller than standard QuickJS

The `.app` bytecode format is the mquickjs native format (magic `0xACFB`),
not compatible with standard QuickJS `qjsc` output.
