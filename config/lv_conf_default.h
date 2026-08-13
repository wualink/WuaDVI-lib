/**
 * @file lv_conf.h
 * @brief LVGL 9 configuration for the WuaDVI ESP32-C3 firmware.
 *
 * RGB565 render target in RAM (partial mode, 24-line strips), no input
 * devices, no filesystem.  Only the widgets the demo UI needs are enabled;
 * everything not set here falls back to LVGL defaults from
 * lv_conf_internal.h.
 *
 * Note: do NOT #include <stdint.h> here.  This file is pulled in by LVGL's
 * .S sources via lv_conf_internal.h, and the GAS assembler chokes on C
 * typedefs.  None of the macros below need stdint types anyway.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/* ── Color depth ─────────────────────────────────────────────────────────── */
/* 16 bpp RGB565, little-endian byte order — same as the RP2354B / PicoDVI
 * framebuffer, so rect payloads need no per-pixel conversion in color mode. */
#define LV_COLOR_DEPTH          16
#define LV_COLOR_16_SWAP        0     /* 0 = LSB first, matches the RP side */

/* ── Memory ──────────────────────────────────────────────────────────────── */
/* LVGL keeps its own static pool: widgets, styles, anims, the scale ticks of
 * the gauge AND the logo draw buffer (lv_draw_buf_create in logo.cpp, up to
 * ~29 KB) live here.  96 KB leaves ample headroom — image draws silently
 * skip when the pool ever runs dry, which must never happen. */
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_BUILTIN
/* LVGL's own pool, a static array — not the ESP heap, which stays free for the
 * render buffer.  Widgets are sized in pixels, so a screen costs roughly four
 * times as much at 1280x720 as at 640x480: 96 KB held a dashboard at 640x480
 * and ran out building a widget gallery at 1280x720.  Running out does not
 * return an error — LV_USE_ASSERT_MALLOC parks in a spin loop and the task
 * watchdog reboots, which looks like a bring-up that restarts forever. */
#define LV_MEM_SIZE             (128U * 1024U)
#define LV_MEM_POOL_EXPAND_SIZE 0

/* ── Tick + refresh ─────────────────────────────────────────────────────── */
/* The tick is fed at runtime via lv_tick_set_cb(millis). */
#define LV_TICK_CUSTOM          0
/* Do NOT lower this to steady 800x600x1 (the mode where the RP's core 1 has the
 * least scanout headroom) — it was tried at 100 ms / ~10 Hz and made the
 * dropouts markedly worse.  Fewer refreshes let more dirty area pile up between
 * them, so LVGL emits more bands at once: total traffic drops but it arrives as
 * longer bursts of back-to-back blits, and what starves core 1 is a long burst,
 * not the average rate.  Spread out at ~30 Hz it copes better. */
#define LV_DEF_REFR_PERIOD      33   /* ~30 Hz target; the SPI link is the real cap */
#define LV_DPI_DEF              130

/* ── OS / threading ─────────────────────────────────────────────────────── */
#define LV_USE_OS               LV_OS_NONE

/* ── Logging ────────────────────────────────────────────────────────────── */
/* CRITICAL: LV_LOG_PRINTF must stay 0 on this board.  printf routes to the
 * IDF console on UART0, and UART0's TX pin (GPIO21) is wired to the RP2354B
 * QSPI_SD3 strap — a log burst there interferes with the RP's internal-flash
 * bus.  Instead lvgl_port_init() registers a print callback that forwards
 * LVGL warnings to the USB-CDC console, where they are actually visible. */
#define LV_USE_LOG              1
#define LV_LOG_LEVEL            LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF           0
#define LV_LOG_USE_TIMESTAMP    1
#define LV_LOG_USE_FILE_LINE    1

/* ── Asserts ────────────────────────────────────────────────────────────── */
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0

/* ── Drawing ────────────────────────────────────────────────────────────── */
#define LV_DRAW_SW_COMPLEX          1
#define LV_USE_DRAW_SW              1
#define LV_DRAW_SW_SUPPORT_RGB565   1
/* ARGB8888 must stay enabled: LVGL composites through ARGB8888 layers —
 * lv_bar renders its indicator into one whenever it overlaps the track's
 * rounded ends (lv_bar.c, lv_draw_layer_create(..., ARGB8888, ...)), and
 * lv_refr uses them for corner-clipped/transparent subtrees.  Without this
 * flag those layer blends fail ("Not supported source color format") and the
 * affected widget areas are silently not drawn. */
#define LV_DRAW_SW_SUPPORT_ARGB8888 1
#define LV_DRAW_SW_SUPPORT_RGB888   0
#define LV_DRAW_SW_SUPPORT_XRGB8888 0
#define LV_DRAW_SW_SUPPORT_L8       0
#define LV_DRAW_SW_SUPPORT_AL88     0
#define LV_DRAW_SW_SUPPORT_A8       0
#define LV_DRAW_SW_SUPPORT_I1       0

/* ── Fonts ──────────────────────────────────────────────────────────────── */
/* Font table used by wua_font_fit() (wua_ui.cpp): the primitives request a
 * text height as a percentage of the screen and snap down to the largest
 * enabled Montserrat, so every resolution keeps the same proportions. */
/* The bottom of the ladder matters as much as the top.  A panel a fifth of a
 * screen wide at 320x240 leaves about 40 px for a caption, and a fitter that
 * cannot go below 14 px has nowhere to step down to — the text then overflows
 * its panel or wraps onto a second line, which looks like a broken fitter
 * rather than an exhausted one.  These modes are pixel-doubled on the way out,
 * so 8 px reaches the monitor as 16. */
#define LV_FONT_MONTSERRAT_8    1
#define LV_FONT_MONTSERRAT_10   1
#define LV_FONT_MONTSERRAT_12   1
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_18   1
#define LV_FONT_MONTSERRAT_24   1
#define LV_FONT_MONTSERRAT_32   1
#define LV_FONT_MONTSERRAT_48   1
#define LV_FONT_DEFAULT         &lv_font_montserrat_14

/* ── Widgets ────────────────────────────────────────────────────────────── */
/* The WHOLE set, not only the six this library's own primitives are built
 * from.  This file is the configuration an application inherits when it
 * supplies none of its own, and a library whose pitch is "declare it and it
 * works" cannot answer lv_slider_create() with a compile error — least of all
 * when the file to edit is generated into .pio/libdeps and regenerated from
 * under the user.
 *
 * The cost is close to nothing: a widget that is never referenced is dropped
 * at link time by --gc-sections, so this buys compile time, not flash.
 *
 * An application that wants a leaner build supplies its own lv_conf.h; the
 * library never overwrites one that already exists. */
#define LV_USE_OBJ              1
#define LV_USE_LABEL            1
#define LV_USE_IMAGE            1   /* Wualabs logo (JPEG-decoded at boot)   */
#define LV_USE_BAR              1   /* linear meter                          */
#define LV_USE_SCALE            1   /* round gauge                           */
#define LV_USE_LINE             1   /* gauge needle                          */

#define LV_USE_ANIMIMG          1
#define LV_USE_ARC              1
#define LV_USE_BUTTON           1
#define LV_USE_BUTTONMATRIX     1
#define LV_USE_CALENDAR         1
#define LV_USE_CANVAS           1
#define LV_USE_CHART            1
#define LV_USE_CHECKBOX         1
#define LV_USE_DROPDOWN         1
#define LV_USE_IMAGEBUTTON      1
#define LV_USE_KEYBOARD         1
#define LV_USE_LED              1
#define LV_USE_LIST             1
#define LV_USE_LOTTIE           0
#define LV_USE_MENU             1
#define LV_USE_MSGBOX           1
#define LV_USE_ROLLER           1
#define LV_USE_SLIDER           1
#define LV_USE_SPAN             1
#define LV_USE_SPINBOX          1
#define LV_USE_SPINNER          1
#define LV_USE_SWITCH           1
#define LV_USE_TABLE            1
#define LV_USE_TABVIEW          1
#define LV_USE_TEXTAREA         1
#define LV_USE_TILEVIEW         1
#define LV_USE_WIN              1

/* ── Layouts / themes ───────────────────────────────────────────────────── */
#define LV_USE_FLEX             1   /* the whole demo layout is flex-based   */
#define LV_USE_GRID             1
#define LV_USE_THEME_DEFAULT    1
#define LV_THEME_DEFAULT_DARK   1   /* dark UI: masks 1-bit flat-field banding */

/* ── Misc ───────────────────────────────────────────────────────────────── */
#define LV_USE_SYSMON           0
#define LV_USE_PERF_MONITOR     0
#define LV_USE_MEM_MONITOR      0

#endif /* LV_CONF_H */
