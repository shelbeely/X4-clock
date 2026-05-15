// roman.js — Roman numeral clock face for Xteink X4
//
// Place at /faces/roman.js on the SD card.
//
// Converts the current hour (1–12) and minute (0–59) into Roman numerals
// and displays them in a large font.  Example: "XI : XLV" for 11:45.
//
// Midnight and noon both display as "XII".  Minutes of zero display as "O"
// (there is no traditional Roman zero; "O" is a common convention here).
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
  var h        = Math.floor(totalSec / 3600) % 12;
  var m        = Math.floor(totalSec / 60)   % 60;

  // 12-hour display: 0 → XII
  if (h === 0) h = 12;

  if (m === _lastMinute) return;
  _lastMinute = m;

  var timeStr = toRoman(h) + " : " + toRoman(m);

  display.clear();
  display.print(20, 60,  "Roman Numeral Clock", 2);
  display.print(40, 250, timeStr,               3);
  display.partialRefresh();

  gc();
}

// ---------------------------------------------------------------------------
// Roman numeral conversion
// ---------------------------------------------------------------------------
function toRoman(n) {
  if (n === 0) return "O";   // zero convention

  var values  = [1000,900,500,400,100,90,50,40,10,9,5,4,1];
  var symbols = ["M","CM","D","CD","C","XC","L","XL","X","IX","V","IV","I"];
  var result  = "";

  for (var i = 0; i < values.length; i++) {
    while (n >= values[i]) {
      result += symbols[i];
      n      -= values[i];
    }
  }
  return result;
}
