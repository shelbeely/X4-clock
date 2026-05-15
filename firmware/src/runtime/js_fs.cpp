/*
 * js_fs.cpp — fs.* JavaScript bindings for Xteink X4
 *
 * Wraps the sdcard driver's integer-handle file API.  All reads are capped at
 * SD_CHUNK_SIZE (4 096 bytes) per call to stay within the 64 KB JS memory
 * budget — apps must read large files in a loop.
 *
 * fs.list() builds an array of plain objects { name, size, isDir } using
 * JSGCRef to protect the array and each entry object across GC cycles.
 */

#include "js_fs.h"
#include "drivers/sdcard.h"
#include "mquickjs.h"
#include <Arduino.h>
#include <string.h>

// Scratch buffer for chunk reads — allocated on the C stack during the call
#define READ_BUF_SIZE SD_CHUNK_SIZE

extern "C" {

// fs.open(path, mode)  → handle (int) or -1
JSValue js_x4_fs_open(JSContext *ctx, JSValue *this_val,
                      int argc, JSValue *argv) {
    if (argc < 2) return JS_ThrowTypeError(ctx, "fs.open(path, mode)");

    JSCStringBuf pbuf, mbuf;
    const char *path = JS_ToCString(ctx, argv[0], &pbuf);
    if (!path) return JS_EXCEPTION;
    const char *mode = JS_ToCString(ctx, argv[1], &mbuf);
    if (!mode) return JS_EXCEPTION;

    int h = sd_open(path, mode[0]);
    return JS_NewInt32(ctx, h);
}

// fs.read(handle, size)  → string (up to size bytes, capped at SD_CHUNK_SIZE)
JSValue js_x4_fs_read(JSContext *ctx, JSValue *this_val,
                      int argc, JSValue *argv) {
    if (argc < 2) return JS_ThrowTypeError(ctx, "fs.read(handle, size)");

    int h = 0, sz = 0;
    if (JS_ToInt32(ctx, &h, argv[0])) return JS_EXCEPTION;
    if (JS_ToInt32(ctx, &sz, argv[1])) return JS_EXCEPTION;

    if (sz <= 0 || sz > READ_BUF_SIZE) sz = READ_BUF_SIZE;

    // Stack-allocated read buffer keeps allocation outside JS heap
    char buf[READ_BUF_SIZE];
    int n = sd_read(h, buf, (size_t)sz);
    if (n <= 0) return JS_NewStringLen(ctx, "", 0);
    return JS_NewStringLen(ctx, buf, (size_t)n);
}

// fs.write(handle, data)  → bytes written
JSValue js_x4_fs_write(JSContext *ctx, JSValue *this_val,
                       int argc, JSValue *argv) {
    if (argc < 2) return JS_ThrowTypeError(ctx, "fs.write(handle, data)");

    int h = 0;
    if (JS_ToInt32(ctx, &h, argv[0])) return JS_EXCEPTION;

    JSCStringBuf buf;
    size_t len = 0;
    const char *data = JS_ToCStringLen(ctx, &len, argv[1], &buf);
    if (!data) return JS_EXCEPTION;

    int n = sd_write(h, data, len);
    return JS_NewInt32(ctx, n);
}

// fs.close(handle)
JSValue js_x4_fs_close(JSContext *ctx, JSValue *this_val,
                       int argc, JSValue *argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "fs.close(handle)");
    int h = 0;
    if (JS_ToInt32(ctx, &h, argv[0])) return JS_EXCEPTION;
    sd_close(h);
    return JS_UNDEFINED;
}

// fs.seek(handle, offset)
JSValue js_x4_fs_seek(JSContext *ctx, JSValue *this_val,
                      int argc, JSValue *argv) {
    if (argc < 2) return JS_ThrowTypeError(ctx, "fs.seek(handle, offset)");
    int h = 0;
    uint32_t offset = 0;
    if (JS_ToInt32(ctx, &h, argv[0])) return JS_EXCEPTION;
    if (JS_ToUint32(ctx, &offset, argv[1])) return JS_EXCEPTION;
    sd_seek(h, offset);
    return JS_UNDEFINED;
}

// fs.size(path)  → file size in bytes, or -1
JSValue js_x4_fs_size(JSContext *ctx, JSValue *this_val,
                      int argc, JSValue *argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "fs.size(path)");
    JSCStringBuf buf;
    const char *path = JS_ToCString(ctx, argv[0], &buf);
    if (!path) return JS_EXCEPTION;
    return JS_NewInt32(ctx, sd_size(path));
}

// fs.exists(path)  → bool
JSValue js_x4_fs_exists(JSContext *ctx, JSValue *this_val,
                        int argc, JSValue *argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "fs.exists(path)");
    JSCStringBuf buf;
    const char *path = JS_ToCString(ctx, argv[0], &buf);
    if (!path) return JS_EXCEPTION;
    return JS_NewBool(sd_exists(path));
}

// fs.list(dirPath)  → [{name, size, isDir}, ...]
JSValue js_x4_fs_list(JSContext *ctx, JSValue *this_val,
                      int argc, JSValue *argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "fs.list(path)");

    JSCStringBuf pbuf;
    const char *path = JS_ToCString(ctx, argv[0], &pbuf);
    if (!path) return JS_EXCEPTION;

    // Read directory listing into C arrays first, then build JS objects.
    // Static arrays avoid overflowing the FreeRTOS task stack (typically 4-8 KB).
    // Limited to 16 entries (~4 KB static footprint) to keep RAM usage bounded.
    static const int MAX_ENTRIES = 16;
    static char    names[MAX_ENTRIES][256];
    static bool    is_dirs[MAX_ENTRIES];
    static uint32_t szs[MAX_ENTRIES];

    int count = sd_list_dir(path, names, is_dirs, szs, MAX_ENTRIES);

    // Build JS array — use GCRef pattern to survive object allocations
    JSGCRef arr_ref, obj_ref;
    JSValue *arr = JS_PushGCRef(ctx, &arr_ref);
    JSValue *obj = JS_PushGCRef(ctx, &obj_ref);

    *arr = JS_NewArray(ctx, count);
    if (JS_IsException(*arr)) {
        JS_PopGCRef(ctx, &obj_ref);
        JS_PopGCRef(ctx, &arr_ref);
        return JS_EXCEPTION;
    }

    for (int i = 0; i < count; i++) {
        *obj = JS_NewObject(ctx);
        if (JS_IsException(*obj)) continue;

        JS_SetPropertyStr(ctx, *obj, "name",  JS_NewString(ctx, names[i]));
        JS_SetPropertyStr(ctx, *obj, "size",  JS_NewUint32(ctx, szs[i]));
        JS_SetPropertyStr(ctx, *obj, "isDir", JS_NewBool(is_dirs[i]));
        JS_SetPropertyUint32(ctx, *arr, (uint32_t)i, *obj);
    }

    JS_PopGCRef(ctx, &obj_ref);
    JSValue result = *arr;
    JS_PopGCRef(ctx, &arr_ref);
    return result;
}

} // extern "C"
