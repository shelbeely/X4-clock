#pragma once
/*
 * wifi_manager.h — WiFi driver for Xteink X4
 *
 * Provides station-mode (client) and AP-mode (access point) WiFi for the
 * ESP32-C3.  Credentials are stored as JSON on the SD card at
 * /config/wifi.json: {"ssid":"...","pass":"..."}
 *
 * Call wifi_init() once at boot if you want auto-connect from the stored
 * credentials.  JS apps can also call wifi.connect() / wifi.startAP()
 * interactively via the js_wifi bindings.
 */

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Initialisation
// ---------------------------------------------------------------------------

// Attempt to load /config/wifi.json from SD and connect in station mode.
// Returns true if connected, false if no credentials or connection failed.
// Does NOT block indefinitely — waits at most wifi_connect_timeout_ms().
bool wifi_init();

// ---------------------------------------------------------------------------
// Station mode
// ---------------------------------------------------------------------------

// Connect to a WiFi network.  Blocks for up to WIFI_CONNECT_TIMEOUT_MS ms.
// Returns true on success.
bool wifi_connect(const char *ssid, const char *pass);

// Disconnect from the current network (or stop AP mode).
void wifi_disconnect();

// Returns true when the device is associated to an AP.
bool wifi_connected();

// Returns the current IP address as a dotted-decimal string (e.g. "192.168.1.42").
// Returns "0.0.0.0" when not connected.
const char *wifi_ip();

// ---------------------------------------------------------------------------
// AP mode
// ---------------------------------------------------------------------------

// Start a SoftAP with the given SSID and password.  Pass an empty string for
// pass to create an open network.
// Returns true on success.
bool wifi_start_ap(const char *ssid, const char *pass);

// Returns true when the device is in AP mode.
bool wifi_is_ap();

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------

// Station-mode connect timeout (milliseconds).
#define WIFI_CONNECT_TIMEOUT_MS  10000
