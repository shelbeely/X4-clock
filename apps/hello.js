// hello.js — Hello World starter app for Xteink X4
//
// Place this file on the SD card at /apps/hello.js
//
// The simplest possible app — a static greeting with two button actions.
// Use this as a starting point for your own apps.
//
// Demonstrates: display API, input.onButton(), system.battery(), system.sleep()

function setup() {
  display.clear();
  display.print(160, 180, "Hello, X4!", 3);
  display.print(20,  390, "Confirm: show battery", 1);
  display.print(20,  420, "Power:   sleep", 1);
  display.refresh();
}

function loop() {
  // Nothing to update every tick — this app is entirely event-driven.
}

input.onButton(function(btn) {
  if (btn === "confirm") {
    // Overlay the battery reading in the lower-left corner
    display.print(20, 460, "Batt: " + system.battery() + "%   ", 1);
    display.partialRefresh();
  } else if (btn === "power") {
    display.clear();
    display.print(280, 220, "Sleeping...", 2);
    display.partialRefresh();
    system.sleep(0);   // wake only on power button
  }
});
