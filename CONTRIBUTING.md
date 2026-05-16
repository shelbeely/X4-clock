# Contributing to X4-clock

Thank you for contributing!  This guide covers everything you need to get set up,
the coding conventions used in this repository, and the steps for common contribution
types.

---

## Table of Contents

1. [Getting Started](#getting-started)
2. [Repository Structure](#repository-structure)
3. [Build and Flash](#build-and-flash)
4. [Adding a New JavaScript API](#adding-a-new-javascript-api)
5. [Adding a New Example App or Face](#adding-a-new-example-app-or-face)
6. [Firmware C++ Style Guide](#firmware-c-style-guide)
7. [JavaScript Style Guide](#javascript-style-guide)
8. [Documentation Standards](#documentation-standards)
9. [Testing](#testing)

---

## Getting Started

### Prerequisites

| Tool | Version | Notes |
|------|---------|-------|
| [PlatformIO CLI](https://platformio.org/install/cli) | latest | `pip install platformio` |
| GCC | any C99-capable | host build for `fetch_mquickjs.sh` only |
| `git` | any | needed by `fetch_mquickjs.sh` |

### One-time setup

```bash
# Clone the repository
git clone https://github.com/shelbeely/X4-clock.git
cd X4-clock

# Download mquickjs and generate the firmware's stdlib headers
cd firmware
bash scripts/fetch_mquickjs.sh
```

This script clones [github.com/bellard/mquickjs](https://github.com/bellard/mquickjs),
compiles a small host tool, and generates `lib/mquickjs/src/x4_stdlib.h`.
It needs to be re-run whenever `firmware/scripts/x4_stdlib.c` changes.

---

## Repository Structure

```
apps/           JS example apps and clock faces
firmware/       PlatformIO project (ESP32-C3, C/C++)
scripts/        Helper scripts (deploy_sd.sh)
```

See [README.md](README.md) for a full annotated tree.

---

## Build and Flash

```bash
# Build
cd firmware && pio run

# Build and flash (device must be connected via USB)
cd firmware && pio run -t upload

# Open the serial monitor (115 200 baud)
cd firmware && pio device monitor
```

A successful build produces `.pio/build/x4/firmware.bin`.

---

## Adding a New JavaScript API

New APIs are registered at build time — there is no dynamic module system.
Follow these four steps exactly:

### Step 1 — Write the C++ binding

Create or extend a file in `firmware/src/runtime/`.  Binding files are named
`js_<subsystem>.cpp` / `js_<subsystem>.h`.

Every binding function must be declared `extern "C"` with this signature:

```cpp
// firmware/src/runtime/js_mymodule.cpp
#include "mquickjs.h"

extern "C" JSValue js_myfunc(JSContext *ctx,
                             JSValue   *this_val,
                             int        argc,
                             JSValue   *argv)
{
    // Extract arguments:  JS_ToInt32(ctx, &n, argv[0]);
    // Return a value:     return JS_NewInt32(ctx, result);
    // Return undefined:   return JS_UNDEFINED;
    // Return an error:    return JS_ThrowTypeError(ctx, "bad arg");
}
```

Key mquickjs helpers:

| Task | Helper |
|------|--------|
| Read int32 | `JS_ToInt32(ctx, &n, argv[i])` |
| Read float | `JS_ToFloat64(ctx, &d, argv[i])` |
| Read string | `const char *s = JS_ToCString(ctx, argv[i]);` … `JS_FreeCString(ctx, s);` |
| New int | `JS_NewInt32(ctx, n)` |
| New float | `JS_NewFloat64(ctx, d)` |
| New string | `JS_NewString(ctx, cstr)` |
| New bool | `JS_NewBool(ctx, b)` |
| New object | `JS_NewObject(ctx)` then `JS_SetPropertyStr(ctx, obj, "key", val)` |

### Step 2 — Register the function

Open `firmware/scripts/x4_stdlib.c` and add an entry to the relevant property
table using `JS_CFUNC_DEF`:

```c
// In the appropriate funcs[] array:
JS_CFUNC_DEF("myfunc", 1, js_myfunc),   // "myfunc" = JS name, 1 = arg count
```

If the API belongs to a new object (e.g. `mymodule.*`), add a new property table
and register it in `js_init_globals()` following the pattern of the existing modules.

### Step 3 — Regenerate headers

```bash
cd firmware
bash scripts/fetch_mquickjs.sh
```

This regenerates `lib/mquickjs/src/x4_stdlib.h`.  **Never edit that file by hand.**

### Step 4 — Build

```bash
cd firmware && pio run
```

Fix any compiler errors, then verify the new function works on-device.

### Step 5 — Update documentation

Every new API must be documented in three places:

1. **`apps/README.md`** — Add a row to the appropriate `### module.*` table with
   the call signature and a description.

2. **`README.md`** — Add the method to the matching row in the
   "JavaScript API at a Glance" table.

3. **`firmware/README.md`** — If a new binding file was created, add it to the
   Repository Layout section.

---

## Adding a New Example App or Face

1. Write your `.js` file following the [JavaScript Style Guide](#javascript-style-guide).
2. Place apps in `apps/` and faces in `apps/faces/`.
3. Add an entry to the "Example Apps" or "Example Faces" table in `apps/README.md`.
4. Add an entry to the "Example Apps" or "Clock Faces" table in `README.md`.
5. If appropriate, add a `cp` line to `scripts/deploy_sd.sh`.

---

## Firmware C++ Style Guide

- **Pin numbers** — always use the named constants from `firmware/src/bsp/x4_pins.h`.
  Never hard-code GPIO numbers in driver or runtime code.
- **Naming** — snake_case for functions and variables; `UPPER_SNAKE_CASE` for macros
  and constants; `CamelCase` for types and classes.
- **Binding files** — named `js_<subsystem>.cpp` / `js_<subsystem>.h`; binding
  functions prefixed `js_` (e.g. `js_display_clear`).
- **Serial output** — use `Serial.printf()` / `Serial.println()` for firmware log
  messages; prefix with the module name in square brackets, e.g. `[display]`.
- **SPI bus** — the shared SPI instance `g_spi` (defined in `main.cpp`) must be
  passed to `display_init()` and `sdcard_init()`.  Do not create a second SPI bus.
- **No dynamic allocation in drivers** — prefer static buffers; the heap is small.
- **`extern "C"`** — all JS binding functions must be declared `extern "C"` so that
  the generated `x4_stdlib.h` (a C header) can reference them from C++ translation
  units.

---

## JavaScript Style Guide

- **ES5 syntax** — use `var` (not `let`/`const`).  mquickjs supports ES5 plus a
  limited subset of ES6 (arrow functions work; classes and generators do not).
- **Indentation** — two spaces.
- **Line length** — 80 characters recommended.
- **Timing** — use `system.millis()` for elapsed-time calculations.  Use
  `system.time()` only when wall-clock time is needed (requires NTP sync or
  `system.setTime()`).
- **Display updates** — call `display.clear()` + `display.refresh()` exactly once
  in `setup()`.  Use `display.partialRefresh()` for all subsequent updates.
- **Memory** — call `gc()` after processing each file chunk or building a large
  string.  Do not accumulate strings in a loop.
- **JSON parsing** — always check that the response body is non-empty before
  calling `JSON.parse()`.
- **Time padding** — use a local `pad2` helper:
  ```js
  function pad2(n) { return n < 10 ? "0" + n : "" + n; }
  ```

---

## Documentation Standards

- The three README files (`README.md`, `firmware/README.md`, `apps/README.md`) are
  the canonical documentation.  Keep them accurate and in sync.
- Use tables for API references — one row per function with call signature and
  description.
- Code examples should be minimal and correct — test them on-device or in the
  firmware's serial output before committing.
- Do not leave section headings without body content, or body content without a
  heading.

---

## Testing

There are no automated test suites.  All validation is manual:

1. **Build passes** — `cd firmware && pio run` must complete with no errors or
   warnings (other than pre-existing `-Wnarrowing` suppressions in `platformio.ini`).
2. **Flash and observe** — flash the firmware, open `pio device monitor`, and
   verify the boot sequence prints as expected and the new functionality works.
3. **JS app test** — copy the affected `.js` file to the SD card, reboot the
   device, and verify on-screen behaviour and serial log output.

A contribution is ready for review when:
- The firmware builds cleanly (`pio run`).
- The new feature or fix works correctly on-device.
- Documentation has been updated in all three README files where relevant.
