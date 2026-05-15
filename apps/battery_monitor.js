// battery_monitor.js — Battery monitor app for Xteink X4
//
// Place this file on the SD card at /apps/battery_monitor.js
//
// Displays the current battery percentage as both a number and a visual bar.
// Redraws automatically whenever the percentage changes.
//
// Demonstrates: display.drawRect() for graphics, system.battery(),
//               loop()-based polling, system.sleep()
//
// Button map:
//   CONFIRM — force an immediate refresh
//   POWER   — deep sleep

var _lastPct = -1;   // last percentage drawn; -1 forces first draw

// Display geometry for the battery bar
var BAR_X = 50;
var BAR_Y = 310;
var BAR_W = 700;
var BAR_H = 70;

function setup() {
  display.clear();
  display.refresh();
}

function loop() {
  var pct = system.battery();
  if (pct === _lastPct) return;   // nothing changed
  _lastPct = pct;
  redraw(pct);
}

function redraw(pct) {
  display.clear();

  // Title
  display.print(20, 60, "Battery Monitor", 3);

  // Large percentage number, centred
  var pctStr = pct + "%";
  display.print(310, 220, pctStr, 4);

  // Filled portion proportional to charge level
  var fillW = Math.floor(BAR_W * pct / 100);
  if (fillW > 0) {
    display.drawRect(BAR_X, BAR_Y, fillW, BAR_H, true);
  }

  // Bar outline drawn last so it is always visible on top of the fill
  display.drawRect(BAR_X, BAR_Y, BAR_W, BAR_H, false);

  // Footer
  display.print(20, 440, "Confirm: refresh   Power: sleep", 1);

  display.partialRefresh();
  gc();
}

input.onButton(function(btn) {
  if (btn === "confirm") {
    _lastPct = -1;   // force redraw on next loop() tick
  } else if (btn === "power") {
    display.clear();
    display.print(280, 220, "Sleeping...", 2);
    display.partialRefresh();
    system.sleep(0);
  }
});
