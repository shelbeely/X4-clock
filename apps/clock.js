// clock.js — Default wall-clock app for Xteink X4
//
// Place this file on the SD card at /apps/clock.js
//
// Time is relative to boot (no RTC hardware).  Add system.setTime() + NTP
// as a follow-on for real wall-clock time.
//
// Demonstrates: display API, system API, input API, gc() best-practice

var lastMinute = -1;

function setup() {
  display.clear();
  display.refresh();
}

function loop() {
  var ms           = system.millis();
  var totalSeconds = Math.floor(ms / 1000);
  var hours        = Math.floor(totalSeconds / 3600) % 24;
  var minutes      = Math.floor(totalSeconds / 60)   % 60;
  var seconds      = totalSeconds % 60;

  // Redraw only when the minute changes (saves EPD partial refresh cycles)
  if (minutes !== lastMinute) {
    lastMinute = minutes;

    display.clear();

    // Large HH:MM centred on 800×480
    var timeStr = pad2(hours) + ":" + pad2(minutes);
    display.print(130, 180, timeStr, 4);

    // Day counter below the time
    var dayStr = "Day " + Math.floor(totalSeconds / 86400);
    display.print(320, 320, dayStr, 2);

    // Battery status bottom-left
    var batStr = "Batt: " + system.battery() + "%";
    display.print(20, 450, batStr, 1);

    display.partialRefresh();

    // Free temporary strings created above
    gc();
  }
}

function onButton(btn) {
  if (btn === "confirm") {
    var batStr = "Batt: " + system.battery() + "%";
    display.print(20, 450, batStr, 1);
    display.partialRefresh();
  } else if (btn === "power") {
    // Long-press → deep sleep (handled by buttons driver, forwarded here)
    display.clear();
    display.print(280, 220, "Sleeping...", 2);
    display.partialRefresh();
    system.sleep(0);   // wake only on power button
  }
}

// Register the button handler
input.onButton(onButton);

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------
function pad2(n) {
  return n < 10 ? "0" + n : "" + n;
}
