#pragma once
/*
 * js_display.h — Display binding helpers
 *
 * The actual C binding functions (js_x4_display_*) are defined with
 * extern "C" linkage in js_display.cpp and are referenced by name in the
 * generated x4_stdlib.h function table.
 *
 * No public C++ API is needed here — the bindings are wired through the
 * mquickjs stdlib at context creation time.
 */

// (intentionally empty — bindings are auto-wired via x4_stdlib.h)
