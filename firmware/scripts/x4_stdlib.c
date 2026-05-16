/*
 * x4_stdlib.c — MicroQuickJS custom stdlib for the Xteink X4 firmware
 *
 * This file is compiled as a HOST binary alongside mquickjs_build.c to
 * generate firmware/lib/mquickjs/src/x4_stdlib.h.  That generated header
 * contains the compiled stdlib bytecode table + a C function table that the
 * firmware links against at build time.
 *
 * Build steps (handled automatically by fetch_mquickjs.sh):
 *   gcc -Wall -O2 -D_GNU_SOURCE \
 *       -o x4_stdlib_tool x4_stdlib.c mquickjs_build.c
 *   ./x4_stdlib_tool -m32        > x4_stdlib.h
 *   ./x4_stdlib_tool -a -m32     > mquickjs_atom.h
 *
 * The -m32 flag generates 32-bit bytecode suitable for the ESP32-C3 (RISC-V
 * 32-bit) even when the host machine is 64-bit.
 *
 * IMPORTANT — Function naming convention:
 *   All X4 binding functions are prefixed js_x4_ to avoid collisions with the
 *   mquickjs internal functions (js_*) that are referenced in the same table.
 *   They are defined with extern "C" linkage in the corresponding runtime
 *   source files (js_display.cpp, js_input.cpp, js_fs.cpp, js_system.cpp).
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "mquickjs_build.h"

/* =========================================================================
 * Standard library classes
 * (Mirrors mqjs_stdlib.c — adding our X4 globals into the global object)
 * ========================================================================= */

/* --- Object --- */
static const JSPropDef js_object_proto[] = {
    JS_CFUNC_DEF("hasOwnProperty", 1, js_object_hasOwnProperty),
    JS_CFUNC_DEF("toString",       0, js_object_toString),
    JS_PROP_END,
};
static const JSPropDef js_object[] = {
    JS_CFUNC_DEF("defineProperty", 3, js_object_defineProperty),
    JS_CFUNC_DEF("getPrototypeOf", 1, js_object_getPrototypeOf),
    JS_CFUNC_DEF("setPrototypeOf", 2, js_object_setPrototypeOf),
    JS_CFUNC_DEF("create",         2, js_object_create),
    JS_CFUNC_DEF("keys",           1, js_object_keys),
    JS_PROP_END,
};
static const JSClassDef js_object_class =
    JS_CLASS_DEF("Object", 1, js_object_constructor, JS_CLASS_OBJECT,
                 js_object, js_object_proto, NULL, NULL);

/* --- Function --- */
static const JSPropDef js_function_proto[] = {
    JS_CGETSET_DEF("prototype", js_function_get_prototype, js_function_set_prototype),
    JS_CFUNC_DEF("call",    1, js_function_call),
    JS_CFUNC_DEF("apply",   2, js_function_apply),
    JS_CFUNC_DEF("bind",    1, js_function_bind),
    JS_CFUNC_DEF("toString",0, js_function_toString),
    JS_CGETSET_MAGIC_DEF("length", js_function_get_length_name, NULL, 0),
    JS_CGETSET_MAGIC_DEF("name",   js_function_get_length_name, NULL, 1),
    JS_PROP_END,
};
static const JSClassDef js_function_class =
    JS_CLASS_DEF("Function", 1, js_function_constructor, JS_CLASS_CLOSURE,
                 NULL, js_function_proto, NULL, NULL);

/* --- Number --- */
static const JSPropDef js_number_proto[] = {
    JS_CFUNC_DEF("toExponential", 1, js_number_toExponential),
    JS_CFUNC_DEF("toFixed",       1, js_number_toFixed),
    JS_CFUNC_DEF("toPrecision",   1, js_number_toPrecision),
    JS_CFUNC_DEF("toString",      1, js_number_toString),
    JS_PROP_END,
};
static const JSPropDef js_number[] = {
    JS_CFUNC_DEF("parseInt",   2, js_number_parseInt),
    JS_CFUNC_DEF("parseFloat", 1, js_number_parseFloat),
    JS_PROP_DOUBLE_DEF("MAX_VALUE",          1.7976931348623157e+308, 0),
    JS_PROP_DOUBLE_DEF("MIN_VALUE",          5e-324,                  0),
    JS_PROP_DOUBLE_DEF("NaN",                NAN,                     0),
    JS_PROP_DOUBLE_DEF("NEGATIVE_INFINITY", -INFINITY,                0),
    JS_PROP_DOUBLE_DEF("POSITIVE_INFINITY",  INFINITY,                0),
    JS_PROP_DOUBLE_DEF("EPSILON",            2.220446049250313e-16,   0),
    JS_PROP_DOUBLE_DEF("MAX_SAFE_INTEGER",   9007199254740991.0,      0),
    JS_PROP_DOUBLE_DEF("MIN_SAFE_INTEGER",  -9007199254740991.0,      0),
    JS_PROP_END,
};
static const JSClassDef js_number_class =
    JS_CLASS_DEF("Number", 1, js_number_constructor, JS_CLASS_NUMBER,
                 js_number, js_number_proto, NULL, NULL);

/* --- Boolean --- */
static const JSClassDef js_boolean_class =
    JS_CLASS_DEF("Boolean", 1, js_boolean_constructor, JS_CLASS_BOOLEAN,
                 NULL, NULL, NULL, NULL);

/* --- String --- */
static const JSPropDef js_string_proto[] = {
    JS_CGETSET_DEF("length",     js_string_get_length,    js_string_set_length),
    JS_CFUNC_MAGIC_DEF("charAt",       1, js_string_charAt, magic_charAt),
    JS_CFUNC_MAGIC_DEF("charCodeAt",   1, js_string_charAt, magic_charCodeAt),
    JS_CFUNC_MAGIC_DEF("codePointAt",  1, js_string_charAt, magic_codePointAt),
    JS_CFUNC_DEF("slice",              2, js_string_slice),
    JS_CFUNC_DEF("substring",          2, js_string_substring),
    JS_CFUNC_DEF("concat",             1, js_string_concat),
    JS_CFUNC_MAGIC_DEF("indexOf",      1, js_string_indexOf, 0),
    JS_CFUNC_MAGIC_DEF("lastIndexOf",  1, js_string_indexOf, 1),
    JS_CFUNC_DEF("match",              1, js_string_match),
    JS_CFUNC_MAGIC_DEF("replace",      2, js_string_replace,    0),
    JS_CFUNC_MAGIC_DEF("replaceAll",   2, js_string_replace,    1),
    JS_CFUNC_DEF("search",             1, js_string_search),
    JS_CFUNC_DEF("split",              2, js_string_split),
    JS_CFUNC_MAGIC_DEF("toLowerCase",  0, js_string_toLowerCase, 1),
    JS_CFUNC_MAGIC_DEF("toUpperCase",  0, js_string_toLowerCase, 0),
    JS_CFUNC_MAGIC_DEF("trim",         0, js_string_trim, 3),
    JS_CFUNC_MAGIC_DEF("trimEnd",      0, js_string_trim, 2),
    JS_CFUNC_MAGIC_DEF("trimStart",    0, js_string_trim, 1),
    JS_CFUNC_DEF("toString",           0, js_string_toString),
    JS_CFUNC_DEF("repeat",             1, js_string_repeat),
    JS_PROP_END,
};
static const JSPropDef js_string[] = {
    JS_CFUNC_MAGIC_DEF("fromCharCode",  1, js_string_fromCharCode, 0),
    JS_CFUNC_MAGIC_DEF("fromCodePoint", 1, js_string_fromCharCode, 1),
    JS_PROP_END,
};
static const JSClassDef js_string_class =
    JS_CLASS_DEF("String", 1, js_string_constructor, JS_CLASS_STRING,
                 js_string, js_string_proto, NULL, NULL);

/* --- Array --- */
static const JSPropDef js_array_proto[] = {
    JS_CFUNC_DEF("concat",                1, js_array_concat),
    JS_CGETSET_DEF("length",              js_array_get_length, js_array_set_length),
    JS_CFUNC_MAGIC_DEF("push",            1, js_array_push, 0),
    JS_CFUNC_DEF("pop",                   0, js_array_pop),
    JS_CFUNC_DEF("join",                  1, js_array_join),
    JS_CFUNC_DEF("toString",              0, js_array_toString),
    JS_CFUNC_DEF("reverse",               0, js_array_reverse),
    JS_CFUNC_DEF("shift",                 0, js_array_shift),
    JS_CFUNC_DEF("slice",                 2, js_array_slice),
    JS_CFUNC_DEF("splice",                2, js_array_splice),
    JS_CFUNC_MAGIC_DEF("unshift",         1, js_array_push,      1),
    JS_CFUNC_MAGIC_DEF("indexOf",         1, js_array_indexOf,   0),
    JS_CFUNC_MAGIC_DEF("lastIndexOf",     1, js_array_indexOf,   1),
    JS_CFUNC_MAGIC_DEF("every",           1, js_array_every, js_special_every),
    JS_CFUNC_MAGIC_DEF("some",            1, js_array_every, js_special_some),
    JS_CFUNC_MAGIC_DEF("forEach",         1, js_array_every, js_special_forEach),
    JS_CFUNC_MAGIC_DEF("map",             1, js_array_every, js_special_map),
    JS_CFUNC_MAGIC_DEF("filter",          1, js_array_every, js_special_filter),
    JS_CFUNC_MAGIC_DEF("reduce",          1, js_array_reduce, js_special_reduce),
    JS_CFUNC_MAGIC_DEF("reduceRight",     1, js_array_reduce, js_special_reduceRight),
    JS_CFUNC_DEF("sort",                  1, js_array_sort),
    JS_PROP_END,
};
static const JSPropDef js_array[] = {
    JS_CFUNC_DEF("isArray", 1, js_array_isArray),
    JS_PROP_END,
};
static const JSClassDef js_array_class =
    JS_CLASS_DEF("Array", 1, js_array_constructor, JS_CLASS_ARRAY,
                 js_array, js_array_proto, NULL, NULL);

/* --- Error classes --- */
static const JSPropDef js_error_proto[] = {
    JS_CFUNC_DEF("toString", 0, js_error_toString),
    JS_PROP_STRING_DEF("name", "Error", 0),
    JS_CGETSET_MAGIC_DEF("message", js_error_get_message, NULL, 0),
    JS_CGETSET_MAGIC_DEF("stack",   js_error_get_message, NULL, 1),
    JS_PROP_END,
};
static const JSClassDef js_error_class =
    JS_CLASS_MAGIC_DEF("Error", 1, js_error_constructor, JS_CLASS_ERROR,
                       NULL, js_error_proto, NULL, NULL);

#define ERROR_DEF(cname, name, class_id)                                   \
static const JSPropDef js_ ## cname ## _proto[] = {                       \
    JS_PROP_STRING_DEF("name", name, 0),                                   \
    JS_PROP_END,                                                           \
};                                                                         \
static const JSClassDef js_ ## cname ## _class =                           \
    JS_CLASS_MAGIC_DEF(name, 1, js_error_constructor, class_id,            \
                       NULL, js_ ## cname ## _proto, &js_error_class, NULL);

ERROR_DEF(eval_error,      "EvalError",      JS_CLASS_EVAL_ERROR)
ERROR_DEF(range_error,     "RangeError",     JS_CLASS_RANGE_ERROR)
ERROR_DEF(reference_error, "ReferenceError", JS_CLASS_REFERENCE_ERROR)
ERROR_DEF(syntax_error,    "SyntaxError",    JS_CLASS_SYNTAX_ERROR)
ERROR_DEF(type_error,      "TypeError",      JS_CLASS_TYPE_ERROR)
ERROR_DEF(uri_error,       "URIError",       JS_CLASS_URI_ERROR)
ERROR_DEF(internal_error,  "InternalError",  JS_CLASS_INTERNAL_ERROR)

/* --- Math --- */
static const JSPropDef js_math[] = {
    JS_CFUNC_MAGIC_DEF("min",   2, js_math_min_max, 0),
    JS_CFUNC_MAGIC_DEF("max",   2, js_math_min_max, 1),
    JS_CFUNC_SPECIAL_DEF("sign",  1, f_f, js_math_sign),
    JS_CFUNC_SPECIAL_DEF("abs",   1, f_f, js_fabs),
    JS_CFUNC_SPECIAL_DEF("floor", 1, f_f, js_floor),
    JS_CFUNC_SPECIAL_DEF("ceil",  1, f_f, js_ceil),
    JS_CFUNC_SPECIAL_DEF("round", 1, f_f, js_round_inf),
    JS_CFUNC_SPECIAL_DEF("sqrt",  1, f_f, js_sqrt),
    JS_PROP_DOUBLE_DEF("E",       2.718281828459045,   0),
    JS_PROP_DOUBLE_DEF("LN10",    2.302585092994046,   0),
    JS_PROP_DOUBLE_DEF("LN2",     0.6931471805599453,  0),
    JS_PROP_DOUBLE_DEF("LOG2E",   1.4426950408889634,  0),
    JS_PROP_DOUBLE_DEF("LOG10E",  0.4342944819032518,  0),
    JS_PROP_DOUBLE_DEF("PI",      3.141592653589793,   0),
    JS_PROP_DOUBLE_DEF("SQRT1_2", 0.7071067811865476,  0),
    JS_PROP_DOUBLE_DEF("SQRT2",   1.4142135623730951,  0),
    JS_CFUNC_SPECIAL_DEF("sin",   1, f_f, js_sin),
    JS_CFUNC_SPECIAL_DEF("cos",   1, f_f, js_cos),
    JS_CFUNC_SPECIAL_DEF("tan",   1, f_f, js_tan),
    JS_CFUNC_SPECIAL_DEF("asin",  1, f_f, js_asin),
    JS_CFUNC_SPECIAL_DEF("acos",  1, f_f, js_acos),
    JS_CFUNC_SPECIAL_DEF("atan",  1, f_f, js_atan),
    JS_CFUNC_DEF("atan2",         2, js_math_atan2),
    JS_CFUNC_SPECIAL_DEF("exp",   1, f_f, js_exp),
    JS_CFUNC_SPECIAL_DEF("log",   1, f_f, js_log),
    JS_CFUNC_DEF("pow",           2, js_math_pow),
    JS_CFUNC_DEF("random",        0, js_math_random),
    JS_CFUNC_DEF("imul",          2, js_math_imul),
    JS_CFUNC_DEF("clz32",         1, js_math_clz32),
    JS_CFUNC_SPECIAL_DEF("fround",1, f_f, js_math_fround),
    JS_CFUNC_SPECIAL_DEF("trunc", 1, f_f, js_trunc),
    JS_CFUNC_SPECIAL_DEF("log2",  1, f_f, js_log2),
    JS_CFUNC_SPECIAL_DEF("log10", 1, f_f, js_log10),
    JS_PROP_END,
};
static const JSClassDef js_math_obj = JS_OBJECT_DEF("Math", js_math);

/* --- JSON --- */
static const JSPropDef js_json[] = {
    JS_CFUNC_DEF("parse",     2, js_json_parse),
    JS_CFUNC_DEF("stringify", 3, js_json_stringify),
    JS_PROP_END,
};
static const JSClassDef js_json_obj = JS_OBJECT_DEF("JSON", js_json);

/* --- Date (Date.now() only — no full Date object) --- */
static const JSPropDef js_date[] = {
    JS_CFUNC_DEF("now", 0, js_date_now),
    JS_PROP_END,
};
static const JSClassDef js_date_class =
    JS_CLASS_DEF("Date", 7, js_date_constructor, JS_CLASS_DATE,
                 js_date, NULL, NULL, NULL);

/* --- console --- */
static const JSPropDef js_console[] = {
    JS_CFUNC_DEF("log", 1, js_print),
    JS_PROP_END,
};
static const JSClassDef js_console_obj = JS_OBJECT_DEF("console", js_console);

/* --- performance --- */
static const JSPropDef js_performance[] = {
    JS_CFUNC_DEF("now", 0, js_performance_now),
    JS_PROP_END,
};
static const JSClassDef js_performance_obj =
    JS_OBJECT_DEF("performance", js_performance);

/* --- Typed arrays --- */
static const JSPropDef js_array_buffer_proto[] = {
    JS_CGETSET_DEF("byteLength", js_array_buffer_get_byteLength, NULL),
    JS_PROP_END,
};
static const JSClassDef js_array_buffer_class =
    JS_CLASS_DEF("ArrayBuffer", 1, js_array_buffer_constructor,
                 JS_CLASS_ARRAY_BUFFER, NULL, js_array_buffer_proto, NULL, NULL);

static const JSPropDef js_typed_array_base_proto[] = {
    JS_CGETSET_MAGIC_DEF("length",     js_typed_array_get_length, NULL, 0),
    JS_CGETSET_MAGIC_DEF("byteLength", js_typed_array_get_length, NULL, 1),
    JS_CGETSET_MAGIC_DEF("byteOffset", js_typed_array_get_length, NULL, 2),
    JS_CGETSET_MAGIC_DEF("buffer",     js_typed_array_get_length, NULL, 3),
    JS_CFUNC_DEF("join",     1, js_array_join),
    JS_CFUNC_DEF("toString", 0, js_array_toString),
    JS_CFUNC_DEF("subarray", 2, js_typed_array_subarray),
    JS_CFUNC_DEF("set",      1, js_typed_array_set),
    JS_PROP_END,
};
static const JSClassDef js_typed_array_base_class =
    JS_CLASS_DEF("TypedArray", 0, js_typed_array_base_constructor,
                 JS_CLASS_TYPED_ARRAY, NULL, js_typed_array_base_proto, NULL, NULL);

#define TA_DEF(name, class_name, bpe)                                         \
static const JSPropDef js_ ## name[] = {                                      \
    JS_PROP_DOUBLE_DEF("BYTES_PER_ELEMENT", bpe, 0), JS_PROP_END,            \
};                                                                             \
static const JSPropDef js_ ## name ## _proto[] = {                            \
    JS_PROP_DOUBLE_DEF("BYTES_PER_ELEMENT", bpe, 0), JS_PROP_END,            \
};                                                                             \
static const JSClassDef js_ ## name ## _class =                               \
    JS_CLASS_MAGIC_DEF(#name, 3, js_typed_array_constructor, class_name,      \
                       js_ ## name, js_ ## name ## _proto,                    \
                       &js_typed_array_base_class, NULL);

TA_DEF(Uint8ClampedArray, JS_CLASS_UINT8C_ARRAY,  1)
TA_DEF(Int8Array,         JS_CLASS_INT8_ARRAY,    1)
TA_DEF(Uint8Array,        JS_CLASS_UINT8_ARRAY,   1)
TA_DEF(Int16Array,        JS_CLASS_INT16_ARRAY,   2)
TA_DEF(Uint16Array,       JS_CLASS_UINT16_ARRAY,  2)
TA_DEF(Int32Array,        JS_CLASS_INT32_ARRAY,   4)
TA_DEF(Uint32Array,       JS_CLASS_UINT32_ARRAY,  4)
TA_DEF(Float32Array,      JS_CLASS_FLOAT32_ARRAY, 4)
TA_DEF(Float64Array,      JS_CLASS_FLOAT64_ARRAY, 8)

/* --- RegExp --- */
static const JSPropDef js_regexp_proto[] = {
    JS_CGETSET_DEF("lastIndex", js_regexp_get_lastIndex, js_regexp_set_lastIndex),
    JS_CGETSET_DEF("source",    js_regexp_get_source,    NULL),
    JS_CGETSET_DEF("flags",     js_regexp_get_flags,     NULL),
    JS_CFUNC_MAGIC_DEF("exec",  1, js_regexp_exec, 0),
    JS_CFUNC_MAGIC_DEF("test",  1, js_regexp_exec, 1),
    JS_PROP_END,
};
static const JSClassDef js_regexp_class =
    JS_CLASS_DEF("RegExp", 2, js_regexp_constructor, JS_CLASS_REGEXP,
                 NULL, js_regexp_proto, NULL, NULL);

/* =========================================================================
 * X4 hardware API objects
 * ========================================================================= */

/* display.* */
static const JSPropDef x4_display_props[] = {
    JS_CFUNC_DEF("clear",          0, js_x4_display_clear),
    JS_CFUNC_DEF("print",          4, js_x4_display_print),
    JS_CFUNC_DEF("drawRect",       4, js_x4_display_drawRect),
    JS_CFUNC_DEF("drawBitmap",     3, js_x4_display_drawBitmap),
    JS_CFUNC_DEF("refresh",        0, js_x4_display_refresh),
    JS_CFUNC_DEF("partialRefresh", 0, js_x4_display_partialRefresh),
    JS_CFUNC_DEF("width",          0, js_x4_display_width),
    JS_CFUNC_DEF("height",         0, js_x4_display_height),
    JS_CFUNC_DEF("hibernate",      0, js_x4_display_hibernate),
    JS_CFUNC_DEF("wake",           0, js_x4_display_wake),
    JS_CFUNC_DEF("setRotation",    1, js_x4_display_setRotation),
    JS_CFUNC_DEF("rotation",       0, js_x4_display_rotation),
    JS_PROP_END,
};
static const JSClassDef x4_display_obj = JS_OBJECT_DEF("display", x4_display_props);

/* input.* */
static const JSPropDef x4_input_props[] = {
    JS_CFUNC_DEF("onButton", 1, js_x4_input_onButton),
    JS_PROP_END,
};
static const JSClassDef x4_input_obj = JS_OBJECT_DEF("input", x4_input_props);

/* fs.* */
static const JSPropDef x4_fs_props[] = {
    JS_CFUNC_DEF("open",   2, js_x4_fs_open),
    JS_CFUNC_DEF("read",   2, js_x4_fs_read),
    JS_CFUNC_DEF("write",  2, js_x4_fs_write),
    JS_CFUNC_DEF("close",  1, js_x4_fs_close),
    JS_CFUNC_DEF("seek",   2, js_x4_fs_seek),
    JS_CFUNC_DEF("size",   1, js_x4_fs_size),
    JS_CFUNC_DEF("list",   1, js_x4_fs_list),
    JS_CFUNC_DEF("exists", 1, js_x4_fs_exists),
    JS_PROP_END,
};
static const JSClassDef x4_fs_obj = JS_OBJECT_DEF("fs", x4_fs_props);

/* system.* */
static const JSPropDef x4_system_props[] = {
    JS_CFUNC_DEF("millis",              0, js_x4_system_millis),
    JS_CFUNC_DEF("battery",             0, js_x4_system_battery),
    JS_CFUNC_DEF("batteryLow",          0, js_x4_system_batteryLow),
    JS_CFUNC_DEF("sleep",               1, js_x4_system_sleep),
    JS_CFUNC_DEF("lightSleep",          1, js_x4_system_lightSleep),
    JS_CFUNC_DEF("setIdleTimeout",      1, js_x4_system_setIdleTimeout),
    JS_CFUNC_DEF("setRefreshInterval",  1, js_x4_system_setRefreshInterval),
    JS_CFUNC_DEF("log",                 1, js_x4_system_log),
    JS_CFUNC_DEF("appName",             0, js_x4_system_appName),
    JS_PROP_END,
};
static const JSClassDef x4_system_obj = JS_OBJECT_DEF("system", x4_system_props);

/* wifi.* */
static const JSPropDef x4_wifi_props[] = {
    JS_CFUNC_DEF("connect",    2, js_x4_wifi_connect),
    JS_CFUNC_DEF("startAP",    2, js_x4_wifi_startAP),
    JS_CFUNC_DEF("disconnect", 0, js_x4_wifi_disconnect),
    JS_CFUNC_DEF("connected",  0, js_x4_wifi_connected),
    JS_CFUNC_DEF("ip",         0, js_x4_wifi_ip),
    JS_PROP_END,
};
static const JSClassDef x4_wifi_obj = JS_OBJECT_DEF("wifi", x4_wifi_props);

/* http.* */
static const JSPropDef x4_http_props[] = {
    JS_CFUNC_DEF("get",      1, js_x4_http_get),
    JS_CFUNC_DEF("getAsync", 2, js_x4_http_getAsync),
    JS_PROP_END,
};
static const JSClassDef x4_http_obj = JS_OBJECT_DEF("http", x4_http_props);

/* server.* */
static const JSPropDef x4_server_props[] = {
    JS_CFUNC_DEF("begin",         1, js_x4_server_begin),
    JS_CFUNC_DEF("stop",          0, js_x4_server_stop),
    JS_CFUNC_DEF("onRequest",     2, js_x4_server_onRequest),
    JS_CFUNC_DEF("send",          3, js_x4_server_send),
    JS_CFUNC_DEF("handleClient",  0, js_x4_server_handleClient),
    JS_PROP_END,
};
static const JSClassDef x4_server_obj = JS_OBJECT_DEF("server", x4_server_props);

/* =========================================================================
 * Global object
 * ========================================================================= */

static const JSPropDef js_global_object[] = {
    /* Standard builtins */
    JS_PROP_CLASS_DEF("Object",             &js_object_class),
    JS_PROP_CLASS_DEF("Function",           &js_function_class),
    JS_PROP_CLASS_DEF("Number",             &js_number_class),
    JS_PROP_CLASS_DEF("Boolean",            &js_boolean_class),
    JS_PROP_CLASS_DEF("String",             &js_string_class),
    JS_PROP_CLASS_DEF("Array",              &js_array_class),
    JS_PROP_CLASS_DEF("Math",               &js_math_obj),
    JS_PROP_CLASS_DEF("Date",               &js_date_class),
    JS_PROP_CLASS_DEF("JSON",               &js_json_obj),
    JS_PROP_CLASS_DEF("RegExp",             &js_regexp_class),
    JS_PROP_CLASS_DEF("Error",              &js_error_class),
    JS_PROP_CLASS_DEF("EvalError",          &js_eval_error_class),
    JS_PROP_CLASS_DEF("RangeError",         &js_range_error_class),
    JS_PROP_CLASS_DEF("ReferenceError",     &js_reference_error_class),
    JS_PROP_CLASS_DEF("SyntaxError",        &js_syntax_error_class),
    JS_PROP_CLASS_DEF("TypeError",          &js_type_error_class),
    JS_PROP_CLASS_DEF("URIError",           &js_uri_error_class),
    JS_PROP_CLASS_DEF("InternalError",      &js_internal_error_class),
    JS_PROP_CLASS_DEF("ArrayBuffer",        &js_array_buffer_class),
    JS_PROP_CLASS_DEF("Uint8ClampedArray",  &js_Uint8ClampedArray_class),
    JS_PROP_CLASS_DEF("Int8Array",          &js_Int8Array_class),
    JS_PROP_CLASS_DEF("Uint8Array",         &js_Uint8Array_class),
    JS_PROP_CLASS_DEF("Int16Array",         &js_Int16Array_class),
    JS_PROP_CLASS_DEF("Uint16Array",        &js_Uint16Array_class),
    JS_PROP_CLASS_DEF("Int32Array",         &js_Int32Array_class),
    JS_PROP_CLASS_DEF("Uint32Array",        &js_Uint32Array_class),
    JS_PROP_CLASS_DEF("Float32Array",       &js_Float32Array_class),
    JS_PROP_CLASS_DEF("Float64Array",       &js_Float64Array_class),
    /* Global functions */
    JS_CFUNC_DEF("parseInt",   2, js_number_parseInt),
    JS_CFUNC_DEF("parseFloat", 1, js_number_parseFloat),
    JS_CFUNC_DEF("eval",       1, js_global_eval),
    JS_CFUNC_DEF("isNaN",      1, js_global_isNaN),
    JS_CFUNC_DEF("isFinite",   1, js_global_isFinite),
    JS_CFUNC_DEF("print",      1, js_print),
    /* Global constants */
    JS_PROP_DOUBLE_DEF("Infinity", 1.0 / 0.0, 0),
    JS_PROP_DOUBLE_DEF("NaN",      NAN,        0),
    JS_PROP_UNDEFINED_DEF("undefined", 0),
    JS_PROP_NULL_DEF("globalThis", 0),
    /* Standard objects */
    JS_PROP_CLASS_DEF("console",     &js_console_obj),
    JS_PROP_CLASS_DEF("performance", &js_performance_obj),
    /* GC trigger (always available per spec) */
    JS_CFUNC_DEF("gc", 0, js_gc),
    /* X4 hardware API */
    JS_PROP_CLASS_DEF("display", &x4_display_obj),
    JS_PROP_CLASS_DEF("input",   &x4_input_obj),
    JS_PROP_CLASS_DEF("fs",      &x4_fs_obj),
    JS_PROP_CLASS_DEF("system",  &x4_system_obj),
    JS_PROP_CLASS_DEF("wifi",    &x4_wifi_obj),
    JS_PROP_CLASS_DEF("http",    &x4_http_obj),
    JS_PROP_CLASS_DEF("server",  &x4_server_obj),
    JS_PROP_END,
};

/* C-closure declarations (js_function_bound must come first) */
static const JSPropDef js_c_function_decl[] = {
    JS_CFUNC_SPECIAL_DEF("bound", 0, generic_params, js_function_bound),
    JS_PROP_END,
};

/* =========================================================================
 * Entry point — called by build tool to emit x4_stdlib.h
 * ========================================================================= */

int main(int argc, char **argv) {
    return build_atoms("js_stdlib", js_global_object, js_c_function_decl,
                       argc, argv);
}
