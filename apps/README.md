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
| `display.hibernate()` | Put SSD1677 into lowest-power standby (~µA). Call after `refresh()` / `partialRefresh()` when on battery. |
| `display.wake()` | Wake display from standby. Automatically called before any drawing command. |

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
| `system.batteryLow()` | Returns `true` when battery ≤ 5 % and not charging. Use to show a low-battery indicator in your face. |
| `system.sleep(ms)` | Enter deep sleep. Hibernates the display automatically. `ms=0` = wake only on power button. |
| `system.lightSleep(ms)` | Enter light sleep for up to `ms` milliseconds. Preserves RAM and WiFi state. Wakeable by timer, power button, or any nav/vol button. Returns actual sleep duration in ms. |
| `system.setIdleTimeout(ms)` | Set auto-sleep idle timeout in ms (default 600 000 = 10 min). Pass `0` to disable. Only active when on battery. |
| `system.setRefreshInterval(ms)` | Set the sleep duration between `loop()` calls (default 20 ms). Higher values save more power; lower values improve responsiveness. Range: 1–60 000 ms. |
| `system.log(msg)` | Print message to USB Serial (development). |
| `system.appName()` | Filename of the running app. |

### `display.*` — new methods

| Call | Description |
|------|-------------|
| `display.setRotation(r)` | Set screen orientation. `r`: 0 = landscape (default, 800×480), 1 = portrait (480×800), 2 = reversed landscape, 3 = reversed portrait. Persists across hibernate/wake. |
| `display.rotation()` | Returns current rotation (0–3). |

### `wifi.*`

Requires `WiFi` hardware.  All calls are synchronous.

| Call | Description |
|------|-------------|
| `wifi.connect(ssid, pass)` | Connect to a WiFi network (station mode). Blocks up to 10 s. Returns `true` on success. `pass` may be `""` for open networks. |
| `wifi.startAP(ssid, pass)` | Start a SoftAP (access point). `pass` may be `""` for open network. Returns `true` on success. |
| `wifi.disconnect()` | Disconnect from network or stop AP and turn WiFi off. |
| `wifi.connected()` | Returns `true` when associated to an AP. |
| `wifi.ip()` | Returns current IP address as a string (e.g. `"192.168.1.42"`). Returns `"0.0.0.0"` when not connected. |

Credentials can be loaded automatically by calling `wifi.connect()` without arguments — the firmware reads `/config/wifi.json` (`{"ssid":"…","pass":"…"}`) if it exists.

### `http.*`

Requires WiFi to be connected.  Response bodies are capped at **4 096 bytes** to stay within the JS heap budget.

| Call | Description |
|------|-------------|
| `http.get(url)` | Synchronous HTTP GET. Returns response body as a string, or `""` on error. |
| `http.getAsync(url, callback)` | Non-blocking GET. `callback(error, body)` is called on the next `loop()` tick. `error` is `""` on success. Only one async request may be in flight at a time. |

### `server.*`

Requires WiFi (station or AP mode).  Call `server.handleClient()` from `loop()` to process incoming requests.

| Call | Description |
|------|-------------|
| `server.begin(port)` | Start the HTTP server on the given port (default 80). |
| `server.stop()` | Stop the server and free resources. |
| `server.onRequest(path, fn)` | Register route handler. `fn(method, body)` is called synchronously from `handleClient()`. Up to 8 routes. |
| `server.send(code, contentType, body)` | Send an HTTP response. Must be called from inside a route handler. |
| `server.handleClient()` | Process one pending HTTP request. Call from `loop()`. |

**Example — minimal settings page:**

```js
function setup() {
  wifi.startAP("X4-Setup", "configure");
  server.begin(80);
  server.onRequest("/", function(method, body) {
    server.send(200, "text/html", "<h1>Hello from X4!</h1>");
  });
  server.onRequest("/api/save", function(method, body) {
    var data = JSON.parse(body);
    system.log("Received: " + JSON.stringify(data));
    server.send(200, "text/plain", "Saved");
  });
}

function loop() {
  server.handleClient();
}
```

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

## Debugging

Use `system.log(msg)` to print messages to the USB serial console:

```js
system.log("value: " + myVar);
```

Start the monitor in a second terminal:

```bash
cd firmware/
pio device monitor
```

### What serial output looks like

On a clean boot you will see the firmware banner followed by log lines from
your app:

```
X4 boot
Loading app: /apps/mygame.js
value: 42
```

Runtime errors (syntax errors, uncaught exceptions) are printed as:

```
JS error: ReferenceError: 'foo' is not defined
```

If nothing appears, check the baud rate — the default is **115200**.

---

## Display Refresh Strategy

The SSD1677 e-ink display has two refresh modes with very different costs:

| Method | Time | Use when |
|--------|------|----------|
| `display.refresh()` | ~3.5 s | App starts — clears ghosting from previous content |
| `display.partialRefresh()` | ~0.42 s | Any subsequent update in `loop()` or `draw()` |

**Best practice**

1. Call `display.clear()` + `display.refresh()` **once in `setup()`** to start
   with a clean screen.
2. Call `display.clear()` + `display.partialRefresh()` for every subsequent
   redraw.

Calling `display.refresh()` inside `loop()` will freeze the device for 3.5 s
on every iteration and wear out the display faster.

---

## Common Mistakes

| Mistake | What happens | Fix |
|---------|-------------|-----|
| Calling `display.refresh()` inside `loop()` | Device freezes ~3.5 s every tick | Use `display.partialRefresh()` for updates; call `display.refresh()` only in `setup()` |
| Registering `input.onButton()` more than once | Second call silently replaces the first handler | Call `input.onButton()` exactly once, at top level |
| Not calling `gc()` after string operations | JS heap fills up and the app crashes | Call `gc()` after building large strings or reading file chunks |
| Allocating a string larger than ~64 KB | Immediate out-of-memory crash | Keep individual strings short; process data in chunks |
| Calling `draw()` from `loop()` in an app | Undefined — `draw()` is a face API, not an app API | Apps use `loop()`; faces use `draw()` |

---



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
| `countdown.js` | Countdown timer — set duration with LEFT/RIGHT, start/pause with CONFIRM, saves last duration |
| `battery_monitor.js` | Live battery gauge — large percentage number and a proportional fill bar |
| `weather.js` | OpenWeatherMap forecast — temperature, condition, humidity; auto-refreshes every 10 min |
| `notifications.js` | Notification viewer — displays events from `/notifications/pending.json` |
| `setup_server.js` | Web configuration portal — AP mode + browser UI to configure WiFi, display, and weather |

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
var _lastTotalMin = -1;

function setup() {       // called once after load
  display.clear();
  display.refresh();
}

function draw() {        // called every second by the clock app
  var ms       = system.millis();
  var totalSec = Math.floor(ms / 1000);
  var totalMin = Math.floor(totalSec / 60);
  var h        = Math.floor(totalSec / 3600) % 24;
  var m        = totalMin % 60;

  if (totalMin === _lastTotalMin) return;   // nothing changed — skip
  _lastTotalMin = totalMin;

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
| `faces/seconds.js` | HH:MM:SS — redraws every second |
| `faces/status.js` | Info-dense: double border + time + day/battery data strip |
| `faces/roman.js` | Roman numeral clock (e.g. `XI : XLV`) |
| `faces/world_clock.js` | Two timezones side by side (configure `UTC_OFFSET_HOURS` in file) |

