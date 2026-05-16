/*
 * wifi_manager.cpp — WiFi station + AP mode driver for Xteink X4
 *
 * Uses the Arduino WiFi library (part of espressif32 platform SDK).
 * Credentials are stored as /config/wifi.json on the SD card.
 */

#include "wifi_manager.h"
#include "sdcard.h"
#include <WiFi.h>
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------

static char s_ip_buf[20] = "0.0.0.0";
static bool s_ap_active  = false;

// ---------------------------------------------------------------------------
// Simple JSON credential parser
// Expected format (strictly): {"ssid":"VALUE","pass":"VALUE"}
// Whitespace around colons/commas is tolerated.
// Returns true if both fields were found and fit in the output buffers.
// ---------------------------------------------------------------------------

static bool parse_wifi_json(const char *json,
                             char *ssid, size_t ssid_len,
                             char *pass, size_t pass_len) {
    // Extract "ssid":"VALUE"
    const char *p = strstr(json, "\"ssid\"");
    if (!p) return false;
    p = strchr(p + 6, '"');  // opening quote of value
    if (!p) return false;
    p++;
    const char *end = strchr(p, '"');
    if (!end) return false;
    size_t n = (size_t)(end - p);
    if (n >= ssid_len) n = ssid_len - 1;
    strncpy(ssid, p, n);
    ssid[n] = '\0';

    // Extract "pass":"VALUE"
    p = strstr(json, "\"pass\"");
    if (!p) {
        pass[0] = '\0';  // optional — open network
        return true;
    }
    p = strchr(p + 6, '"');
    if (!p) { pass[0] = '\0'; return true; }
    p++;
    end = strchr(p, '"');
    if (!end) { pass[0] = '\0'; return true; }
    n = (size_t)(end - p);
    if (n >= pass_len) n = pass_len - 1;
    strncpy(pass, p, n);
    pass[n] = '\0';

    return true;
}

// ---------------------------------------------------------------------------
// wifi_init — load credentials from SD and connect
// ---------------------------------------------------------------------------

bool wifi_init() {
    if (!sdcard_available()) {
        Serial.println("[wifi] SD not available — skipping auto-connect");
        return false;
    }

    int32_t sz = sd_size("/config/wifi.json");
    if (sz <= 0) {
        Serial.println("[wifi] /config/wifi.json not found");
        return false;
    }

    // Read JSON file (cap at 512 bytes — credentials are always short)
    int fh = sd_open("/config/wifi.json", 'r');
    if (fh < 0) return false;

    char buf[512] = {};
    int n = sd_read(fh, buf, sizeof(buf) - 1);
    sd_close(fh);
    if (n <= 0) return false;
    buf[n] = '\0';

    char ssid[64] = {}, pass[64] = {};
    if (!parse_wifi_json(buf, ssid, sizeof(ssid), pass, sizeof(pass))) {
        Serial.println("[wifi] failed to parse /config/wifi.json");
        return false;
    }

    Serial.printf("[wifi] auto-connecting to '%s'\n", ssid);
    return wifi_connect(ssid, pass);
}

// ---------------------------------------------------------------------------
// wifi_connect
// ---------------------------------------------------------------------------

bool wifi_connect(const char *ssid, const char *pass) {
    if (!ssid || ssid[0] == '\0') return false;

    // Disconnect from any current connection / AP
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false);
    s_ap_active = false;

    if (pass && pass[0] != '\0') {
        WiFi.begin(ssid, pass);
    } else {
        WiFi.begin(ssid);
    }

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start >= WIFI_CONNECT_TIMEOUT_MS) {
            Serial.printf("[wifi] connect timeout for '%s'\n", ssid);
            return false;
        }
        delay(100);
    }

    IPAddress ip = WiFi.localIP();
    snprintf(s_ip_buf, sizeof(s_ip_buf), "%d.%d.%d.%d",
             ip[0], ip[1], ip[2], ip[3]);
    Serial.printf("[wifi] connected, IP: %s\n", s_ip_buf);
    return true;
}

// ---------------------------------------------------------------------------
// wifi_disconnect
// ---------------------------------------------------------------------------

void wifi_disconnect() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    s_ap_active = false;
    strncpy(s_ip_buf, "0.0.0.0", sizeof(s_ip_buf));
}

// ---------------------------------------------------------------------------
// wifi_connected
// ---------------------------------------------------------------------------

bool wifi_connected() {
    return WiFi.status() == WL_CONNECTED;
}

// ---------------------------------------------------------------------------
// wifi_ip
// ---------------------------------------------------------------------------

const char *wifi_ip() {
    if (s_ap_active) {
        IPAddress ip = WiFi.softAPIP();
        snprintf(s_ip_buf, sizeof(s_ip_buf), "%d.%d.%d.%d",
                 ip[0], ip[1], ip[2], ip[3]);
        return s_ip_buf;
    }
    if (WiFi.status() == WL_CONNECTED) {
        IPAddress ip = WiFi.localIP();
        snprintf(s_ip_buf, sizeof(s_ip_buf), "%d.%d.%d.%d",
                 ip[0], ip[1], ip[2], ip[3]);
        return s_ip_buf;
    }
    return "0.0.0.0";
}

// ---------------------------------------------------------------------------
// wifi_start_ap
// ---------------------------------------------------------------------------

bool wifi_start_ap(const char *ssid, const char *pass) {
    if (!ssid || ssid[0] == '\0') return false;

    WiFi.mode(WIFI_AP);
    bool ok;
    if (pass && pass[0] != '\0') {
        ok = WiFi.softAP(ssid, pass);
    } else {
        ok = WiFi.softAP(ssid);
    }

    if (ok) {
        s_ap_active = true;
        IPAddress ip = WiFi.softAPIP();
        snprintf(s_ip_buf, sizeof(s_ip_buf), "%d.%d.%d.%d",
                 ip[0], ip[1], ip[2], ip[3]);
        Serial.printf("[wifi] AP started '%s', IP: %s\n", ssid, s_ip_buf);
    } else {
        Serial.println("[wifi] softAP failed");
    }
    return ok;
}

// ---------------------------------------------------------------------------
// wifi_is_ap
// ---------------------------------------------------------------------------

bool wifi_is_ap() {
    return s_ap_active;
}
