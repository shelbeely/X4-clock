#pragma once
/*
 * js_notify.h — notify.* core firmware API
 *
 * Maintains a C-side cache of notification items read from
 * /notifications/pending.json on the SD card.  The cache persists across JS
 * context reloads so clock faces and apps share the same data without hitting
 * the SD card on every draw().
 *
 * Call notify_init() once at boot (from app_loader_run).
 */

// Load (or reload) /notifications/pending.json from SD into the C-side cache.
void notify_init();
