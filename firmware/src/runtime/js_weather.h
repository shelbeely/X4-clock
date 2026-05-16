#pragma once
/*
 * js_weather.h — weather.* core firmware API
 *
 * Maintains a C-side cache of the last OpenWeatherMap fetch.  The cache
 * persists across JS context reloads so clock faces can call weather.temp()
 * etc. without triggering a network request on every draw().
 *
 * Call weather_init() once at boot (from app_loader_run) to load the OWM
 * API key and city from /config/settings.json.  Faces or apps then call
 * weather.refresh() when they want fresh data (requires WiFi).
 */

// Read /config/settings.json and cache the OWM key + city.
// Does NOT fetch weather data — call weather.refresh() from JS for that.
void weather_init();
