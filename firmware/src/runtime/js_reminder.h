#pragma once
/*
 * js_reminder.h — reminder.* core firmware API
 *
 * Maintains a C-side cache of reminders read from
 * /reminders/pending.json on the SD card.  The cache persists across JS
 * context reloads so clock faces and apps share the same data.
 *
 * Reminder format (JSON):
 *   [{"id":1,"title":"…","time":1716001200,"body":"…","recurring":86400}, …]
 *
 * "recurring" is the repeat interval in seconds (0 = one-shot).
 * "time" is a Unix timestamp (seconds).  Use system.time() to compare.
 *
 * Call reminder_init() once at boot (from app_loader_run).
 */

// Load (or reload) /reminders/pending.json from SD into the C-side cache.
void reminder_init();
