// minimal.js — Minimal clock face for Xteink X4
//
// Place at /faces/minimal.js on the SD card.
//
// Displays only the current time in the largest font — nothing else.
// Useful as a distraction-free face or as a simple face template.

var _lastMinute = -1;

function setup() {
  display.clear();
  display.refresh();
}

function draw() {
  var ms       = system.millis();
  var totalSec = Math.floor(ms / 1000);
  var h        = Math.floor(totalSec / 3600) % 24;
  var m        = Math.floor(totalSec / 60)   % 60;

  if (m === _lastMinute) return;
  _lastMinute = m;

  var timeStr = pad2(h) + ":" + pad2(m);

  display.clear();
  display.print(200, 240, timeStr, 4);
  display.partialRefresh();

  gc();
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------
function pad2(n) {
  return n < 10 ? "0" + n : "" + n;
}
