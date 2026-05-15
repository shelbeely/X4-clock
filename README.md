# X4-clock — Xteink X4 Base Firmware

Full base firmware for the **Xteink X4** "Pebble Watch" device.
Drop a JavaScript app on the SD card and it runs — no recompiling the firmware needed.

## What's in this Repository

```
.
├── apps/
│   ├── clock.js        ← default wall-clock app (copy to SD card)
│   └── README.md       ← complete JS API reference + app developer guide
├── firmware/
│   ├── platformio.ini
│   ├── partitions.csv
│   ├── lib/mquickjs/   ← MicroQuickJS engine (populated by setup script)
│   ├── scripts/
│   │   ├── x4_stdlib.c          ← custom stdlib definition
│   │   └── fetch_mquickjs.sh    ← one-time setup script
│   ├── src/
│   │   ├── bsp/         ← GPIO / ADC pin definitions
│   │   ├── drivers/     ← display, buttons, battery, SD card
│   │   ├── runtime/     ← JS engine, bindings, app loader
│   │   └── builtin/     ← C fallback clock (no SD card needed)
│   └── README.md        ← build + flash instructions
└── README.md
```

## Hardware

| Component | Detail |
|-----------|--------|
| MCU | ESP32-C3 (RISC-V 32-bit, 160 MHz, 16 MB flash) |
| Display | 4.26" SSD1677 e-ink, 800×480, B&W |
| Storage | microSD (shared SPI) |
| Buttons | 7 — 4 nav (ADC), 2 vol (ADC), 1 power (digital) |
| Battery | 650 mAh Li-ion |

## Quick Start

### 1. Set up mquickjs (once)

```bash
cd firmware/
bash scripts/fetch_mquickjs.sh
```

### 2. Build and flash

```bash
pio run -t upload
```

### 3. Prepare the SD card

```bash
mkdir -p <SD>/apps
cp apps/clock.js <SD>/apps/clock.js
```

### 4. Write your own app

Create `/apps/mygame.js` on the SD card:

```js
function setup() {
  display.clear();
  display.print(200, 200, "Hello X4!", 3);
  display.refresh();
}

function loop() {
  // Update display, read sensors, etc.
}

input.onButton(function(btn) {
  if (btn === "confirm") {
    display.print(20, 440, "Batt: " + system.battery() + "%", 1);
    display.partialRefresh();
  }
});
```

See [apps/README.md](apps/README.md) for the full API reference.

## JavaScript API at a Glance

| Object | Methods |
|--------|---------|
| `display` | `clear()`, `print(x,y,text,size)`, `drawRect()`, `drawBitmap()`, `refresh()`, `partialRefresh()`, `width()`, `height()` |
| `input` | `onButton(fn)` |
| `fs` | `open()`, `read()`, `write()`, `close()`, `seek()`, `size()`, `list()`, `exists()` |
| `system` | `millis()`, `battery()`, `sleep(ms)`, `log(msg)`, `appName()` |
| global | `gc()` |

## JavaScript Runtime: MicroQuickJS

Uses [MicroQuickJS](https://github.com/bellard/mquickjs) by Fabrice Bellard.
- Fixed 64 KB memory buffer per app context — no heap fragmentation
- Compacting GC — `gc()` always available
- 32-bit bytecode (`mqjs -m32`) for ESP32-C3 RISC-V
- Apps can also be pre-compiled to `.app` bytecode for faster startup

## Reference Hardware Links

- https://github.com/sunwoods/Xteink-X4
- https://www.good-display.com/product/457.html
