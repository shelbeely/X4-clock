// weather.js — Weather display app for Xteink X4
//
// Place this file on the SD card at /apps/weather.js
//
// Fetches current weather from the OpenWeatherMap API and displays:
//   - City name and country
//   - Temperature (°C)
//   - Weather condition description
//   - Humidity %
//   - Last update time (minutes since boot)
//
// Requirements:
//   - WiFi credentials in /config/wifi.json: {"ssid":"...","pass":"..."}
//   - OpenWeatherMap API key and city in /config/settings.json:
//       {"owm_key":"YOUR_API_KEY","city":"London","tz_offset":0}
//
// Button map:
//   CONFIRM — force an immediate weather refresh
//   POWER   — deep sleep

var REFRESH_INTERVAL_MS = 600000;  // refresh weather every 10 minutes
var lastRefresh   = -REFRESH_INTERVAL_MS;  // force fetch on first loop()
var lastDisplay   = "";            // last drawn status line
var connected     = false;
var owmKey        = "";
var city          = "";
var tzOffset      = 0;            // UTC offset in hours (for display only)

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

function setup() {
  display.clear();
  display.print(200, 220, "Weather", 3);
  display.print(260, 310, "Loading...", 2);
  display.refresh();

  loadConfig();

  if (owmKey === "" || city === "") {
    showError("No config found.", "/config/settings.json required");
    return;
  }

  display.clear();
  display.print(200, 220, "Connecting...", 2);
  display.partialRefresh();

  connected = wifi.connect();
  if (!connected) {
    showError("WiFi failed.", "Check /config/wifi.json");
    return;
  }

  display.clear();
  display.print(220, 220, "Connected!", 2);
  display.partialRefresh();
}

// ---------------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------------

function loop() {
  var now = system.millis();
  if (now - lastRefresh >= REFRESH_INTERVAL_MS) {
    lastRefresh = now;
    fetchWeather();
  }
}

// ---------------------------------------------------------------------------
// Fetch and display weather
// ---------------------------------------------------------------------------

function fetchWeather() {
  if (!connected) return;

  var url = "http://api.openweathermap.org/data/2.5/weather?q=" +
            city + "&appid=" + owmKey + "&units=metric";

  var body = http.get(url);
  gc();

  if (body === "") {
    showError("Fetch failed", "Check API key and network");
    return;
  }

  drawWeather(body);
  gc();
}

function drawWeather(json) {
  // Extract fields with simple string search (no full JSON parser needed for
  // flat primitive values).
  var temp     = extractNumber(json, '"temp":');
  var humidity = extractNumber(json, '"humidity":');
  var desc     = extractString(json, '"description":"');
  var name     = extractString(json, '"name":"');
  var country  = extractString(json, '"country":"');

  display.clear();

  // City and country heading
  var heading = name + ", " + country;
  display.print(20, 60, heading, 2);

  // Large temperature
  var tempStr = Math.round(temp) + " C";
  display.print(80, 160, tempStr, 4);

  // Condition description
  display.print(20, 300, desc, 2);

  // Humidity
  var humStr = "Humidity: " + Math.round(humidity) + "%";
  display.print(20, 370, humStr, 2);

  // Battery and last-updated footer
  var mins = Math.floor(system.millis() / 60000);
  var footer = "Updated: " + mins + " min  Batt: " + system.battery() + "%";
  display.print(20, 450, footer, 1);

  display.partialRefresh();
  gc();
}

// ---------------------------------------------------------------------------
// Error display
// ---------------------------------------------------------------------------

function showError(line1, line2) {
  display.clear();
  display.print(20, 200, line1, 2);
  display.print(20, 270, line2, 1);
  display.print(20, 440, "CONFIRM: retry   POWER: sleep", 1);
  display.partialRefresh();
}

// ---------------------------------------------------------------------------
// Config loading
// ---------------------------------------------------------------------------

function loadConfig() {
  var sz = fs.size("/config/settings.json");
  if (sz <= 0) return;

  var h = fs.open("/config/settings.json", "r");
  if (h < 0) return;
  var raw = fs.read(h, 4096);
  fs.close(h);

  owmKey   = extractString(raw, '"owm_key":"');
  city     = extractString(raw, '"city":"');
  var off  = extractNumber(raw, '"tz_offset":');
  if (off !== null) tzOffset = off;
  gc();
}

// ---------------------------------------------------------------------------
// Simple JSON field extractors (avoid full JSON.parse overhead)
// ---------------------------------------------------------------------------

function extractString(text, key) {
  var idx = text.indexOf(key);
  if (idx < 0) return "";
  var start = idx + key.length;
  var end   = text.indexOf('"', start);
  if (end < 0) return "";
  return text.substring(start, end);
}

function extractNumber(text, key) {
  var idx = text.indexOf(key);
  if (idx < 0) return 0;
  var start = idx + key.length;
  // skip whitespace
  while (start < text.length && text.charAt(start) === " ") start++;
  var end = start;
  while (end < text.length) {
    var c = text.charAt(end);
    if ((c >= "0" && c <= "9") || c === "." || c === "-") {
      end++;
    } else {
      break;
    }
  }
  if (end === start) return 0;
  return parseFloat(text.substring(start, end));
}

// ---------------------------------------------------------------------------
// Button handler
// ---------------------------------------------------------------------------

input.onButton(function(btn) {
  if (btn === "confirm") {
    lastRefresh = -REFRESH_INTERVAL_MS;  // force refresh next loop
  } else if (btn === "power") {
    display.clear();
    display.print(280, 220, "Sleeping...", 2);
    display.partialRefresh();
    system.sleep(0);
  }
});
