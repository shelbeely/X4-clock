#pragma once
/*
 * js_http_client.h — HTTP client binding helpers for Xteink X4
 *
 * Exposed to JS as http.*
 *   http.get(url)               → string body (synchronous, capped at 4 KB)
 *   http.getAsync(url, callback) → void  (non-blocking; cb(err, body) on next tick)
 *
 * js_http_poll(ctx) must be called from the app_loader loop after each loop()
 * invocation so that pending async results are delivered to JS callbacks.
 */

#include "mquickjs.h"

// Called by app_loader after each JS loop() to deliver any pending async result.
void js_http_poll(JSContext *ctx);

// Called by js_engine when context is destroyed, to cancel any pending request.
void js_http_reset();
