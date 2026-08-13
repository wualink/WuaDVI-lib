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

/* ── Standard widgets ───────────────────────────────────────────────────────
 *
 * Wrappers around the LVGL widgets, sized in percentages of the parent and
 * styled to stay legible in EVERY display mode.  Use these rather than the
 * lv_*_create() calls they wrap.
 *
 * The reason is the monochrome modes.  There, every colour collapses to black
 * or white by luminance, so colour cannot separate one part of a widget from
 * another: a slider whose track and indicator are both mid-tone becomes an
 * empty rectangle, and an LED that is "off" disappears rather than looking
 * off.  Each primitive therefore outlines its background and drives its
 * indicator to full white when the active mode is 1-bit, and uses the colour
 * you pass when it is not.  Hairline widths and fonts scale with the mode, so
 * the same call works from 320x240 to 1280x720.
 *
 * All of them return the plain LVGL object: drive it with the ordinary
 * lv_slider_set_value(), lv_bar_set_value(), lv_obj_add_state() and friends.
 * The primitives own construction and appearance, not behaviour.
 *
 * Percentages are of the PARENT's content box, resolved immediately, so a
 * parent must be sized before its widget is created — the same rule wua_tile()
 * and wua_gauge() already follow.
 */

/**
 * @brief Pixels for a percentage of the smaller side of a parent's content box.
 *
 * The sizing rule the square widgets use, exposed because an application
 * building its own composite needs the same answer.
 *
 * @param parent  Object whose content box is measured.
 * @param pct     Percentage of the smaller side.
 * @return Size in pixels, never below a usable minimum.
 */
int32_t wua_fit(lv_obj_t *parent, int32_t pct);

/**
 * @brief Push button with a centred caption.
 * @param parent  Parent object.
 * @param text    Caption, or NULL for a bare button.
 * @param w_pct   Width as % of the parent content width.
 * @param h_pct   Height as % of the parent content height.
 * @param color   Face colour; ignored in the monochrome modes.
 * @return The button (add LV_STATE_PRESSED to show it pressed).
 */
lv_obj_t *wua_button(lv_obj_t *parent, const char *text,
                     int32_t w_pct, int32_t h_pct, lv_color_t color);

/**
 * @brief Checkbox with a label.
 * @param parent  Parent object.
 * @param text    Label text.
 * @param h_pct   Text height as % of the parent content height.
 * @return The checkbox (toggle with LV_STATE_CHECKED).
 */
lv_obj_t *wua_checkbox(lv_obj_t *parent, const char *text, int32_t h_pct);

/**
 * @brief On/off switch with a delimited track.
 * @param parent  Parent object.
 * @param w_pct   Width as % of the parent content width.
 * @param h_pct   Height as % of the parent content height.
 * @return The switch (toggle with LV_STATE_CHECKED).
 */
lv_obj_t *wua_switch(lv_obj_t *parent, int32_t w_pct, int32_t h_pct);

/**
 * @brief Indicator LED that is visible when off as well as on.
 *
 * The ring is always drawn, so "off" reads as an unlit lamp rather than as
 * nothing at all — which is what a bare lv_led does in the mono modes.
 *
 * @param parent    Parent object.
 * @param size_pct  Diameter as % of the smaller side of the parent.
 * @param color     Lit colour; ignored in the monochrome modes.
 * @return The LED (drive it with lv_led_on(), lv_led_off()).
 */
lv_obj_t *wua_led(lv_obj_t *parent, int32_t size_pct, lv_color_t color);

/**
 * @brief Slider with a visible track, fill and knob.
 * @param parent  Parent object.
 * @param w_pct   Width as % of the parent content width.
 * @param h_pct   Track thickness as % of the parent content height.
 * @param min     Range minimum.
 * @param max     Range maximum.
 * @param color   Fill colour; ignored in the monochrome modes.
 * @return The slider (drive it with lv_slider_set_value()).
 */
lv_obj_t *wua_slider(lv_obj_t *parent, int32_t w_pct, int32_t h_pct,
                     int32_t min, int32_t max, lv_color_t color);

/**
 * @brief Progress bar with a visible track and fill.
 *
 * The plain bar. wua_meter() is the composite that pairs one with a numeric
 * readout; this is the widget on its own.
 *
 * @param parent  Parent object.
 * @param w_pct   Width as % of the parent content width.
 * @param h_pct   Height as % of the parent content height.
 * @param min     Range minimum.
 * @param max     Range maximum.
 * @param color   Fill colour; ignored in the monochrome modes.
 * @return The bar (drive it with lv_bar_set_value()).
 */
lv_obj_t *wua_bar(lv_obj_t *parent, int32_t w_pct, int32_t h_pct,
                  int32_t min, int32_t max, lv_color_t color);

/**
 * @brief Circular arc indicator, knobless (there is no input device).
 * @param parent    Parent object.
 * @param size_pct  Diameter as % of the smaller side of the parent.
 * @param min       Range minimum.
 * @param max       Range maximum.
 * @param color     Indicator colour; ignored in the monochrome modes.
 * @return The arc (drive it with lv_arc_set_value()).
 */
lv_obj_t *wua_arc(lv_obj_t *parent, int32_t size_pct,
                  int32_t min, int32_t max, lv_color_t color);

/**
 * @brief Indeterminate activity spinner.
 * @param parent     Parent object.
 * @param size_pct   Diameter as % of the smaller side of the parent.
 * @param period_ms  Time for one full revolution.
 * @return The spinner (it animates itself).
 */
lv_obj_t *wua_spinner(lv_obj_t *parent, int32_t size_pct, uint32_t period_ms);

/**
 * @brief Scrolling option list with the selection highlighted.
 * @param parent   Parent object.
 * @param options  Newline-separated options, e.g. "Idle\nRun\nFault".
 * @param w_pct    Width as % of the parent content width.
 * @param rows     Options visible at once (1 or more).
 * @return The roller (drive it with lv_roller_set_selected()).
 */
lv_obj_t *wua_roller(lv_obj_t *parent, const char *options,
                     int32_t w_pct, int32_t rows);

/**
 * @brief Dropdown showing its current selection.
 *
 * Room is reserved for the arrow, so the selected text cannot run underneath
 * it. Without an input device the list never opens, so what this shows is the
 * closed state.
 *
 * @param parent   Parent object.
 * @param options  Newline-separated options.
 * @param w_pct    Width as % of the parent content width.
 * @return The dropdown (drive it with lv_dropdown_set_selected()).
 */
lv_obj_t *wua_dropdown(lv_obj_t *parent, const char *options, int32_t w_pct);

/* ── Writing a sketch without LVGL ──────────────────────────────────────────
 *
 * Creating widgets is not the whole of an interface: it also has a screen, a
 * layout, values that change, a clock that changes them, and colours. If the
 * library stops at construction, an application still has to reach into LVGL
 * for the rest — and then it is back to guessing which calls are safe in the
 * monochrome modes, which was the problem the primitives exist to remove.
 *
 * What follows completes the set, so a sketch can be written end to end
 * without naming LVGL once. Anything underneath still works: these are the
 * same objects, and an application that wants LVGL directly is free to use it.
 */

/** An on-screen object. The library's name for what LVGL calls an lv_obj_t. */
typedef lv_obj_t wua_obj_t;

/** A colour. */
typedef lv_color_t wua_color_t;

/** Callback for wua_timer(). */
typedef void (*wua_timer_cb_t)(void);

/**
 * @brief Callback for wua_sweep(): applies a value to a widget.
 *
 * Every `wua_*_set()` below matches this shape, so a setter can be passed
 * straight to wua_sweep() rather than wrapped in a shim.
 */
typedef void (*wua_anim_cb_t)(wua_obj_t *target, int32_t value);

/**
 * @brief A colour from a 24-bit RGB value, e.g. wua_color(0x0E5A50).
 * @param rgb  0xRRGGBB.
 * @return The colour.
 */
wua_color_t wua_color(uint32_t rgb);

/**
 * @brief The active screen, themed and ready for a column of content.
 *
 * Applies the theme background, one padding unit, and a vertical flow, so a
 * sketch starts from a styled root instead of a bare one.
 *
 * @return The screen; add wua_header() and wua_grid() to it.
 */
wua_obj_t *wua_screen(void);

/**
 * @brief Full-width band at the top of a screen, as tall as its content.
 * @param parent  Usually wua_screen().
 * @return The header; fill it with wua_label().
 */
wua_obj_t *wua_header(wua_obj_t *parent);

/**
 * @brief Full-width wrapping area that takes every row the header left.
 * @param parent  Usually wua_screen().
 * @return The grid; fill it with wua_tile().
 */
wua_obj_t *wua_grid(wua_obj_t *parent);

/* ── Setting values ─────────────────────────────────────────────────────────
 * One per widget, all shaped for wua_sweep(). The booleans take an int so the
 * signatures stay uniform; any non-zero value is "on". */

/** @brief Show a button pressed (non-zero) or released. */
void wua_button_set_pressed(wua_obj_t *button, int32_t pressed);

/** @brief Check (non-zero) or clear a checkbox. */
void wua_checkbox_set(wua_obj_t *checkbox, int32_t checked);

/** @brief Turn a switch on (non-zero) or off. */
void wua_switch_set(wua_obj_t *sw, int32_t on);

/** @brief Light an LED (non-zero) or leave it unlit. */
void wua_led_set(wua_obj_t *led, int32_t on);

/** @brief Move a slider to @p value (clamped to its range). */
void wua_slider_set(wua_obj_t *slider, int32_t value);

/** @brief Move a bar to @p value (clamped to its range). */
void wua_bar_set(wua_obj_t *bar, int32_t value);

/** @brief Move an arc to @p value (clamped to its range). */
void wua_arc_set(wua_obj_t *arc, int32_t value);

/** @brief Select a roller option by index, scrolling to it. */
void wua_roller_select(wua_obj_t *roller, int32_t index);

/** @brief Select a dropdown option by index. */
void wua_dropdown_select(wua_obj_t *dropdown, int32_t index);

/* ── Making things happen ───────────────────────────────────────────────── */

/**
 * @brief Call @p cb every @p period_ms, forever.
 *
 * For state that steps: a mode changing, a reading being polled. For a value
 * that should look continuous use wua_sweep() instead — a timer slow enough to
 * be cheap is also slow enough to look like stuttering.
 *
 * @param period_ms  Interval in milliseconds.
 * @param cb         Function to call.
 */
void wua_timer(uint32_t period_ms, wua_timer_cb_t cb);

/**
 * @brief Sweep a widget between two values, forever, back and forth.
 *
 * The sweep steps at the display refresh rate, so motion is smooth — and a
 * pixel link that has actually stopped shows as a freeze rather than being
 * hidden by an update slow enough to look broken anyway.
 *
 * @code
 *   wua_sweep(slider, wua_slider_set, 0, 100, 2600);
 * @endcode
 *
 * @param target     Widget to drive.
 * @param cb         Setter to apply, e.g. wua_slider_set.
 * @param from       Value at the start of each pass.
 * @param to         Value at the end of each pass.
 * @param period_ms  Duration of one pass; a round trip takes twice this.
 */
void wua_sweep(wua_obj_t *target, wua_anim_cb_t cb,
               int32_t from, int32_t to, uint32_t period_ms);

/* ── The rest of what a sketch needs ────────────────────────────────────── */

/**
 * @brief A colour from 8-bit components, usable in a constant initialiser.
 *
 * wua_color() is a function, so it cannot initialise a `static const` theme at
 * file scope. This macro can:
 *
 * @code
 *   static const wua_theme_t kTheme = { .bg = WUA_RGB(0x00, 0x00, 0x00), ... };
 * @endcode
 */
#define WUA_RGB(r, g, b) LV_COLOR_MAKE(r, g, b)

/** How a container arranges its content. */
typedef enum {
    WUA_ALIGN_START,  /**< Pack to the top-left.            */
    WUA_ALIGN_CENTER, /**< Centre on both axes.             */
} wua_align_t;

/**
 * @brief Set how a container arranges its children.
 * @param obj  Container, typically from wua_tile() or wua_column().
 * @param how  Alignment to apply.
 */
void wua_align(wua_obj_t *obj, wua_align_t how);

/**
 * @brief Replace a label's text.
 * @param label  Label from wua_label() or wua_value_label().
 * @param text   New text (copied by the label).
 */
void wua_label_set(wua_obj_t *label, const char *text);

/**
 * @brief Replace a label's text, printf-style.
 *
 * @code
 *   wua_label_setf(temp, "%d.%d C", c10 / 10, abs(c10 % 10));
 * @endcode
 *
 * @param label  Label from wua_label() or wua_value_label().
 * @param fmt    printf format string, followed by its arguments.
 */
void wua_label_setf(wua_obj_t *label, const char *fmt, ...);

/**
 * @brief Sweep a gauge between two values, forever, back and forth.
 *
 * wua_sweep() drives the plain widgets; a gauge is a composite with its own
 * handle, so it gets its own call rather than a cast at the call site.
 *
 * @param gauge      Handle from wua_gauge().
 * @param from       Value at the start of each pass.
 * @param to         Value at the end of each pass.
 * @param period_ms  Duration of one pass; a round trip takes twice this.
 */
void wua_gauge_sweep(wua_gauge_t *gauge, int32_t from, int32_t to,
                     uint32_t period_ms);

/**
 * @brief Sweep a meter between two values, forever, back and forth.
 * @param meter      Handle from wua_meter().
 * @param from       Value at the start of each pass.
 * @param to         Value at the end of each pass.
 * @param period_ms  Duration of one pass; a round trip takes twice this.
 */
void wua_meter_sweep(wua_meter_t *meter, int32_t from, int32_t to,
                     uint32_t period_ms);
