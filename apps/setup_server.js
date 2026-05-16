// setup_server.js — Web configuration server app for Xteink X4
//
// Place this file on the SD card at /apps/setup_server.js
//
// Starts the X4 in WiFi AP mode and serves a web page at http://192.168.4.1/
// where you can configure:
//   - WiFi SSID and password (/config/wifi.json)
//   - Screen rotation (0-3)
//   - Loop refresh interval (ms)
//   - Timezone UTC offset (hours)
//   - OpenWeatherMap API key and city (/config/settings.json)
//
// After saving, the device reboots (deep-sleep / wake) with the new settings.
//
// Usage:
//   1. Copy this app to the SD card
//   2. Launch it from the app picker
//   3. Connect your phone/PC to WiFi network "X4-Setup" (password: "configure")
//   4. Open http://192.168.4.1/ in your browser
//   5. Fill in the form and press Save
//
// Button map:
//   POWER — exit (deep sleep)

// ---------------------------------------------------------------------------
// HTML settings page — served from C constant to avoid SD dependency
// ---------------------------------------------------------------------------

var PAGE_HTML = '<!DOCTYPE html><html><head>' +
  '<meta name="viewport" content="width=device-width,initial-scale=1">' +
  '<title>X4 Setup</title>' +
  '<style>body{font-family:sans-serif;max-width:480px;margin:20px auto;padding:0 16px}' +
  'h1{font-size:1.4em}label{display:block;margin-top:12px;font-weight:bold}' +
  'input{width:100%;padding:6px;box-sizing:border-box;font-size:1em;margin-top:4px}' +
  'button{margin-top:20px;padding:10px 24px;font-size:1em;background:#1a73e8;color:#fff;border:none;border-radius:4px;cursor:pointer}' +
  '#msg{margin-top:12px;color:green}</style></head><body>' +
  '<h1>&#9201; Xteink X4 Setup</h1><form id="f">' +
  '<h2>WiFi</h2>' +
  '<label>SSID<input name="ssid" id="ssid"></label>' +
  '<label>Password<input name="pass" id="pass" type="password"></label>' +
  '<h2>Display</h2>' +
  '<label>Rotation (0=landscape 1=portrait 2=rev.landscape 3=rev.portrait)<input name="rotation" id="rotation" type="number" min="0" max="3" value="0"></label>' +
  '<label>Refresh interval (ms, default 20)<input name="refresh_ms" id="refresh_ms" type="number" min="1" max="60000" value="20"></label>' +
  '<h2>Time</h2>' +
  '<label>Timezone UTC offset (hours, e.g. -5 or 2)<input name="tz_offset" id="tz_offset" type="number" min="-12" max="14" value="0"></label>' +
  '<h2>Weather (OpenWeatherMap)</h2>' +
  '<label>API Key<input name="owm_key" id="owm_key"></label>' +
  '<label>City<input name="city" id="city" value="London"></label>' +
  '<br><button type="submit">Save &amp; Reboot</button>' +
  '</form><div id="msg"></div>' +
  '<script>' +
  'fetch("/api/current").then(r=>r.json()).then(d=>{' +
  '["ssid","rotation","refresh_ms","tz_offset","owm_key","city"].forEach(k=>{' +
  'var el=document.getElementById(k);if(el&&d[k]!==undefined)el.value=d[k];});});' +
  'document.getElementById("f").onsubmit=function(e){' +
  'e.preventDefault();' +
  'var data={};new FormData(e.target).forEach(function(v,k){data[k]=v;});' +
  'fetch("/api/save",{method:"POST",headers:{"Content-Type":"application/json"},' +
  'body:JSON.stringify(data)}).then(r=>r.text()).then(t=>{' +
  'document.getElementById("msg").textContent=t;});};' +
  '</script></body></html>';

// ---------------------------------------------------------------------------
// Current settings (read from SD)
// ---------------------------------------------------------------------------

var _settings = {
  ssid:        "",
  rotation:    0,
  refresh_ms:  20,
  tz_offset:   0,
  owm_key:     "",
  city:        "London"
};

function loadCurrentSettings() {
  // Load WiFi SSID
  if (fs.exists("/config/wifi.json")) {
    var h = fs.open("/config/wifi.json", "r");
    if (h >= 0) {
      var raw = fs.read(h, 512);
      fs.close(h);
      var ssid = extractString(raw, '"ssid":"');
      if (ssid !== "") _settings.ssid = ssid;
    }
  }
  // Load settings.json
  if (fs.exists("/config/settings.json")) {
    var h2 = fs.open("/config/settings.json", "r");
    if (h2 >= 0) {
      var raw2 = fs.read(h2, 1024);
      fs.close(h2);
      var rotation   = extractNumber(raw2, '"rotation":');
      var refresh_ms = extractNumber(raw2, '"refresh_ms":');
      var tz_offset  = extractNumber(raw2, '"tz_offset":');
      var owm_key    = extractString(raw2, '"owm_key":"');
      var city       = extractString(raw2, '"city":"');
      if (rotation !== 0)   _settings.rotation   = rotation;
      if (refresh_ms !== 0) _settings.refresh_ms = refresh_ms;
      _settings.tz_offset = tz_offset;
      if (owm_key !== "") _settings.owm_key = owm_key;
      if (city !== "")    _settings.city    = city;
    }
  }
  gc();
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

function setup() {
  loadCurrentSettings();

  display.clear();
  display.print(80, 160, "Starting AP...", 2);
  display.refresh();

  var ok = wifi.startAP("X4-Setup", "configure");
  if (!ok) {
    display.clear();
    display.print(200, 200, "AP failed!", 2);
    display.partialRefresh();
    return;
  }

  server.begin(80);

  // Serve the settings page
  server.onRequest("/", function(method, body) {
    server.send(200, "text/html", PAGE_HTML);
  });

  // Return current settings as JSON for the page to pre-populate fields
  server.onRequest("/api/current", function(method, body) {
    var json = JSON.stringify(_settings);
    server.send(200, "application/json", json);
  });

  // Save new settings
  server.onRequest("/api/save", function(method, body) {
    if (method !== "POST") {
      server.send(405, "text/plain", "Method Not Allowed");
      return;
    }
    var data;
    try {
      data = JSON.parse(body);
    } catch (e) {
      server.send(400, "text/plain", "Bad JSON");
      return;
    }

    // Write wifi.json — use JSON.stringify to safely escape values
    if (data.ssid !== undefined && data.ssid !== "") {
      var wj = JSON.stringify({ ssid: "" + data.ssid, pass: "" + (data.pass || "") });
      ensureDir("/config");
      var wh = fs.open("/config/wifi.json", "w");
      if (wh >= 0) { fs.write(wh, wj); fs.close(wh); }
    }

    // Write settings.json
    var sj = JSON.stringify({
      rotation:   parseInt(data.rotation   || 0),
      refresh_ms: parseInt(data.refresh_ms || 20),
      tz_offset:  parseInt(data.tz_offset  || 0),
      owm_key:    "" + (data.owm_key || ""),
      city:       "" + (data.city    || "London")
    });
    ensureDir("/config");
    var sh = fs.open("/config/settings.json", "w");
    if (sh >= 0) { fs.write(sh, sj); fs.close(sh); }

    server.send(200, "text/plain", "Saved! Rebooting in 3 seconds...");
    gc();
  });

  var ip = wifi.ip();
  display.clear();
  display.print(20, 80,  "AP: X4-Setup", 2);
  display.print(20, 150, "Pass: configure", 1);
  display.print(20, 210, "Open: http://" + ip + "/", 2);
  display.print(20, 310, "Configure WiFi, display,", 1);
  display.print(20, 340, "weather, and timezone.", 1);
  display.print(20, 440, "POWER: sleep", 1);
  display.partialRefresh();

  system.setRefreshInterval(100);  // poll server frequently
}

// ---------------------------------------------------------------------------
// Loop — handle incoming HTTP requests
// ---------------------------------------------------------------------------

function loop() {
  server.handleClient();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function ensureDir(path) {
  // No mkdir in fs API; directory must already exist.
  // The user is expected to create /config on the SD card.
}

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
  while (start < text.length && text.charAt(start) === " ") start++;
  var end = start;
  while (end < text.length) {
    var c = text.charAt(end);
    if ((c >= "0" && c <= "9") || c === "." || c === "-") end++;
    else break;
  }
  if (end === start) return 0;
  return parseFloat(text.substring(start, end));
}

// ---------------------------------------------------------------------------
// Button handler
// ---------------------------------------------------------------------------

input.onButton(function(btn) {
  if (btn === "power") {
    server.stop();
    wifi.disconnect();
    display.clear();
    display.print(280, 220, "Sleeping...", 2);
    display.partialRefresh();
    system.sleep(0);
  }
});
