/*
 * js_input.cpp — input.* JavaScript bindings for Xteink X4
 *
 * The JS app registers a callback via  input.onButton(fn).
 * During each loop iteration the app_loader calls js_input_dispatch_events()
 * which drains the hardware button queue and calls fn(buttonName) for each
 * pending event.
 *
 * The callback JSValue is kept alive across GC cycles using JS_AddGCRef /
 * JS_DeleteGCRef — the correct mquickjs pattern for persistent references.
 */

#include "js_input.h"
#include "drivers/buttons.h"
#include "mquickjs.h"
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Persistent callback storage
// ---------------------------------------------------------------------------

static JSGCRef  s_cb_ref;
static JSValue *s_cb_ptr  = nullptr;   // nullptr = no callback registered

extern "C" {

// input.onButton(fn)
JSValue js_x4_input_onButton(JSContext *ctx, JSValue *this_val,
                              int argc, JSValue *argv) {
    if (argc < 1 || !JS_IsFunction(ctx, argv[0]))
        return JS_ThrowTypeError(ctx, "input.onButton requires a function");

    if (s_cb_ptr == nullptr) {
        // Allocate a persistent tracked reference
        s_cb_ptr = JS_AddGCRef(ctx, &s_cb_ref);
    }
    *s_cb_ptr = argv[0];
    return JS_UNDEFINED;
}

} // extern "C"

// ---------------------------------------------------------------------------
// C++ helpers called by app_loader
// ---------------------------------------------------------------------------

void js_input_reset() {
    // The old JSContext is being destroyed — the JSGCRef is no longer valid.
    // Zero both the pointer and the ref so any stale data is cleared.
    s_cb_ptr = nullptr;
    memset(&s_cb_ref, 0, sizeof(s_cb_ref));
}

void js_input_dispatch_events(JSContext *ctx) {
    if (!s_cb_ptr || JS_IsUndefined(*s_cb_ptr)) return;

    while (buttons_available()) {
        ButtonEvent ev = buttons_dequeue();
        if (ev == BTN_NONE) continue;

        const char *name = BTN_NAMES[ev];

        // We need 3 stack slots: arg, func, this
        if (JS_StackCheck(ctx, 3)) {
            Serial.println("[JS] input dispatch: stack overflow");
            return;
        }

        JSValue btn_str = JS_NewString(ctx, name);
        // After JS_NewString the GC may compact — *s_cb_ptr is auto-updated
        // because s_cb_ref is tracked.  btn_str is a temporary used
        // immediately before the next allocation.
        JS_PushArg(ctx, btn_str);
        JS_PushArg(ctx, *s_cb_ptr);
        JS_PushArg(ctx, JS_UNDEFINED);   // this = undefined
        JSValue ret = JS_Call(ctx, 1);
        if (JS_IsException(ret)) {
            Serial.println("[JS] input callback threw an exception");
        }
    }
}
