# X4-clock — Xteink X4 Base Firmware

Full base firmware for the **Xteink X4** "Pebble Watch" device.
Drop a JavaScript app on the SD card and it runs — no recompiling the firmware needed.

## What's in this Repository

```
.
├── apps/
│   ├── clock.js              ← standalone wall-clock app
│   ├── hello.js              ← minimal Hello World starter
│   ├── stopwatch.js          ← start/stop/reset stopwatch
│   ├── countdown.js          ← countdown timer with persistent settings
│   ├── battery_monitor.js    ← live battery gauge with bar graph
│   ├── faces/
│   │   ├── digital.js        ← JS mirror of the built-in digital face
│   │   ├── minimal.js        ← time only, no decoration
│   │   ├── seconds.js        ← HH:MM:SS (redraws every second)
│   │   ├── status.js         ← info-dense with border and data strip
│   │   ├── roman.js          ← Roman numeral clock
│   │   └── world_clock.js    ← two-timezone side-by-side display
│   └── README.md             ← complete JS API + face developer guide
├── firmware/
│   ├── platformio.ini
│   ├── partitions.csv
│   ├── lib/mquickjs/         ← MicroQuickJS engine (populated by setup script)
│   ├── scripts/
│   │   ├── x4_stdlib.c       ← custom stdlib definition
│   │   └── fetch_mquickjs.sh ← one-time setup script
│   ├── src/
│   │   ├── bsp/              ← GPIO / ADC pin definitions
│   │   ├── drivers/          ← display, buttons, battery, SD card
│   │   ├── runtime/          ← JS engine, bindings, app loader
│   │   └── builtin/          ← built-in clock app (no SD card needed)
│   └── README.md             ← build + flash instructions
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
cd firmware/
pio run -t upload
```

### 3. Prepare the SD card

```bash
# Create required directories
mkdir -p <SD>/apps
mkdir -p <SD>/faces
mkdir -p <SD>/config
mkdir -p <SD>/notifications

# Copy the example apps
cp apps/clock.js            <SD>/apps/clock.js
cp apps/hello.js            <SD>/apps/hello.js
cp apps/stopwatch.js        <SD>/apps/stopwatch.js
cp apps/countdown.js        <SD>/apps/countdown.js
cp apps/battery_monitor.js  <SD>/apps/battery_monitor.js
cp apps/weather.js          <SD>/apps/weather.js
cp apps/notifications.js    <SD>/apps/notifications.js
cp apps/setup_server.js     <SD>/apps/setup_server.js

# Copy the clock faces
cp apps/faces/digital.js     <SD>/faces/digital.js
cp apps/faces/minimal.js     <SD>/faces/minimal.js
cp apps/faces/seconds.js     <SD>/faces/seconds.js
cp apps/faces/status.js      <SD>/faces/status.js
cp apps/faces/roman.js       <SD>/faces/roman.js
cp apps/faces/world_clock.js <SD>/faces/world_clock.js
```

Or use the helper script (run from the repository root):

```bash
bash scripts/deploy_sd.sh <SD>
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

## Built-in Clock App

The firmware ships with a **built-in clock application** that runs on boot even
without an SD card.  It is always the first item in the app picker.

While the clock is running:

| Button | Action |
|--------|--------|
| LEFT | Previous face |
| RIGHT | Next face |
| CONFIRM | Show battery % overlay |
| BACK | Return to app picker |
| POWER | Enter deep sleep |

### Clock Faces

Faces are `.js` files placed at `/faces/<name>.js` on the SD card.
A face exports two functions:

```js
function setup() { /* called once after the face loads */ }
function draw()  { /* called every second — update the display here */ }
```

The firmware ships five ready-made faces:

| Face | Description |
|------|-------------|
| `digital.js` | Large HH:MM + day counter + battery |
| `minimal.js` | Time only — large HH:MM, no extras |
| `seconds.js` | HH:MM:SS — redraws every second |
| `status.js` | Info-dense: border + time + day + battery strip |
| `roman.js` | Roman numeral time (e.g. `XI : XLV`) |
| `world_clock.js` | Two timezones side by side (configurable UTC offset) |

See [apps/README.md](apps/README.md) for the complete face developer guide.

---

## Example Apps

| App | Description |
|-----|-------------|
| `clock.js` | Standalone wall clock (separate from the built-in clock) |
| `hello.js` | Minimal Hello World — good starting point |
| `stopwatch.js` | Start/stop/reset stopwatch |
| `countdown.js` | Countdown timer with adjustable duration, pause, and persistent settings |
| `battery_monitor.js` | Live battery percentage with bar graph |
| `weather.js` | OpenWeatherMap current weather — temperature, condition, humidity |
| `notifications.js` | Notification viewer from `/notifications/pending.json` |
| `setup_server.js` | Browser-based WiFi & display configuration portal (AP mode) |

---

## SD Card Config Layout

```
SD:/
├── apps/              ← JS app files
├── faces/             ← clock face files
├── config/
│   ├── wifi.json      ← {"ssid":"…","pass":"…"}
│   ├── settings.json  ← {"rotation":0,"refresh_ms":20,"tz_offset":0,"owm_key":"…","city":"London"}
│   └── index.html     ← optional custom settings page
└── notifications/
    └── pending.json   ← [{"title":"…","time":540,"body":"…"},…]
```

---

## JavaScript API at a Glance

| Object | Methods |
|--------|---------|
| `display` | `clear()`, `print(x,y,text,size)`, `drawRect()`, `drawBitmap()`, `refresh()`, `partialRefresh()`, `width()`, `height()`, `setRotation(r)`, `rotation()` |
| `input` | `onButton(fn)` |
| `fs` | `open()`, `read()`, `write()`, `close()`, `seek()`, `size()`, `list()`, `exists()` |
| `system` | `millis()`, `battery()`, `batteryLow()`, `sleep(ms)`, `lightSleep(ms)`, `setIdleTimeout(ms)`, `setRefreshInterval(ms)`, `log(msg)`, `appName()` |
| `wifi` | `connect(ssid, pass)`, `startAP(ssid, pass)`, `disconnect()`, `connected()`, `ip()` |
| `http` | `get(url)`, `getAsync(url, cb)` |
| `server` | `begin(port)`, `stop()`, `onRequest(path, fn)`, `send(code, type, body)`, `handleClient()` |
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
