// seconds.js — Seconds clock face for Xteink X4
//
// Place at /faces/seconds.js on the SD card.
//
// Redraws every second to display HH:MM:SS in a large font.
// Useful when seconds precision matters.  The per-second partial refresh
// takes ~420 ms — this is normal for an e-ink display.
//
// The built-in clock app calls draw() once per second.

var _lastSec = -1;

function setup() {
  display.clear();
  display.refresh();
}

function draw() {
  var ms       = system.millis();
  var totalSec = Math.floor(ms / 1000);
  var h        = Math.floor(totalSec / 3600) % 24;
  var m        = Math.floor(totalSec / 60)   % 60;
  var s        = totalSec % 60;

  if (s === _lastSec) return;
  _lastSec = s;

  var timeStr = pad2(h) + ":" + pad2(m) + ":" + pad2(s);

  display.clear();
  display.print(60, 220, timeStr, 4);
  display.partialRefresh();

  gc();
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------
function pad2(n) {
  return n < 10 ? "0" + n : "" + n;
}
