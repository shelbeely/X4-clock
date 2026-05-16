#pragma once
#include <stdint.h>
/*
 * js_weather.h — weather.* core firmware API
 *
 * Maintains a C-side cache of the last OpenWeatherMap fetch.  The cache
 * persists across JS context reloads so clock faces can call weather.temp()
 * etc. without triggering a network request on every draw().
 *
 * Call weather_init() once at boot (from app_loader_run) to load the OWM
 * API key, city, and fallback timezone from /config/settings.json.
 *
 * When weather.refresh() succeeds, the OpenWeatherMap response includes a
 * "timezone" field (UTC offset in seconds).  The firmware automatically calls
 * configTime() with this value so NTP syncs with the correct local timezone.
 *
 * JS API:
 *   weather.refresh()           → bool
 *   weather.valid()             → bool
 *   weather.temp()              → float (°C)
 *   weather.humidity()          → int   (%)
 *   weather.condition()         → string
 *   weather.city()              → string  (from API response)
 *   weather.age()               → int    (ms since last refresh, or -1)
 *   weather.tz()                → int    (UTC offset seconds; 0 if unknown)
 *   weather.setLocation(city)   → bool   (updates cache + /config/settings.json)
 *   weather.location()          → string (configured city name)
 */

// Load OWM key, city, and fallback tz_offset from /config/settings.json.
void weather_init();

// Returns the UTC timezone offset in seconds as provided by the last
// OpenWeatherMap response, or the fallback value from settings.json.
// Returns 0 if neither has been received.
int32_t weather_tz_offset_sec();
