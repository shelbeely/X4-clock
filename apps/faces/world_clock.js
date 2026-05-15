// world_clock.js — World clock face for Xteink X4
//
// Place at /faces/world_clock.js on the SD card.
//
// Displays two clocks side by side:
//   LEFT  — local time (boot-relative, same as all other faces)
//   RIGHT — a second timezone at a fixed UTC offset
//
// Edit UTC_OFFSET_HOURS below to match your second timezone.
// For example:
//   -5  → UTC-5  (Eastern Standard Time)
//    0  → UTC+0  (London / GMT)
//   +1  → UTC+1  (Central European Time)
//   +9  → UTC+9  (Japan Standard Time)
//
// Redraws once per minute to conserve e-ink partial refresh cycles.

var UTC_OFFSET_HOURS = 0;   // ← change to your second-zone offset

var _lastTotalMin = -1;

function setup() {
  display.clear();
  display.refresh();
}

function draw() {
  var ms       = system.millis();
  var totalSec = Math.floor(ms / 1000);
  var totalMin = Math.floor(totalSec / 60);

  if (totalMin === _lastTotalMin) return;
  _lastTotalMin = totalMin;

  // Local time (boot-relative)
  var lh = Math.floor(totalSec / 3600) % 24;
  var lm = totalMin % 60;

  // Remote time — apply UTC offset in hours
  var remoteOffsetSec = UTC_OFFSET_HOURS * 3600;
  var remoteSec = totalSec + remoteOffsetSec;
  var rh = Math.floor(remoteSec / 3600) % 24;
  if (rh < 0) rh += 24;
  var rm = Math.floor(remoteSec / 60) % 60;
  if (rm < 0) rm += 60;

  display.clear();

  // Column headers
  display.print(60,  60, "Local",  2);
  display.print(500, 60, "UTC" + (UTC_OFFSET_HOURS >= 0 ? "+" : "") + UTC_OFFSET_HOURS, 2);

  // Vertical divider
  display.drawRect(395, 50, 4, 380, true);

  // Local time
  display.print(40, 240, pad2(lh) + ":" + pad2(lm), 4);

  // Remote time
  display.print(430, 240, pad2(rh) + ":" + pad2(rm), 4);

  display.partialRefresh();
  gc();
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------
function pad2(n) { return n < 10 ? "0" + n : "" + n; }
