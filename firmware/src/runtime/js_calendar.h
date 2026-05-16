#pragma once
/*
 * js_calendar.h — calendar.* core firmware API
 *
 * Maintains a C-side cache of calendar events read from
 * /calendar/events.json on the SD card.  The cache persists across JS
 * context reloads so clock faces and apps share the same data.
 *
 * Event format (JSON):
 *   [{"id":1,"title":"…","start":1716000000,"end":1716003600,"desc":"…"}, …]
 *
 * Call calendar_init() once at boot (from app_loader_run).
 */

// Load (or reload) /calendar/events.json from SD into the C-side cache.
void calendar_init();
