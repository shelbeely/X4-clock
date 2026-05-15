// status.js — Status board clock face for Xteink X4
//
// Place at /faces/status.js on the SD card.
//
// Information-dense layout:
//   • Double border decoration
//   • Large HH:MM centred in the upper half
//   • Horizontal rule dividing clock from data strip
//   • Day counter and battery percentage in the lower section
//
// Redraws once per minute to conserve e-ink partial refresh cycles.

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
  var day      = Math.floor(totalSec / 86400);
  var bat      = system.battery();

  if (m === _lastMinute) return;
  _lastMinute = m;

  display.clear();

  // Outer and inner border
  display.drawRect(8,  8,  784, 464, false);
  display.drawRect(16, 16, 768, 448, false);

  // Large time in the upper half
  var timeStr = pad2(h) + ":" + pad2(m);
  display.print(220, 190, timeStr, 4);

  // Horizontal rule between clock and data strip
  display.drawRect(16, 295, 768, 4, true);

  // Day counter — left side of data strip
  display.print(40,  360, "Day",       2);
  display.print(190, 360, "" + day,    2);

  // Battery percentage — right side of data strip
  display.print(430, 360, "Battery",   2);
  display.print(700, 360, bat + "%",   2);

  display.partialRefresh();
  gc();
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------
function pad2(n) {
  return n < 10 ? "0" + n : "" + n;
}
