// stopwatch.js — Stopwatch app for Xteink X4
//
// Place this file on the SD card at /apps/stopwatch.js
//
// Demonstrates: system.millis() for elapsed timing, input.onButton() for
// start/stop/reset, display.partialRefresh() for per-second screen updates.
//
// Button map:
//   CONFIRM — start / stop
//   BACK    — reset (stops and clears time)
//   POWER   — deep sleep

var _running = false;      // is the stopwatch currently running?
var _startMs = 0;          // millis() value when the last Start was pressed
var _elapsed = 0;          // accumulated milliseconds before current run
var _lastDrawnSec = -1;    // last second value drawn on screen

function setup() {
  display.clear();
  display.print(20, 80,  "Stopwatch",                        3);
  display.print(20, 390, "Confirm: start / stop",            1);
  display.print(20, 420, "Back: reset   Power: sleep",       1);
  display.refresh();

  drawTime(0);
}

function loop() {
  var nowMs  = _running ? (_elapsed + system.millis() - _startMs) : _elapsed;
  var nowSec = Math.floor(nowMs / 1000);

  if (nowSec !== _lastDrawnSec) {
    _lastDrawnSec = nowSec;
    drawTime(nowMs);
  }
}

// Draw the elapsed time and running/stopped status onto the display.
function drawTime(ms) {
  var totalSec = Math.floor(ms / 1000);
  var h = Math.floor(totalSec / 3600);
  var m = Math.floor(totalSec / 60) % 60;
  var s = totalSec % 60;

  var timeStr   = pad2(h) + ":" + pad2(m) + ":" + pad2(s);
  var statusStr = _running ? "Running  " : "Stopped  ";

  display.clear();
  display.print(20,  80,  "Stopwatch",                   3);
  display.print(80,  250, timeStr,                       4);
  display.print(20,  340, statusStr,                     2);
  display.print(20,  390, "Confirm: start / stop",       1);
  display.print(20,  420, "Back: reset   Power: sleep",  1);
  display.partialRefresh();
  gc();
}

input.onButton(function(btn) {
  if (btn === "confirm") {
    if (_running) {
      // Stop: freeze elapsed time
      _elapsed += system.millis() - _startMs;
      _running  = false;
    } else {
      // Start: record the current millis() anchor
      _startMs = system.millis();
      _running = true;
    }
    _lastDrawnSec = -1;   // force immediate redraw

  } else if (btn === "back") {
    // Reset: stop and clear
    _running      = false;
    _elapsed      = 0;
    _lastDrawnSec = -1;

  } else if (btn === "power") {
    display.clear();
    display.print(280, 220, "Sleeping...", 2);
    display.partialRefresh();
    system.sleep(0);
  }
});

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------
function pad2(n) {
  return n < 10 ? "0" + n : "" + n;
}
