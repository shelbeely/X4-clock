#pragma once
// js_system.h — system.* binding helpers (intentionally empty)
// Bindings wired via x4_stdlib.h; see js_system.cpp for implementations.

// Called by app_loader to set the current app name so system.appName() works
void js_system_set_app_name(const char *name);
