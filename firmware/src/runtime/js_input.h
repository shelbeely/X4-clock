#pragma once
/*
 * js_input.h — Button-event bridge between the FreeRTOS queue and the JS
 *              input.onButton() callback.
 *
 * The app calls  input.onButton(fn)  once at setup to register a callback.
 * The app loader's main loop calls  js_input_dispatch_events()  each
 * iteration to drain the hardware button queue and invoke the JS callback.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "mquickjs.h"

// Must be called when a new context is created (resets stored callback)
void js_input_reset();

// Drain the hardware button queue and call the registered JS callback for
// each pending event.  Should be called once per loop() iteration.
void js_input_dispatch_events(JSContext *ctx);

#ifdef __cplusplus
}
#endif
