# X4-clock

Minimal repository setup for an **Xteink X4 base firmware** that can later host small watch-style apps.

## Goal

Set up the project as the foundation for an **X4 "Pebble watch" style firmware** with:

- an Xteink X4 hardware target
- a base firmware layer for future apps
- support for **all hardware buttons**
- a wall-clock friendly starting point

This repository intentionally does **not** include firmware implementation yet. It only establishes the initial structure and scope.

## Hardware target

- Device: **Xteink X4**
- MCU family: **ESP32-C3**
- Display class: **4.3\" E-Ink**
- Inputs: **physical hardware buttons only**

Reference material:

- https://github.com/sunwoods/Xteink-X4
- https://www.good-display.com/product/457.html

## Minimal firmware scope

The initial base firmware should eventually provide:

1. board bring-up for the X4
2. display initialization and screen refresh support
3. hardware button input handling for every physical button
4. a small app-facing layer so simple apps can be added without changing board support code
5. a default clock-style app shell suitable for wall-clock use

## Repository layout

```text
.
├── apps/
│   └── README.md
├── firmware/
│   └── README.md
└── README.md
```

- `firmware/` is reserved for the future board/base firmware layer
- `apps/` is reserved for future app modules that run on top of the base firmware

## Notes

- No firmware code has been added yet.
- No build, lint, or test tooling existed in the repository when this setup was created.
- The next implementation step should be choosing the firmware build system and defining the X4 board support package around the documented ESP32-C3 + E-Ink + button hardware.
