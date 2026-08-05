/**
 * @file wua_ui.h
 * @brief WuaDVI UI primitives — resolution-independent building blocks.
 *
 * Every primitive takes RELATIVE sizes (percentages) and resolves pixels,
 * fonts and styles internally for the active resolution, so screens built on
 * top of them never contain a per-resolution #if:
 *
 *   - Text heights are given as a percentage of wua_screen_h() and snap down to
 *     the largest enabled Montserrat font (14/18/24/32/48) — the same layout
 *     keeps the same proportions from 320x240 to 800x600.
 *   - Value labels reserve the width of their widest possible text, so the
 *     layout NEVER reflows when a value changes (no "dancing" widgets).
 *   - Widget setters skip redundant updates (same value → no redraw → no
 *     SPI traffic, no allocator churn).
 *   - The palette is monochrome-safe: every foreground survives the wire's
 *     1-bit luminance threshold as white and every background as black, and
 *     the dark background masks the mono flat-field banding.
 *
 * Usage example (a complete tile):
 * @code
 *   lv_obj_t   *content = wua_tile(parent, "Gauge", 48, 48);
 *   wua_gauge_t *gauge  = wua_gauge(content, 85, 0, 100);
 *   wua_gauge_set(gauge, 42);
 * @endcode
 */
#pragma once
#include <lvgl.h>
#include <stdint.h>

/* ── Theme: the application's visual identity ────────────────────────────────
 * The primitives own the MECHANISM — resolution-independent sizing, the
 * flat-field dither that suppresses TMDS banding, redraw suppression that keeps
 * the pixel link quiet.  The colors are POLICY and belong to the application,
 * so they live in a theme it can replace.
 *
 * The tile background is filled with a fine two-shade dither texture rather
 * than a flat color: a perfectly constant field makes the HDMI TMDS encoder
 * emit faint periodic banding (regular vertical lines) even in RGB565.  The
 * dither alternates two shades ±1 LSB around the base color — they average to
 * the base (invisible) but give every pixel neighborhood variation, so the
 * flat-field pattern never forms.  A gradient was tried first but posterizes
 * in RGB565 into flat bands that band again; the dither breaks it uniformly. */

/** Colors a screen is built from.  See wua_theme_set(). */
typedef struct {
    lv_color_t bg;     /**< Screen background.                              */
    lv_color_t tile;   /**< Panel base color.                               */
    lv_color_t text;   /**< Primary text.                                   */
    lv_color_t dim;    /**< Secondary text, borders and scale marks.        */
    lv_color_t accent; /**< Needles, bar indicators, highlights.            */
    lv_color_t track;  /**< Meter and scale tracks.                         */
} wua_theme_t;

/** The default identity (Wualabs): dark panels, white text, amber accent. */
extern const wua_theme_t WUA_THEME_DEFAULT;

/**
 * @brief Replace the active theme.
 *
 * REJECTS a theme that would be unusable in the monochrome modes.  Those modes
 * reduce every color to one bit by luminance, so a foreground that thresholds
 * to black — or a background that thresholds to white — disappears.  Failing
 * here is far better than discovering it after switching resolution.
 *
 * Call before creating widgets; existing objects keep the colors they were
 * built with.
 *
 * @param theme  Palette to adopt (copied; the caller may free it).
 * @return true if adopted; false if not monochrome-safe (theme unchanged).
 */
bool wua_theme_set(const wua_theme_t *theme);

/**
 * @brief The active theme, for application code that draws its own widgets.
 * @return Never NULL; the default until wua_theme_set() succeeds.
 */
const wua_theme_t *wua_theme(void);

/** Round gauge handle (see wua_gauge()). */
typedef struct {
    lv_obj_t *wrapper;  /**< Row container holding scale + readout.      */
    lv_obj_t *scale;    /**< The lv_scale (round). */
    lv_obj_t *needle;   /**< Needle line object.                         */
    lv_obj_t *label;    /**< Fixed-width numeric readout.                */
    int32_t needle_len; /**< Needle length in pixels.                    */
    int32_t last_value; /**< Last shown value (skip redundant updates).  */
} wua_gauge_t;

/** Linear meter handle (see wua_meter()). */
typedef struct {
    lv_obj_t *wrapper;  /**< Column container holding readout + bar.     */
    lv_obj_t *bar;      /**< The lv_bar.                                 */
    lv_obj_t *label;    /**< Fixed-width percentage readout.             */
    int32_t last_value; /**< Last shown value (skip redundant updates).  */
} wua_meter_t;

/**
 * @brief Initialize the primitives (padding unit, handle pools).
 *
 * Call once after lvgl_port_init() and before creating any primitive.
 */
void wua_ui_init(void);

/**
 * @brief Base padding/radius unit for the active resolution.
 * @return Pixels (grows with the screen: 4 px at 240 lines, 10 px at 600).
 */
int32_t wua_pad(void);

/**
 * @brief Largest enabled Montserrat font whose nominal size fits a height.
 * @param px  Available text height in pixels.
 * @return Font to use (never NULL; falls back to the smallest font).
 */
const lv_font_t *wua_font_fit(int32_t px);

/**
 * @brief Bare transparent container: no theme panel, no scrolling.
 * @param parent  Parent object.
 * @return The new container.
 */
lv_obj_t *wua_container(lv_obj_t *parent);

/**
 * @brief Container that stacks its children vertically, with the standard gap.
 *
 * Prefer this over wua_container() whenever a container has more than one
 * child: wua_container() carries NO layout, so its children all land at the
 * same position and silently overlap.
 *
 * Height defaults to its content, so it takes exactly the room its children
 * need and never steals space from a sibling that is meant to grow.
 *
 * @param parent  Parent object.
 * @return The new container.
 */
lv_obj_t *wua_column(lv_obj_t *parent);

/**
 * @brief Container that lays its children out in a row, wrapping as needed.
 *
 * The row counterpart of wua_column(); same reasoning. Useful as a tile grid,
 * where it wraps tiles onto as many lines as their widths need.
 *
 * @param parent  Parent object.
 * @return The new container.
 */
lv_obj_t *wua_row(lv_obj_t *parent);

/**
 * @brief Styled text label with the font picked from a screen-height ratio.
 * @param parent      Parent object.
 * @param text        Initial text.
 * @param height_pct  Desired text height as % of wua_screen_h() (e.g. 6).
 * @param color       Text color (usually from wua_theme()).
 * @return The new label.
 */
lv_obj_t *wua_label(lv_obj_t *parent, const char *text,
                    int32_t height_pct, lv_color_t color);

/**
 * @brief Numeric/value label whose width is FIXED to its widest content.
 *
 * The label reserves the horizontal space of @p max_text once, so later
 * value changes never resize it and never reflow the surrounding layout —
 * this is what keeps gauges and clocks from "dancing" on screen.  The font
 * is picked from @p height_pct and stepped down automatically if the widest
 * content would not fit the parent's content width (a value label never
 * clips, whatever container it lands in).
 *
 * @param parent      Parent object (its layout is resolved to measure the
 *                    available width — create it after sizing the parent).
 * @param max_text    Widest text the label will ever show (e.g. "100 %",
 *                    "00:00:00").  Also the initial text.
 * @param height_pct  Desired text height as % of wua_screen_h().
 * @return The new label (update it with lv_label_set_text_fmt()).
 */
lv_obj_t *wua_value_label(lv_obj_t *parent, const char *max_text,
                          int32_t height_pct);

/**
 * @brief Titled panel: bordered tile with a small caption at the top-left.
 *
 * The background is filled with @p bg_color as a fine two-shade dither (not a
 * flat fill) to avoid the TMDS flat-field banding; use a color dark enough
 * that white text contrasts and that thresholds to black in the mono modes.
 *
 * @param parent    Parent object (usually a flex-wrap grid).
 * @param title     Caption text, or NULL for an untitled panel.
 * @param w_pct     Tile width, % of the parent content width.
 * @param h_pct     Tile height, % of the parent content height.
 * @param bg_color  Tile background base color.
 * @return The tile's content container (flex column, centered) to populate.
 */
lv_obj_t *wua_tile(lv_obj_t *parent, const char *title,
                   int32_t w_pct, int32_t h_pct, lv_color_t bg_color);

/**
 * @brief Round gauge: scale + needle + fixed-width numeric readout.
 *
 * The diameter is a percentage of the SMALLER side of @p parent's content
 * area (resolved after a forced layout pass), so the same call fills its
 * tile evenly at every resolution.  Ticks, needle and readout font all
 * scale with the resulting diameter.
 *
 * @param parent        Parent container (e.g. a wua_tile() content).
 * @param diameter_pct  Diameter as % of min(parent content w, h), e.g. 85.
 * @param min           Scale minimum value.
 * @param max           Scale maximum value.
 * @return Gauge handle (owned by the module; valid for the app lifetime),
 *         or NULL if the handle pool is exhausted.
 */
wua_gauge_t *wua_gauge(lv_obj_t *parent, int32_t diameter_pct,
                       int32_t min, int32_t max);

/**
 * @brief Move a gauge to a value (needle + readout, redundancy-filtered).
 * @param gauge  Handle from wua_gauge().
 * @param value  New value (clamped by the scale range).
 */
void wua_gauge_set(wua_gauge_t *gauge, int32_t value);

/**
 * @brief Linear meter: fixed-width percentage readout over a styled bar.
 * @param parent     Parent container (e.g. a wua_tile() content).
 * @param width_pct  Meter width as % of the parent content width, e.g. 85.
 * @param min        Bar minimum value.
 * @param max        Bar maximum value.
 * @return Meter handle (owned by the module; valid for the app lifetime),
 *         or NULL if the handle pool is exhausted.
 */
wua_meter_t *wua_meter(lv_obj_t *parent, int32_t width_pct,
                       int32_t min, int32_t max);

/**
 * @brief Move a meter to a value (bar + readout, redundancy-filtered).
 * @param meter  Handle from wua_meter().
 * @param value  New value (clamped by the bar range).
 */
void wua_meter_set(wua_meter_t *meter, int32_t value);

/**
 * @brief HH:MM:SS clock label with a fixed width (never reflows).
 * @param parent      Parent object.
 * @param height_pct  Desired text height as % of wua_screen_h().
 * @return The clock label; update it with wua_clock_set().
 */
lv_obj_t *wua_clock(lv_obj_t *parent, int32_t height_pct);

/**
 * @brief Format and show a second count as HH:MM:SS (hours wrap at 100).
 * @param clock    Label returned by wua_clock().
 * @param seconds  Seconds to display.
 */
void wua_clock_set(lv_obj_t *clock, uint32_t seconds);
