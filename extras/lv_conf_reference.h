/**
 * @file lv_conf_reference.h
 * @brief Reference LVGL configuration for WuaDVI-lib — COPY, DO NOT INCLUDE.
 *
 * LVGL is configured per APPLICATION, not per library: `lv_conf.h` has to be
 * visible when LVGL itself is compiled, and only the application controls that.
 * A library therefore cannot set these for you.
 *
 * Copy this file into your project as `lv_conf.h` and adjust it, or merge the
 * settings below into a configuration you already have.
 *
 *   PlatformIO:  put it in include/ and add -DLV_CONF_INCLUDE_SIMPLE
 *   Arduino IDE: place lv_conf.h next to the lvgl library folder
 *
 * The settings marked REQUIRED are validated at compile time by WuaDVI-lib; if
 * one is wrong the build stops with an explanatory #error instead of failing
 * mysteriously at runtime.
 */
#pragma once

#if 1 /* Set to 1 once this file has been copied and renamed to lv_conf.h. */

/* ── REQUIRED: colour depth ──────────────────────────────────────────────────
 * The WuaDVI wire format is RGB565.  The monochrome display modes are produced
 * by thresholding that, not by building LVGL in 1-bit — so this is 16 in every
 * mode, including 640x480x1 and 1280x720x1. */
#define LV_COLOR_DEPTH 16

/* ── REQUIRED: ARGB8888 software drawing ─────────────────────────────────────
 * lv_bar composites its indicator through an ARGB8888 layer.  With this off the
 * bar draws its background and then silently refuses to fill — a failure that
 * looks like a logic bug in your code and costs hours to find. */
#define LV_DRAW_SW_SUPPORT_ARGB8888 1

/* ── REQUIRED: font set ──────────────────────────────────────────────────────
 * The widget primitives scale text by stepping between these sizes to fit the
 * active resolution.  Removing one narrows the ladder; removing several makes
 * text jump between very different sizes across modes. */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_48 1

/* ── Memory ──────────────────────────────────────────────────────────────────
 * 96 KB is comfortable for the reference dashboard on an ESP32-C3.  Networked
 * applications that also run WiFi and TLS may need to lower this — measure with
 * lv_mem_monitor() rather than guessing. */
#define LV_USE_STDLIB_MALLOC  LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING  LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_BUILTIN
#define LV_MEM_SIZE           (96U * 1024U)

/* ── Tick and refresh ────────────────────────────────────────────────────────
 * The tick is fed at runtime by the library; do not enable LV_TICK_CUSTOM.
 *
 * Do NOT lower the refresh rate to steady a marginal display mode: fewer
 * refreshes let more dirty area accumulate, so LVGL emits larger bursts of
 * rectangles.  Total traffic falls but it arrives in longer bursts, which is
 * exactly what starves the display engine.  Spread out at ~30 Hz it copes
 * better.  This was measured, not assumed. */
#define LV_TICK_CUSTOM     0
#define LV_DEF_REFR_PERIOD 33
#define LV_DPI_DEF         130

/* ── OS / threading ──────────────────────────────────────────────────────────
 * LVGL is single-threaded here.  If your application serves a network from
 * another task, never call lv_* from it — queue the change and apply it from
 * the task that owns LVGL. */
#define LV_USE_OS LV_OS_NONE

/* ── Logging ─────────────────────────────────────────────────────────────────
 * Keep LV_LOG_PRINTF at 0: printf() can block long enough to disturb the rect
 * stream.  Route logs through lv_log_register_print_cb() instead. */
#define LV_USE_LOG    1
#define LV_LOG_PRINTF 0

/* ── Widgets used by the primitives ──────────────────────────────────────── */
#define LV_USE_LABEL 1
#define LV_USE_BAR   1
#define LV_USE_SCALE 1
#define LV_USE_LINE  1

#endif /* copied to lv_conf.h */
