#pragma once
/*
 * js_engine.h — MicroQuickJS context lifecycle manager
 *
 * MicroQuickJS (mquickjs) uses a single fixed-size memory buffer per context.
 * We allocate JS_MEM_SIZE bytes as a static array and pass it to
 * JS_NewContext() each time an app is launched.
 *
 * Key design points:
 *   • One context per app run — destroyed and recreated between apps
 *   • Memory limit enforced by buffer size (JS_MEM_SIZE = 64 KB)
 *   • All X4 native functions are in the compiled-in js_stdlib (x4_stdlib.h)
 *   • The generated x4_stdlib.h provides extern declarations + function table
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "mquickjs.h"

// Memory budget for the JavaScript context (64 KB per spec)
#define JS_MEM_SIZE (64 * 1024)

// Initialise the engine subsystem (call once at boot)
void js_engine_init();

// Create a fresh context using the static memory buffer.
// Registers all native X4 bindings.  Returns NULL on failure.
JSContext *js_engine_new_context();

// Execute raw JavaScript source stored in 'src' (null-terminated).
// Returns false and prints the exception if an error occurs.
bool js_engine_run_source(JSContext *ctx,
                          const char *src, size_t len,
                          const char *filename);

// Execute pre-compiled mquickjs bytecode from 'buf'.
// The buffer must remain valid for the lifetime of the context.
// Returns false on error.
bool js_engine_run_bytecode(JSContext *ctx,
                            uint8_t *buf, size_t len);

// Destroy a context previously created by js_engine_new_context().
void js_engine_destroy_context(JSContext *ctx);

// Helper: call a named global function with zero arguments.
// Returns false if the function does not exist or throws.
bool js_engine_call_func(JSContext *ctx, const char *name);

// Helper: print the current pending exception to Serial and clear it.
void js_engine_dump_exception(JSContext *ctx);

#ifdef __cplusplus
}
#endif
