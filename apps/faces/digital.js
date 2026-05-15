// digital.js — Digital clock face for Xteink X4
//
// Place at /faces/digital.js on the SD card.
//
// The built-in clock app calls draw() every second.
// Time is relative to boot (no RTC); add NTP/RTC support via system.setTime()
// when that API is available.
//
// This face mirrors the built-in C++ digital face and serves as a reference.

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

  // Redraw only when the minute changes (conserves EPD partial refresh cycles)
  if (m === _lastMinute) return;
  _lastMinute = m;

  var timeStr = pad2(h) + ":" + pad2(m);
  var dayStr  = "Day " + day;
  var batStr  = "Batt: " + bat + "%";

  display.clear();
  display.print(120, 180, timeStr, 4);
  display.print(300, 320, dayStr,  2);
  display.print(20,  450, batStr,  1);
  display.partialRefresh();

  gc();
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------
function pad2(n) {
  return n < 10 ? "0" + n : "" + n;
}
