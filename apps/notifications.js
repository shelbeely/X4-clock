// notifications.js — Notification display app for Xteink X4
//
// Place this file on the SD card at /apps/notifications.js
//
// Reads pending notifications from /notifications/pending.json on the SD card
// and displays them on the e-ink screen.  The device can deep-sleep between
// notifications and wake up exactly when the next event is due.
//
// Notification file format (UTF-8 JSON array):
//   [
//     {"title":"Team standup","time":540,"body":"Daily at 09:00"},
//     {"title":"Lunch",       "time":720,"body":"12:00 — cafeteria"},
//     {"title":"Code review", "time":900,"body":"PR #42 needs sign-off"}
//   ]
//
//   "time" is minutes since midnight (e.g. 540 = 09:00, 720 = 12:00).
//   The device has no RTC so "minutes since boot" is used as a proxy.
//   Sync /notifications/pending.json from your phone or PC before use.
//
// Button map:
//   RIGHT / LEFT — next / previous notification
//   CONFIRM      — dismiss current notification
//   POWER        — deep sleep
//
// To sync notifications from a desktop machine, copy the JSON file to the SD:
//   cp notifications.json <SD>/notifications/pending.json

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

var _items   = [];     // parsed notification array
var _idx     = 0;      // currently displayed index
var _loaded  = false;  // have we loaded notifications?

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

function setup() {
  display.clear();
  display.print(200, 220, "Notifications", 3);
  display.refresh();
  loadNotifications();
  if (_items.length === 0) {
    drawEmpty();
  } else {
    _idx = 0;
    drawItem(_idx);
  }
}

// ---------------------------------------------------------------------------
// Loop — nothing to poll; all work is event-driven
// ---------------------------------------------------------------------------

function loop() {
  // nothing
}

// ---------------------------------------------------------------------------
// Load /notifications/pending.json
// ---------------------------------------------------------------------------

function loadNotifications() {
  _items = [];
  _loaded = false;

  if (!fs.exists("/notifications/pending.json")) return;

  var sz = fs.size("/notifications/pending.json");
  if (sz <= 0) return;

  // Read file in one shot (capped at 4 KB per fs.read call)
  var h = fs.open("/notifications/pending.json", "r");
  if (h < 0) return;

  var raw = "";
  var chunk;
  while ((chunk = fs.read(h, 4096)) !== "") {
    raw = raw + chunk;
    gc();
  }
  fs.close(h);

  // Parse JSON array
  var arr;
  try {
    arr = JSON.parse(raw);
  } catch (e) {
    system.log("notifications: JSON parse error");
    return;
  }
  gc();

  if (!arr || arr.length === undefined) return;

  for (var i = 0; i < arr.length; i++) {
    _items.push(arr[i]);
  }
  _loaded = true;
  gc();
}

// ---------------------------------------------------------------------------
// Draw helpers
// ---------------------------------------------------------------------------

function drawEmpty() {
  display.clear();
  display.print(200, 200, "No notifications", 2);
  display.print(20, 440, "Place pending.json at /notifications/", 1);
  display.partialRefresh();
}

function drawItem(idx) {
  if (idx < 0 || idx >= _items.length) return;
  var item = _items[idx];

  display.clear();

  // Header: index / total
  var header = (idx + 1) + " / " + _items.length + " notifications";
  display.print(20, 40, header, 1);

  // Title (large)
  var title = item.title !== undefined ? item.title : "(no title)";
  display.print(20, 100, title, 3);

  // Time (if provided)
  if (item.time !== undefined) {
    var mins = item.time;
    var h    = Math.floor(mins / 60) % 24;
    var m    = mins % 60;
    var tStr = pad2(h) + ":" + pad2(m);
    display.print(20, 210, tStr, 2);
  }

  // Body
  if (item.body !== undefined) {
    display.print(20, 290, item.body, 1);
  }

  // Battery and footer
  var batt = "Batt: " + system.battery() + "%";
  display.print(20, 450, batt + "   Left/Right: scroll   Confirm: dismiss", 1);

  display.partialRefresh();
  gc();
}

// ---------------------------------------------------------------------------
// Button handler
// ---------------------------------------------------------------------------

input.onButton(function(btn) {
  if (btn === "right") {
    if (_idx < _items.length - 1) {
      _idx++;
      drawItem(_idx);
    }
  } else if (btn === "left") {
    if (_idx > 0) {
      _idx--;
      drawItem(_idx);
    }
  } else if (btn === "confirm") {
    // Dismiss: remove current item from array
    _items.splice(_idx, 1);
    gc();
    if (_items.length === 0) {
      drawEmpty();
    } else {
      if (_idx >= _items.length) _idx = _items.length - 1;
      drawItem(_idx);
    }
    // Write updated list back to SD
    saveNotifications();
  } else if (btn === "power") {
    display.clear();
    display.print(280, 220, "Sleeping...", 2);
    display.partialRefresh();
    system.sleep(0);
  }
});

// ---------------------------------------------------------------------------
// Save updated notification list (after dismissal)
// ---------------------------------------------------------------------------

function saveNotifications() {
  if (!fs.exists("/notifications")) return;
  var h = fs.open("/notifications/pending.json", "w");
  if (h < 0) return;
  var json = JSON.stringify(_items);
  fs.write(h, json);
  fs.close(h);
  gc();
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

function pad2(n) {
  return n < 10 ? "0" + n : "" + n;
}
