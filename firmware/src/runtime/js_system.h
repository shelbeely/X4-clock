#pragma once
// js_system.h — system.* binding helpers
// Bindings wired via x4_stdlib.h; see js_system.cpp for implementations.

#include <stdint.h>

// Called by app_loader to set the current app name so system.appName() works
void js_system_set_app_name(const char *name);

// Returns the current idle-sleep timeout in milliseconds.
// Default is IDLE_SLEEP_MS (from x4_pins.h).  Can be changed by JS via
// system.setIdleTimeout(ms).  Returns 0 when auto-sleep is disabled.
uint32_t js_system_idle_timeout_ms();

// Returns the current loop sleep interval in milliseconds.
// Default is LOOP_SLEEP_MS (from x4_pins.h).  Can be changed by JS via
// system.setRefreshInterval(ms).
uint32_t js_system_loop_sleep_ms();
