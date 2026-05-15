// countdown.js — Countdown timer app for Xteink X4
//
// Place this file on the SD card at /apps/countdown.js
//
// Set a duration with LEFT/RIGHT, then start with CONFIRM.
// The last used duration is saved to /apps/countdown.cfg so it persists
// across reboots.
//
// Demonstrates: button-based state input, millis() countdown math,
//               fs read/write for persistent settings.
//
// Button map:
//   RIGHT   — increase duration by 1 minute (setup mode only)
//   LEFT    — decrease duration by 1 minute (setup mode only, min 1 min)
//   CONFIRM — start / stop
//   BACK    — reset (stop and return to setup mode)
//   POWER   — deep sleep

var CFG_PATH = "/apps/countdown.cfg";

// Duration in milliseconds (default 5 minutes)
var _durationMs  = 5 * 60 * 1000;
// millis() when the countdown started
var _startMs     = 0;
// accumulated pause time (so we can resume correctly)
var _pausedMs    = 0;
// true while counting down
var _running     = false;
// true once the countdown has finished
var _finished    = false;
// last second remaining drawn on screen (-1 = force redraw)
var _lastDrawnSec = -1;

// -----------------------------------------------------------------------
// Persistent settings
// -----------------------------------------------------------------------

function loadConfig() {
  if (!fs.exists(CFG_PATH)) return;
  var h = fs.open(CFG_PATH, "r");
  if (h < 0) return;
  var data = fs.read(h, 32);
  fs.close(h);
  var v = parseInt(data, 10);
  if (v > 0) _durationMs = v * 60 * 1000;
}

function saveConfig() {
  var h = fs.open(CFG_PATH, "w");
  if (h < 0) return;
  fs.write(h, "" + Math.floor(_durationMs / 60000));
  fs.close(h);
}

// -----------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------

function setup() {
  loadConfig();
  _pausedMs = _durationMs;   // start paused at full duration
  redraw();
}

function loop() {
  if (!_running) return;

  var remaining = _pausedMs - (system.millis() - _startMs);
  if (remaining < 0) remaining = 0;

  var remSec = Math.ceil(remaining / 1000);
  if (remSec === _lastDrawnSec) return;
  _lastDrawnSec = remSec;

  if (remaining === 0 && !_finished) {
    _finished = true;
    _running  = false;
    drawFinished();
  } else if (!_finished) {
    drawCountdown(remaining);
  }
}

// -----------------------------------------------------------------------
// Display
// -----------------------------------------------------------------------

function redraw() {
  if (_finished) {
    drawFinished();
  } else if (_running) {
    var remaining = _pausedMs - (system.millis() - _startMs);
    if (remaining < 0) remaining = 0;
    drawCountdown(remaining);
  } else {
    drawSetup();
  }
}

function drawSetup() {
  var mins = Math.floor(_durationMs / 60000);
  display.clear();
  display.print(20,  60,  "Countdown Timer",             3);
  display.print(20, 200,  "Duration:",                   2);
  display.print(340, 200, pad2(mins) + ":00",            3);
  display.print(20, 380,  "Left/Right: adjust",          1);
  display.print(20, 410,  "Confirm: start",              1);
  display.print(20, 440,  "Power: sleep",                1);
  display.partialRefresh();
  gc();
}

function drawCountdown(ms) {
  var totalSec = Math.ceil(ms / 1000);
  var m = Math.floor(totalSec / 60);
  var s = totalSec % 60;

  display.clear();
  display.print(20,  60,  "Countdown Timer",             3);
  display.print(100, 230, pad2(m) + ":" + pad2(s),       4);
  display.print(20,  380, "Confirm: pause",              1);
  display.print(20,  410, "Back: reset   Power: sleep",  1);
  display.partialRefresh();
  gc();
}

function drawFinished() {
  display.clear();
  display.print(20,  60,  "Countdown Timer",  3);
  display.print(200, 230, "Done!",            4);
  display.print(20,  410, "Back: reset   Power: sleep", 1);
  display.partialRefresh();
  gc();
}

// -----------------------------------------------------------------------
// Input
// -----------------------------------------------------------------------

input.onButton(function(btn) {
  if (btn === "right" && !_running && !_finished) {
    _durationMs += 60 * 1000;
    _pausedMs    = _durationMs;
    _lastDrawnSec = -1;
    drawSetup();

  } else if (btn === "left" && !_running && !_finished) {
    if (_durationMs > 60 * 1000) {
      _durationMs -= 60 * 1000;
      _pausedMs    = _durationMs;
    }
    _lastDrawnSec = -1;
    drawSetup();

  } else if (btn === "confirm") {
    if (_finished) return;   // must reset first
    if (_running) {
      // Pause: freeze remaining time
      _pausedMs = _pausedMs - (system.millis() - _startMs);
      if (_pausedMs < 0) _pausedMs = 0;
      _running  = false;
      _lastDrawnSec = -1;
      drawCountdown(_pausedMs);
    } else {
      // Start / resume
      saveConfig();
      _startMs  = system.millis();
      _running  = true;
    }

  } else if (btn === "back") {
    // Reset to setup mode
    _running      = false;
    _finished     = false;
    _pausedMs     = _durationMs;
    _lastDrawnSec = -1;
    drawSetup();

  } else if (btn === "power") {
    display.clear();
    display.print(280, 220, "Sleeping...", 2);
    display.partialRefresh();
    system.sleep(0);
  }
});

// -----------------------------------------------------------------------
// Utility
// -----------------------------------------------------------------------
function pad2(n) {
  return n < 10 ? "0" + n : "" + n;
}
