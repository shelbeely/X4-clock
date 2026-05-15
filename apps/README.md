# Xteink X4 — App Developer Guide

Apps are JavaScript (`.js`) or pre-compiled mquickjs bytecode (`.app`) files
that live on the SD card at `/apps/`.

---

## Quick Start

1. Copy your `.js` file to the SD card: `/apps/mygame.js`
2. Insert the SD card and power on the X4
3. If `/apps/` contains more than one app a selection list is shown;
   otherwise the single app auto-launches

---

## App Lifecycle

An app **must** define at least `setup()` and `loop()`:

```js
function setup() {
  // Called once on launch.  Initialise state, clear display.
  display.clear();
  display.refresh();
}

function loop() {
  // Called repeatedly.  Return as quickly as possible.
  // Do not call delay() — use system.millis() for timing.
}
```

Register a button handler via `input.onButton`:

```js
input.onButton(function(btn) {
  // btn: "right" | "left" | "confirm" | "back"
  //      "volup" | "voldown" | "power"
});
```

---

## Full JavaScript API Reference

### `display.*`

| Call | Description |
|------|-------------|
| `display.clear()` | Fill screen with white |
| `display.print(x, y, text, size)` | Draw text. `size` 1–4 (1 = tiny, 4 = large) |
| `display.drawRect(x, y, w, h, filled)` | Draw rectangle |
| `display.drawBitmap(x, y, path)` | Draw 1-bit BMP from SD card path |
| `display.refresh()` | Full e-ink refresh (~3.5 s, clears ghosting) |
| `display.partialRefresh()` | Partial refresh (~0.42 s, for frequent updates) |
| `display.width()` | Returns 800 |
| `display.height()` | Returns 480 |

### `input.*`

| Call | Description |
|------|-------------|
| `input.onButton(fn)` | Register callback `fn(buttonName)`. Replaces any previous callback. |

Button names: `"right"`, `"left"`, `"confirm"`, `"back"`, `"volup"`, `"voldown"`, `"power"`

### `fs.*`

| Call | Description |
|------|-------------|
| `fs.open(path, mode)` | Open file. `mode`: `"r"` / `"w"` / `"a"`. Returns handle (int) or -1. |
| `fs.read(handle, size)` | Read up to `size` bytes (max 4096). Returns string. |
| `fs.write(handle, data)` | Write string. Returns bytes written. |
| `fs.close(handle)` | Close file. |
| `fs.seek(handle, offset)` | Seek to byte offset. |
| `fs.size(path)` | File size in bytes, or -1. |
| `fs.list(dirPath)` | Returns `[{name, size, isDir}, …]`. |
| `fs.exists(path)` | Returns boolean. |

### `system.*`

| Call | Description |
|------|-------------|
| `system.millis()` | Milliseconds since boot. |
| `system.battery()` | Battery percentage 0–100. |
| `system.sleep(ms)` | Enter deep sleep. `ms=0` = wake only on power button. |
| `system.log(msg)` | Print message to USB Serial (development). |
| `system.appName()` | Filename of the running app. |

### `gc()`

Manually trigger the garbage collector.  Call after allocating large
temporary strings or processing a file chunk.

---

## Memory Constraints

| Resource | Limit |
|----------|-------|
| Working memory (JS heap) | **64 KB** — enforced by the runtime |
| Script file size | ≤ 32 KB recommended |
| fs.read() per call | ≤ 4 096 bytes |

### Best Practices

1. **Read large files in chunks** — never read an entire file at once:
   ```js
   var h = fs.open("/apps/data.bin", "r");
   var chunk;
   while ((chunk = fs.read(h, 4096)) !== "") {
     processChunk(chunk);
     gc();   // free chunk string before next read
   }
   fs.close(h);
   ```

2. **Call `gc()` after processing each chunk** — this frees the previous
   chunk string before allocating the next one.

3. **Reuse variables** — avoid accumulating many small strings in a loop.

4. **Avoid deep recursion** — the JS stack is limited.

---

## Compiling to Bytecode (`.app`)

Pre-compiling your script to mquickjs bytecode gives faster startup and
slightly lower RAM usage at load time.

On your development machine (requires a working C toolchain):

```bash
# 1. Build the mqjs CLI tool from the mquickjs source
git clone https://github.com/bellard/mquickjs.git
cd mquickjs && make mqjs

# 2. Compile your app (use -m32 for 32-bit ESP32-C3 target)
./mqjs -m32 -o mygame.app mygame.js
```

Copy `mygame.app` to `/apps/` on the SD card.  The firmware detects `.app`
files automatically.

---

## Setting a Default App

Create `/apps/default.txt` containing only the filename of the default app
(no path, no extension alternative needed — just the exact filename):

```
clock.js
```

If this file is absent and multiple apps are present, a selection picker is
shown at boot.

---

## Example Apps

| App | Description |
|-----|-------------|
| `clock.js` | Standalone wall clock — large HH:MM, day counter, battery on confirm, sleep on power |
| `hello.js` | Minimal Hello World — static greeting, battery on confirm, sleep on power |
| `stopwatch.js` | Start/stop/reset stopwatch with live per-second updates |
| `battery_monitor.js` | Live battery gauge — large percentage number and a proportional fill bar |

---

## Clock Face Developer Guide

The firmware includes a **built-in clock application** that loads swappable
faces.  A face is a small `.js` file placed at `/faces/<name>.js` on the SD
card.

### How faces work

- The clock app calls `draw()` on the active face **every second**.
- `draw()` should check whether a visible change has occurred (e.g. the minute
  changed) and only call `display.partialRefresh()` when it redraws — partial
  refreshes take ~420 ms so calling one every second is fine, but calling one
  every loop when nothing changed wastes battery.
- The face **must not** call `input.onButton()`.  All button handling is managed
  by the clock app.

### Minimal face template

```js
var _lastMinute = -1;

function setup() {       // called once after load
  display.clear();
  display.refresh();
}

function draw() {        // called every second by the clock app
  var ms       = system.millis();
  var totalSec = Math.floor(ms / 1000);
  var h        = Math.floor(totalSec / 3600) % 24;
  var m        = Math.floor(totalSec / 60)   % 60;

  if (m === _lastMinute) return;   // nothing changed — skip
  _lastMinute = m;

  display.clear();
  display.print(200, 240, pad2(h) + ":" + pad2(m), 4);
  display.partialRefresh();
  gc();
}

function pad2(n) { return n < 10 ? "0" + n : "" + n; }
```

Save as `/faces/myface.js` on the SD card.  The clock app will detect it
automatically on the next boot.

### Face API reference

| Available API | Notes |
|---------------|-------|
| `display.*`   | All display functions — use `display.partialRefresh()` for updates |
| `system.millis()` | Milliseconds since boot — derive H/M/S from this |
| `system.battery()` | Battery 0–100 % |
| `system.log(msg)` | Debug output to USB Serial |
| `gc()` | Trigger garbage collector — call after allocating temporary strings |

### Lifecycle

```
/faces/ scan at boot
        ↓
setup() — once
        ↓
draw()  — every second (clock app calls this)
        ↓
context destroyed when face is switched or clock app exits
```

### Switching faces on device

While the clock is running:
- **LEFT** — switch to the previous face
- **RIGHT** — switch to the next face
- **BACK** — exit to the app picker

### Example faces

| Face | Description |
|------|-------------|
| `faces/digital.js` | Large HH:MM + day counter + battery (JS mirror of built-in) |
| `faces/minimal.js` | Large HH:MM only, no decoration |
| `faces/bold.js` | HH:MM:SS — redraws every second |
| `faces/status.js` | Info-dense: double border + time + day/battery data strip |
| `faces/roman.js` | Roman numeral clock (e.g. `XI : XLV`) |

