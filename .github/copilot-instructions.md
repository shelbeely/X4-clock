# Copilot Instructions — Xteink X4 Firmware

## Project Overview

This repository is the base firmware for the **Xteink X4** — an ESP32-C3 e-ink smartwatch with a
4.26" SSD1677 display (800×480, B&W).  The core idea: users drop a `.js` file on a microSD card
and the firmware runs it without any recompile.

The JavaScript runtime is **MicroQuickJS** (mquickjs) by Fabrice Bellard — a minimal engine with
a fixed 64 KB memory buffer.  All JS APIs are registered at build time via `firmware/scripts/x4_stdlib.c`.

---

## Repository Layout

```
apps/           JavaScript example apps and clock faces (run on the device)
  faces/        Clock face scripts (.js)
  README.md     Complete JS API reference + face developer guide

firmware/       ESP32-C3 PlatformIO project (C/C++)
  platformio.ini
  partitions.csv
  scripts/
    x4_stdlib.c          Custom stdlib definition — edit to add JS APIs
    fetch_mquickjs.sh    One-time setup: fetches mquickjs, generates headers
  lib/mquickjs/src/      Populated by fetch_mquickjs.sh (do not edit manually)
  src/
    bsp/x4_pins.h        GPIO/ADC pin numbers + thresholds
    drivers/             display, buttons, battery, sdcard, wifi_manager
    runtime/             JS engine, all JS bindings, app_loader
    builtin/             Built-in clock app (works without SD card)
    main.cpp             Arduino setup()/loop() — boot sequence

scripts/
  deploy_sd.sh           Copies apps/faces to an SD card mount point

README.md                Project overview, quick start
CONTRIBUTING.md          How to add new JS APIs and contribute
```

---

## Build, Flash, and Monitor

```bash
# One-time setup (from the firmware/ directory)
bash scripts/fetch_mquickjs.sh

# Build
cd firmware && pio run

# Build + flash
cd firmware && pio run -t upload

# Serial monitor (115200 baud)
cd firmware && pio device monitor
```

Re-run `fetch_mquickjs.sh` after any change to `firmware/scripts/x4_stdlib.c`.

---

## Architecture: Adding a New JavaScript API

The process is always the same four steps:

1. **Write the C++ binding** in a new or existing `firmware/src/runtime/js_*.cpp` file.
   Functions must be declared `extern "C"` with the signature:
   ```cpp
   extern "C" JSValue js_myfunc(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
   ```

2. **Register the function** in `firmware/scripts/x4_stdlib.c` using `JS_CFUNC_DEF` inside
   the relevant property table (e.g. `js_display_funcs`, `js_system_funcs`).

3. **Regenerate headers** by re-running `bash firmware/scripts/fetch_mquickjs.sh`.

4. **Rebuild** with `cd firmware && pio run`.

**Do not** modify `lib/mquickjs/src/x4_stdlib.h` directly — it is generated.

---

## Key Constraints and Invariants

### Memory
- The JS context has exactly **64 KB** — no dynamic growth.  Every allocation counts.
- JS apps should call `gc()` after building large strings or reading file chunks.
- Individual strings must stay well under 64 KB.

### E-ink display
- `display.refresh()` takes **~3.5 s** — call it once in `setup()` only.
- `display.partialRefresh()` takes **~0.42 s** — use this for all subsequent updates.
- Always call `display.hibernate()` before deep sleep to cut display standby to µA.
- `display.wake()` is called automatically before any drawing command.

### JS app lifecycle (apps)
- Apps must define `setup()` (called once) and `loop()` (called repeatedly).
- Register `input.onButton()` exactly once at the top level — a second call silently
  replaces the first.

### JS face lifecycle (clock faces)
- Faces must define `setup()` (called once) and `draw()` (called every second by the
  clock app).
- Faces **must not** call `input.onButton()` — button handling belongs to the clock app.
- `draw()` should only call `display.partialRefresh()` when the display content actually
  changes; checking whether the minute changed before redrawing is the standard pattern.

### SD card paths
All SD card paths are absolute from the SD root: `/apps/`, `/faces/`, `/config/`,
`/notifications/`, `/calendar/`, `/reminders/`.

### WiFi / HTTP
- WiFi calls (`wifi.connect`, `wifi.startAP`) block for up to 10 s.
- HTTP `http.get()` blocks; `http.getAsync()` calls back on the next `loop()` tick.
- Response bodies are capped at 4 096 bytes to stay within the JS heap budget.

---

## Firmware C++ Conventions

- All pin numbers and ADC thresholds live in `firmware/src/bsp/x4_pins.h`.  Never
  hard-code GPIO numbers elsewhere.
- Driver init functions are called from `main.cpp` in dependency order.
- JS binding files are named `js_<subsystem>.cpp` / `js_<subsystem>.h`.
- Use `Serial.printf()` / `Serial.println()` for debug output — `system.log()` in JS
  routes to Serial.
- The shared SPI bus is `g_spi` (defined in `main.cpp`); pass it to `display_init()` and
  `sdcard_init()`.

## JavaScript App Conventions

- Use `var` (not `let`/`const`) — mquickjs supports ES5 + a subset of ES6.
- Two-space indentation.
- Keep apps under 32 KB — the JS parser reads the whole file before executing.
- Pad time values to two digits with a helper: `function pad2(n){return n<10?"0"+n:""+n;}`.
- Prefer `system.millis()` for elapsed-time calculations; use `system.time()` only when
  wall-clock time is needed (requires NTP sync or `system.setTime()`).
- HTTP body parsing: always check that `body` is non-empty before calling `JSON.parse()`.

---

## Configuration Files (SD card)

| Path | Format |
|------|--------|
| `/config/wifi.json` | `{"ssid":"…","pass":"…"}` |
| `/config/settings.json` | `{"rotation":0,"refresh_ms":20,"tz_offset":0,"owm_key":"…","city":"London"}` |
| `/notifications/pending.json` | `[{"title":"…","time":"HH:MM","body":"…"}]` |
| `/calendar/events.json` | `[{"id":1,"title":"…","start":<unix>,"end":<unix>,"desc":"…"}]` |
| `/reminders/pending.json` | `[{"id":1,"title":"…","time":<unix>,"body":"…","recurring":<seconds>}]` |
| `/apps/default.txt` | Single line: filename of app to auto-launch (e.g. `clock.js`) |

---

## Testing

There are no automated test suites.  Validation is done by:

1. Building the firmware: `cd firmware && pio run` (must succeed with no errors).
2. Flashing to a device and observing Serial output + display behaviour.
3. For JS apps, testing on-device with `pio device monitor` for log output.

When making firmware changes, always verify the build compiles cleanly before
declaring a task complete.

---

## Documentation

- `README.md` — project overview, hardware table, quick start, JS API at a glance.
- `firmware/README.md` — build/flash instructions, repository layout, memory budget,
  adding JS APIs, mquickjs details.
- `apps/README.md` — full JS API reference, app lifecycle, face developer guide,
  debugging, common mistakes.
- `CONTRIBUTING.md` — contributor workflow, coding style, how to add new JS APIs.

Keep these in sync when making changes.  API additions should update **all three**
README files: the at-a-glance table in `README.md`, the API reference in `apps/README.md`,
and (if a new binding file is created) the repository layout in `firmware/README.md`.
