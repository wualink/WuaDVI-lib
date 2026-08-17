/**
 * @file wua_ui.cpp
 * @brief WuaDVI UI primitives implementation — see wua_ui.h.
 */
#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "wua_ui.h"
#include "wuadvi_config.h"
#include "wua_resolution.h"

/* ── Theme ───────────────────────────────────────────────────────────────── */
const wua_theme_t WUA_THEME_DEFAULT = {
    .bg = LV_COLOR_MAKE(0x00, 0x00, 0x00),     /* screen background        */
    .tile = LV_COLOR_MAKE(0x14, 0x1A, 0x21),   /* panel base               */
    .text = LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),   /* primary text             */
    .dim = LV_COLOR_MAKE(0xB8, 0xBE, 0xC6),    /* secondary text / borders */
    .accent = LV_COLOR_MAKE(0xFF, 0xB8, 0x00), /* Wualabs amber            */
    .track = LV_COLOR_MAKE(0x2A, 0x32, 0x3C),  /* meter / scale track      */
};

static wua_theme_t s_theme = WUA_THEME_DEFAULT;

/**
 * @brief Does this color survive the 1-bit reduction as lit?
 *
 * MUST match the threshold the display engine applies on the wire (ITU-R 601
 * luma, lit at >= 128), or a theme that passes here would still vanish there.
 */
static bool lum_is_lit(lv_color_t c) {
    const uint32_t y = (uint32_t)c.red * 30u + (uint32_t)c.green * 59u +
                       (uint32_t)c.blue * 11u;
    return (y / 100u) >= 128u;
}

bool wua_theme_set(const wua_theme_t *theme) {
    if (theme == NULL)
        return false;

    /* Foregrounds must stay visible and backgrounds must stay dark once the
     * monochrome modes collapse everything to one bit.  Refusing here turns a
     * blank screen three resolutions later into an error at startup. */
    if (!lum_is_lit(theme->text) || !lum_is_lit(theme->dim) ||
        !lum_is_lit(theme->accent)) {
        return false; /* a foreground would threshold to black */
    }
    if (lum_is_lit(theme->bg) || lum_is_lit(theme->tile) ||
        lum_is_lit(theme->track)) {
        return false; /* a background would threshold to white */
    }

    s_theme = *theme;
    return true;
}

const wua_theme_t *wua_theme(void) {
    return &s_theme;
}

/* ── Module state ────────────────────────────────────────────────────────── */
static int32_t s_pad = 4;

/* The TMDS flat-field banding of solid tile backgrounds is broken on the
 * pixels themselves (ordered dither in the LVGL flush, see lvgl_port.cpp),
 * so the tiles here are plain solid colors. */

/* Handles live in static pools — embedded-friendly, no heap fragmentation. */
#define WUA_UI_MAX_GAUGES 4
#define WUA_UI_MAX_METERS 4
static wua_gauge_t s_gauges[WUA_UI_MAX_GAUGES];
static uint8_t s_gauge_count = 0;
static wua_meter_t s_meters[WUA_UI_MAX_METERS];
static uint8_t s_meter_count = 0;

/** Enabled Montserrat table used by wua_font_fit(), ascending sizes. */
static const struct {
    int32_t size;
    const lv_font_t *font;
} s_fonts[] = {
    {8, &lv_font_montserrat_8},
    {10, &lv_font_montserrat_10},
    {12, &lv_font_montserrat_12},
    {14, &lv_font_montserrat_14},
    {18, &lv_font_montserrat_18},
    {24, &lv_font_montserrat_24},
    {32, &lv_font_montserrat_32},
    {48, &lv_font_montserrat_48},
};
#define WUA_FONT_COUNT (sizeof(s_fonts) / sizeof(s_fonts[0]))

void wua_ui_init(void) {
    /* One padding unit that grows with the screen: 4 px at 240 lines,
     * 8 px at 480, 10 px at 600. */
    s_pad = (int32_t)wua_screen_h() / 60;
    if (s_pad < 3)
        s_pad = 3;

    s_gauge_count = 0;
    s_meter_count = 0;
}

int32_t wua_pad(void) {
    return s_pad;
}

const lv_font_t *wua_font_fit(int32_t px) {
    const lv_font_t *best = s_fonts[0].font;
    for (size_t i = 0; i < WUA_FONT_COUNT; ++i) {
        if (s_fonts[i].size <= px)
            best = s_fonts[i].font;
    }
    return best;
}

/**
 * @brief Convert a screen-height percentage to the matching font.
 * @param height_pct  Desired text height as % of wua_screen_h().
 */
static const lv_font_t *font_for_pct(int32_t height_pct) {
    return wua_font_fit((int32_t)wua_screen_h() * height_pct / 100);
}

/* Defined with the widget primitives further down, needed by wua_tile(). */
static const lv_font_t *font_fit_text(const char *text, const lv_font_t *font,
                                      int32_t avail);

lv_obj_t *wua_container(lv_obj_t *parent) {
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    return c;
}

lv_obj_t *wua_column(lv_obj_t *parent) {
    lv_obj_t *c = wua_container(parent);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(c, s_pad, 0);
    /* Content height, so a column never takes room a growing sibling needs. */
    lv_obj_set_height(c, LV_SIZE_CONTENT);
    return c;
}

lv_obj_t *wua_row(lv_obj_t *parent) {
    lv_obj_t *c = wua_container(parent);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(c, s_pad, 0);
    lv_obj_set_style_pad_column(c, s_pad, 0);
    return c;
}

lv_obj_t *wua_label(lv_obj_t *parent, const char *text,
                    int32_t height_pct, lv_color_t color) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font_for_pct(height_pct), 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_label_set_text(l, text);
    return l;
}

lv_obj_t *wua_value_label(lv_obj_t *parent, const char *max_text,
                          int32_t height_pct) {
    const lv_font_t *font = font_for_pct(height_pct);

    /* Width-aware fit: if the widest content would not fit the parent's
     * content box at the height-picked font, step down until it does — a
     * value label must never clip, whatever tile it lands in. */
    lv_obj_update_layout(lv_screen_active());
    const int32_t avail = lv_obj_get_content_width(parent);
    lv_point_t size;
    lv_text_get_size(&size, max_text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    while (avail > 0 && size.x + 2 > avail && font != s_fonts[0].font) {
        size_t i = WUA_FONT_COUNT;
        while (i-- > 1) {
            if (s_fonts[i].font == font) {
                font = s_fonts[i - 1].font;
                break;
            }
        }
        lv_text_get_size(&size, max_text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    }

    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, wua_theme()->text, 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(l, max_text);

    /* Freeze the width at the widest content so later updates never resize
     * the label — the surrounding layout stays perfectly still. */
    lv_obj_set_width(l, size.x + 2);
    return l;
}

lv_obj_t *wua_tile(lv_obj_t *parent, const char *title,
                   int32_t w_pct, int32_t h_pct, lv_color_t bg_color) {
    lv_obj_t *tile = wua_container(parent);
    lv_obj_set_size(tile, lv_pct(w_pct), lv_pct(h_pct));
    /* Solid background (the flush-time dither breaks its TMDS banding). */
    lv_obj_set_style_bg_color(tile, bg_color, 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(tile, wua_theme()->dim, 0);
    lv_obj_set_style_border_width(tile, 1, 0);
    lv_obj_set_style_radius(tile, s_pad, 0);
    lv_obj_set_style_pad_all(tile, s_pad, 0);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    if (title != NULL) {
        /* The caption is fitted to the tile's WIDTH, not just picked from the
         * screen height.  A tile is usually the narrow dimension, and a word
         * as ordinary as "Dropdown" walks straight out of a fifth of a screen
         * otherwise.  The dots are the backstop: whatever the font ends up
         * being, the caption is bounded by the tile it belongs to. */
        lv_obj_update_layout(lv_screen_active());
        const int32_t inner = lv_obj_get_content_width(tile);

        lv_obj_t *cap = lv_label_create(tile);
        lv_obj_set_style_text_font(cap, font_fit_text(title, font_for_pct(6), inner),
                                   0);
        lv_obj_set_style_text_color(cap, wua_theme()->dim, 0);
        lv_label_set_long_mode(cap, LV_LABEL_LONG_MODE_DOTS);
        lv_label_set_text(cap, title);
        if (inner > 0)
            lv_obj_set_width(cap, inner);
    }

    lv_obj_t *content = wua_container(tile);
    lv_obj_set_width(content, lv_pct(100));
    lv_obj_set_flex_grow(content, 1);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return content;
}

/* ── Gauge ───────────────────────────────────────────────────────────────── */
wua_gauge_t *wua_gauge(lv_obj_t *parent, int32_t diameter_pct,
                       int32_t min, int32_t max) {
    if (s_gauge_count >= WUA_UI_MAX_GAUGES) {
        LV_LOG_WARN("wua_gauge: handle pool exhausted");
        return NULL;
    }
    wua_gauge_t *g = &s_gauges[s_gauge_count++];
    memset(g, 0, sizeof(*g));
    g->last_value = INT32_MIN;

    /* Resolve the parent's content box now so the diameter percentage maps
     * to real pixels (percent sizes are only known after a layout pass). */
    lv_obj_update_layout(lv_screen_active());
    const int32_t avail_w = lv_obj_get_content_width(parent);
    const int32_t avail_h = lv_obj_get_content_height(parent);
    int32_t d = LV_MIN(avail_w, avail_h) * diameter_pct / 100;

    /* The readout sits BESIDE the disc, not under it, so the diameter alone is
     * not the gauge's width — and a diameter taken from the smaller side spends
     * room the number still needs.  A tile wide enough absorbs that (1280x720
     * did); a narrower one pushes the readout out of the panel.
     *
     * Settle the pair rather than guessing once: the readout's font is derived
     * from the diameter, so shrinking the disc shrinks the number too and frees
     * more width than it costs.  Two passes is normally enough. */
    char max_text[12];
    snprintf(max_text, sizeof(max_text), "%ld", (long)max);
    for (int pass = 0; pass < 4; ++pass) {
        lv_point_t probe;
        lv_text_get_size(&probe, max_text, wua_font_fit(d * 35 / 100), 0, 0,
                         LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        const int32_t fits = avail_w - probe.x - 3 * s_pad;
        if (d <= fits)
            break;
        d = fits;
    }
    if (d < 24)
        d = 24;

    g->wrapper = wua_container(parent);
    lv_obj_set_size(g->wrapper, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    /* Side by side while there is room, stacked when there is not.  A gauge in
     * a narrow tile that insists on a row squeezes its own readout to nothing;
     * a column costs height, which a narrow tile usually has to spare. */
    const bool stack = (d + 4 * s_pad) > avail_w;
    lv_obj_set_flex_flow(g->wrapper,
                         stack ? LV_FLEX_FLOW_COLUMN : LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g->wrapper, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(g->wrapper, s_pad * 2, 0);
    lv_obj_set_style_pad_row(g->wrapper, s_pad, 0);

    g->scale = lv_scale_create(g->wrapper);
    lv_obj_set_size(g->scale, d, d);
    lv_scale_set_mode(g->scale, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_range(g->scale, min, max);
    lv_scale_set_total_tick_count(g->scale, 21);
    lv_scale_set_major_tick_every(g->scale, 5);
    lv_scale_set_label_show(g->scale, false);
    lv_scale_set_angle_range(g->scale, 270);
    lv_scale_set_rotation(g->scale, 135);
    lv_obj_set_style_arc_color(g->scale, wua_theme()->dim, LV_PART_MAIN);
    lv_obj_set_style_line_color(g->scale, wua_theme()->dim, LV_PART_MAIN);
    lv_obj_set_style_line_color(g->scale, wua_theme()->text, LV_PART_INDICATOR);
    lv_obj_set_style_line_color(g->scale, wua_theme()->dim, LV_PART_ITEMS);

    g->needle = lv_line_create(g->scale);
    int32_t needle_w = d / 24;
    if (needle_w < 2)
        needle_w = 2;
    lv_obj_set_style_line_width(g->needle, needle_w, 0);
    lv_obj_set_style_line_color(g->needle, wua_theme()->accent, 0);
    lv_obj_set_style_line_rounded(g->needle, true, 0);
    g->needle_len = d * 38 / 100;
    lv_scale_set_line_needle_value(g->scale, g->needle, g->needle_len, min);

    /* Readout font scales with the gauge disc; width frozen at the widest
     * value the range allows. max_text was measured above, where the diameter
     * was settled against it. */
    const lv_font_t *font = wua_font_fit(d * 35 / 100);
    g->label = lv_label_create(g->wrapper);
    lv_obj_set_style_text_font(g->label, font, 0);
    /* Never wrap.  The label carries a frozen width, but a flex row is free to
     * shrink it when the tile is small, and the default long mode then breaks
     * "100" across two lines — which reads as a broken widget rather than a
     * cramped one.  Clipping is honest; wrapping is not. */
    lv_label_set_long_mode(g->label, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_flex_grow(g->label, 0, 0);
    lv_obj_set_style_text_color(g->label, wua_theme()->text, 0);
    lv_obj_set_style_text_align(g->label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(g->label, max_text);
    lv_point_t size;
    lv_text_get_size(&size, max_text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_obj_set_width(g->label, size.x + 2);

    wua_gauge_set(g, min);
    return g;
}

void wua_gauge_set(wua_gauge_t *gauge, int32_t value) {
    if (gauge == NULL || value == gauge->last_value)
        return;
    gauge->last_value = value;
    lv_scale_set_line_needle_value(gauge->scale, gauge->needle,
                                   gauge->needle_len, value);
    lv_label_set_text_fmt(gauge->label, "%ld", (long)value);
}

/* ── Meter ───────────────────────────────────────────────────────────────── */
wua_meter_t *wua_meter(lv_obj_t *parent, int32_t width_pct,
                       int32_t min, int32_t max) {
    if (s_meter_count >= WUA_UI_MAX_METERS) {
        LV_LOG_WARN("wua_meter: handle pool exhausted");
        return NULL;
    }
    wua_meter_t *m = &s_meters[s_meter_count++];
    memset(m, 0, sizeof(*m));
    m->last_value = INT32_MIN;

    m->wrapper = wua_container(parent);
    lv_obj_set_width(m->wrapper, lv_pct(width_pct));
    lv_obj_set_height(m->wrapper, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(m->wrapper, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(m->wrapper, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(m->wrapper, s_pad, 0);

    char max_text[16];
    snprintf(max_text, sizeof(max_text), "%ld %%", (long)max);
    m->label = wua_value_label(m->wrapper, max_text, 11);

    /* Bar height grows with the screen (9 px at 240 lines, 24 at 600). */
    int32_t bar_h = (int32_t)wua_screen_h() / 25;
    if (bar_h < 8)
        bar_h = 8;
    m->bar = lv_bar_create(m->wrapper);
    lv_obj_set_size(m->bar, lv_pct(100), bar_h);
    lv_bar_set_range(m->bar, min, max);
    lv_obj_set_style_bg_color(m->bar, wua_theme()->track, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(m->bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(m->bar, wua_theme()->dim, LV_PART_MAIN);
    lv_obj_set_style_border_width(m->bar, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(m->bar, s_pad, LV_PART_MAIN);
    lv_obj_set_style_bg_color(m->bar, wua_theme()->accent, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(m->bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(m->bar, s_pad, LV_PART_INDICATOR);

    wua_meter_set(m, min);
    return m;
}

void wua_meter_set(wua_meter_t *meter, int32_t value) {
    if (meter == NULL || value == meter->last_value)
        return;
    meter->last_value = value;
    lv_bar_set_value(meter->bar, value, LV_ANIM_OFF);
    lv_label_set_text_fmt(meter->label, "%ld %%", (long)value);
}

/* ── Clock ───────────────────────────────────────────────────────────────── */
lv_obj_t *wua_clock(lv_obj_t *parent, int32_t height_pct) {
    return wua_value_label(parent, "00:00:00", height_pct);
}

void wua_clock_set(lv_obj_t *clock, uint32_t seconds) {
    lv_label_set_text_fmt(clock, "%02u:%02u:%02u",
                          (unsigned)((seconds / 3600u) % 100u),
                          (unsigned)((seconds / 60u) % 60u),
                          (unsigned)(seconds % 60u));
}

/* ── Standard widgets ────────────────────────────────────────────────────────
 *
 * Everything below exists because of one property of the monochrome modes: the
 * engine reduces each pixel to one bit by luminance, so two colours of similar
 * brightness become the same pixel.  LVGL's stock widgets are styled in shades
 * of one hue — a slider's track and its indicator, a switch's track and its
 * knob — and every one of those pairs collapses into a single flat shape.
 *
 * The fix that works in every mode is not a different palette but a different
 * construction: outline the background, and drive the moving part to full
 * white.  An outline is geometry, and geometry survives thresholding. */

/** Border width that stays visible at every resolution (1 px at 240 lines). */
static int32_t hairline(void) {
    int32_t w = (int32_t)wua_screen_h() / 240;
    return (w < 1) ? 1 : w;
}

/** The colour a moving part must take: white in mono, the caller's otherwise. */
static lv_color_t lit(lv_color_t requested) {
    return wua_screen_is_mono() ? wua_theme()->text : requested;
}

/** The colour a background must take so a lit part stands out against it. */
static lv_color_t unlit(lv_color_t requested) {
    return wua_screen_is_mono() ? wua_theme()->bg : requested;
}

/** Outline a part so its extent is readable once colour is gone. */
static void outline(lv_obj_t *o, lv_part_t part, lv_color_t color) {
    lv_obj_set_style_border_color(o, color, part);
    lv_obj_set_style_border_width(o, hairline(), part);
    lv_obj_set_style_border_opa(o, LV_OPA_COVER, part);
}

/** Resolve a percentage of the parent content box into pixels. */
static void resolve_parent(lv_obj_t *parent, int32_t *w, int32_t *h) {
    lv_obj_update_layout(lv_screen_active());
    *w = lv_obj_get_content_width(parent);
    *h = lv_obj_get_content_height(parent);
}

int32_t wua_fit(lv_obj_t *parent, int32_t pct) {
    int32_t avail_w, avail_h;
    resolve_parent(parent, &avail_w, &avail_h);
    int32_t d = LV_MIN(avail_w, avail_h) * pct / 100;
    if (d < 16)
        d = 16;
    return d;
}

/** Pixels for a percentage of the parent's content width/height. */
static void fit_box(lv_obj_t *parent, int32_t w_pct, int32_t h_pct,
                    int32_t *out_w, int32_t *out_h) {
    int32_t avail_w, avail_h;
    resolve_parent(parent, &avail_w, &avail_h);
    *out_w = avail_w * w_pct / 100;
    *out_h = avail_h * h_pct / 100;
    if (*out_w < 12)
        *out_w = 12;
    if (*out_h < 6)
        *out_h = 6;
}

/** A font that fits inside a box @p h px tall, with room for ascenders. */
static const lv_font_t *font_in(int32_t h) {
    return wua_font_fit(h * 60 / 100);
}

/**
 * @brief Step a font down until @p text fits @p avail px wide.
 *
 * A height-picked font says nothing about width, and a tile is usually the
 * narrow dimension: "Enabled" beside a tick box, or the longest option of a
 * dropdown plus its arrow, is what actually overflows a panel.
 *
 * @param text   Widest string that must fit.
 * @param font   Starting font (from the height).
 * @param avail  Width available in pixels; <= 0 means unconstrained.
 * @return A font whose rendering of @p text fits, or the smallest available.
 */
static const lv_font_t *font_fit_text(const char *text, const lv_font_t *font,
                                      int32_t avail) {
    if (text == NULL || avail <= 0)
        return font;

    lv_point_t size;
    lv_text_get_size(&size, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    while (size.x > avail && font != s_fonts[0].font) {
        size_t i = WUA_FONT_COUNT;
        while (i-- > 1) {
            if (s_fonts[i].font == font) {
                font = s_fonts[i - 1].font;
                break;
            }
        }
        lv_text_get_size(&size, text, font, 0, 0, LV_COORD_MAX,
                         LV_TEXT_FLAG_NONE);
    }
    return font;
}

/** The longest line of a newline-separated option list. */
static const char *longest_option(const char *options, char *buf, size_t cap) {
    size_t best_len = 0;
    const char *best = options;
    const char *p = options;
    while (p != NULL && *p != '\0') {
        const char *nl = strchr(p, '\n');
        const size_t len = (nl != NULL) ? (size_t)(nl - p) : strlen(p);
        if (len > best_len) {
            best_len = len;
            best = p;
        }
        p = (nl != NULL) ? nl + 1 : NULL;
    }
    if (best_len >= cap)
        best_len = cap - 1;
    memcpy(buf, best, best_len);
    buf[best_len] = '\0';
    return buf;
}

lv_obj_t *wua_button(lv_obj_t *parent, const char *text,
                     int32_t w_pct, int32_t h_pct, lv_color_t color) {
    int32_t w, h;
    fit_box(parent, w_pct, h_pct, &w, &h);

    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, unlit(color), LV_PART_MAIN);
    lv_obj_set_style_radius(b, h / 6, LV_PART_MAIN);
    outline(b, LV_PART_MAIN, wua_theme()->text);

    /* Pressed inverts rather than darkening: a shade change is exactly what
     * the 1-bit reduction throws away, an inversion is what it keeps. */
    lv_obj_set_style_bg_color(b, lit(color), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(b, wua_theme()->bg,
                                LV_PART_MAIN | LV_STATE_PRESSED);

    /* The caption colour is set on the BUTTON, not on the label, and left to
     * inherit.  A child does not receive its parent's states, so a pressed
     * style written onto the label would never fire — the caption stayed white
     * on a white face and the button looked blank while pressed. */
    lv_obj_set_style_text_color(b, wua_theme()->text, LV_PART_MAIN);

    if (text != NULL) {
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, text);
        lv_obj_set_style_text_font(
            l, font_fit_text(text, font_in(h), w - 2 * s_pad), 0);
        lv_obj_center(l);
    }
    return b;
}

lv_obj_t *wua_checkbox(lv_obj_t *parent, const char *text, int32_t h_pct) {
    int32_t w, h;
    fit_box(parent, 100, h_pct, &w, &h);

    lv_obj_t *c = lv_checkbox_create(parent);
    lv_checkbox_set_text(c, (text != NULL) ? text : "");

    /* A checkbox sizes itself to its content, so a font chosen from the height
     * alone walks straight out of a narrow tile — box and caption with it.
     * Reserve room for the tick box, then fit the caption to what is left. */
    int32_t avail_w, avail_h;
    resolve_parent(parent, &avail_w, &avail_h);

    /* The tick box is drawn at the font's own line height, so how much room is
     * left for the caption depends on the font still being chosen.  Settle it
     * instead of guessing once: fit, then re-measure against what the chosen
     * font actually reserves, until it stops moving. */
    const lv_font_t *font = font_in(h);
    for (int pass = 0; pass < 4; ++pass) {
        const int32_t box = lv_font_get_line_height(font) + 3 * s_pad;
        const lv_font_t *next = font_fit_text(text, font, avail_w - box);
        if (next == font)
            break;
        font = next;
    }
    lv_obj_set_style_text_font(c, font, 0);
    lv_obj_set_style_text_color(c, wua_theme()->text, 0);
    lv_obj_set_style_pad_all(c, 0, LV_PART_MAIN);

    /* Content width and NO max_width.  A max width does not clip a checkbox,
     * it wraps it: LVGL breaks the caption onto a second line, which then
     * pushes out of the panel vertically instead of horizontally.  The font
     * fit above is what keeps it inside; this keeps it on one line. */
    lv_obj_set_width(c, LV_SIZE_CONTENT);
    lv_obj_set_style_flex_grow(c, 0, 0);

    /* The tick box: outlined when clear, filled when set, so both states are
     * shapes rather than shades. */
    lv_obj_set_style_bg_color(c, wua_theme()->bg, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, LV_PART_INDICATOR);
    outline(c, LV_PART_INDICATOR, wua_theme()->text);
    lv_obj_set_style_bg_color(c, wua_theme()->text,
                              LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(c, wua_theme()->bg,
                                LV_PART_INDICATOR | LV_STATE_CHECKED);
    return c;
}

lv_obj_t *wua_switch(lv_obj_t *parent, int32_t w_pct, int32_t h_pct) {
    int32_t w, h;
    fit_box(parent, w_pct, h_pct, &w, &h);

    lv_obj_t *s = lv_switch_create(parent);
    lv_obj_set_size(s, w, h);

    /* The track, which a stock switch leaves as a shade and mono erases. */
    lv_obj_set_style_bg_color(s, wua_theme()->bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, LV_PART_MAIN);
    outline(s, LV_PART_MAIN, wua_theme()->text);

    lv_obj_set_style_bg_color(s, wua_theme()->dim, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s, wua_theme()->accent,
                              LV_PART_INDICATOR | LV_STATE_CHECKED);

    /* The knob carries a dark rim so it stays a distinct shape even when it
     * sits on top of a lit indicator. */
    lv_obj_set_style_bg_color(s, wua_theme()->text, LV_PART_KNOB);
    outline(s, LV_PART_KNOB, wua_theme()->bg);
    return s;
}

lv_obj_t *wua_led(lv_obj_t *parent, int32_t size_pct, lv_color_t color) {
    const int32_t d = wua_fit(parent, size_pct);

    /* Deliberately NOT lv_led.  That widget renders its own fill and scales
     * every colour it draws — border included — by its brightness, so the
     * outline meant to show an unlit lamp faded away with the fill and "off"
     * became nothing at all.  A plain object gives the ring a life of its own:
     * always drawn, with only the fill following the state. */
    lv_obj_t *l = wua_container(parent);
    lv_obj_set_size(l, d, d);
    lv_obj_set_style_radius(l, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(l, wua_theme()->bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(l, LV_OPA_COVER, LV_PART_MAIN);
    outline(l, LV_PART_MAIN, wua_theme()->text);

    lv_obj_set_style_bg_color(l, lit(color), LV_PART_MAIN | LV_STATE_CHECKED);
    return l;
}

/** Shared track/fill/knob styling for the linear indicators. */
static void style_track(lv_obj_t *o, int32_t h, lv_color_t color, bool knob) {
    lv_obj_set_style_bg_color(o, wua_theme()->bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(o, h / 2, LV_PART_MAIN);
    outline(o, LV_PART_MAIN, wua_theme()->text);

    lv_obj_set_style_bg_color(o, lit(color), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(o, h / 2, LV_PART_INDICATOR);

    if (knob) {
        lv_obj_set_style_bg_color(o, wua_theme()->text, LV_PART_KNOB);
        outline(o, LV_PART_KNOB, wua_theme()->bg);
    }
}

lv_obj_t *wua_slider(lv_obj_t *parent, int32_t w_pct, int32_t h_pct,
                     int32_t min, int32_t max, lv_color_t color) {
    int32_t w, h;
    fit_box(parent, w_pct, h_pct, &w, &h);

    lv_obj_t *s = lv_slider_create(parent);

    /* The knob is a circle of the track's height centred on the track's end,
     * so it hangs h/2 past each side.  Sizing the track to the full width is
     * what pushed the slider out of its tile: take the overhang off first. */
    lv_obj_set_size(s, (w > h) ? (w - h) : w, h);
    lv_slider_set_range(s, min, max);
    style_track(s, h, color, true);
    lv_obj_set_style_pad_all(s, 0, LV_PART_KNOB);
    return s;
}

lv_obj_t *wua_bar(lv_obj_t *parent, int32_t w_pct, int32_t h_pct,
                  int32_t min, int32_t max, lv_color_t color) {
    int32_t w, h;
    fit_box(parent, w_pct, h_pct, &w, &h);

    lv_obj_t *b = lv_bar_create(parent);
    lv_obj_set_size(b, w, h);
    lv_bar_set_range(b, min, max);
    style_track(b, h, color, false);
    return b;
}

lv_obj_t *wua_arc(lv_obj_t *parent, int32_t size_pct,
                  int32_t min, int32_t max, lv_color_t color) {
    const int32_t d = wua_fit(parent, size_pct);
    int32_t aw = d / 10;
    if (aw < 2)
        aw = 2;

    lv_obj_t *a = lv_arc_create(parent);
    lv_obj_set_size(a, d, d);
    lv_arc_set_range(a, min, max);
    lv_arc_set_bg_angles(a, 135, 45);
    lv_arc_set_rotation(a, 0);

    /* The track uses unlit(), not dim.  The theme's dim is a light grey, which
     * the 1-bit threshold rounds UP to white — so track and indicator both
     * came out white and the arc read as a solid ring with no value in it. */
    lv_obj_set_style_arc_color(a, unlit(wua_theme()->dim), LV_PART_MAIN);
    lv_obj_set_style_arc_width(a, aw, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(a, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(a, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_color(a, lit(color), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(a, aw, LV_PART_INDICATOR);

    /* No input device, so the knob is a thing that cannot be dragged and only
     * costs contrast where the indicator ends. */
    lv_obj_remove_style(a, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
    return a;
}

lv_obj_t *wua_spinner(lv_obj_t *parent, int32_t size_pct, uint32_t period_ms) {
    const int32_t d = wua_fit(parent, size_pct);
    int32_t aw = d / 10;
    if (aw < 2)
        aw = 2;

    lv_obj_t *s = lv_spinner_create(parent);
    lv_obj_set_size(s, d, d);
    lv_spinner_set_anim_params(s, (int32_t)period_ms, 60);

    /* Same reason as wua_arc(): a dim track thresholds to white in the mono
     * modes and the spinner becomes a plain filled ring with nothing moving. */
    lv_obj_set_style_arc_color(s, unlit(wua_theme()->dim), LV_PART_MAIN);
    lv_obj_set_style_arc_width(s, aw, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(s, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s, wua_theme()->text, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s, aw, LV_PART_INDICATOR);
    return s;
}

lv_obj_t *wua_roller(lv_obj_t *parent, const char *options,
                     int32_t w_pct, int32_t rows) {
    int32_t w, h;
    fit_box(parent, w_pct, 100, &w, &h);
    if (rows < 1)
        rows = 1;

    lv_obj_t *r = lv_roller_create(parent);
    lv_roller_set_options(r, (options != NULL) ? options : "",
                          LV_ROLLER_MODE_NORMAL);

    /* Size the font from the row height the tile can actually give, then from
     * the width the longest option needs, so options are clipped by neither. */
    char widest[24];
    const lv_font_t *font = font_in(h / (rows + 1));
    font = font_fit_text(longest_option((options != NULL) ? options : "",
                                        widest, sizeof(widest)),
                         font, w - 4 * s_pad);
    lv_obj_set_style_text_font(r, font, LV_PART_MAIN);
    lv_obj_set_style_text_font(r, font, LV_PART_SELECTED);
    lv_roller_set_visible_row_count(r, (uint32_t)rows);
    lv_obj_set_width(r, w);

    lv_obj_set_style_bg_color(r, wua_theme()->bg, LV_PART_MAIN);
    lv_obj_set_style_text_color(r, wua_theme()->dim, LV_PART_MAIN);
    outline(r, LV_PART_MAIN, wua_theme()->text);

    /* Selection as an inversion, for the same reason the button uses one. */
    lv_obj_set_style_bg_color(r, wua_theme()->text, LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_PART_SELECTED);
    lv_obj_set_style_text_color(r, wua_theme()->bg, LV_PART_SELECTED);
    return r;
}

lv_obj_t *wua_dropdown(lv_obj_t *parent, const char *options, int32_t w_pct) {
    int32_t w, h;
    fit_box(parent, w_pct, 100, &w, &h);

    lv_obj_t *d = lv_dropdown_create(parent);
    lv_dropdown_set_options(d, (options != NULL) ? options : "");
    lv_obj_set_width(d, w);

    /* The arrow is drawn inside the right padding, so padding the arrow's
     * width only walks it inwards — it does not reserve a column for it.  Fit
     * the font to the width the longest option leaves once the symbol and the
     * ordinary padding are accounted for, and leave the padding alone. */
    char widest[24];
    const char *longest =
        longest_option((options != NULL) ? options : "", widest, sizeof(widest));
    const lv_font_t *font = font_in(h / 4);
    for (int pass = 0; pass < 4; ++pass) {
        /* The arrow is a glyph of the same font, so its width moves with every
         * step down — settle the pair rather than reserving once. */
        const int32_t arrow = lv_font_get_line_height(font) + 4 * s_pad;
        const lv_font_t *next = font_fit_text(longest, font, w - arrow);
        if (next == font)
            break;
        font = next;
    }

    lv_obj_set_style_text_font(d, font, LV_PART_MAIN);
    lv_obj_set_style_bg_color(d, wua_theme()->bg, LV_PART_MAIN);
    lv_obj_set_style_text_color(d, wua_theme()->text, LV_PART_MAIN);
    lv_obj_set_style_pad_all(d, s_pad, LV_PART_MAIN);
    outline(d, LV_PART_MAIN, wua_theme()->text);

    lv_obj_t *list = lv_dropdown_get_list(d);
    lv_obj_set_style_text_font(list, font, LV_PART_MAIN);
    lv_obj_set_style_bg_color(list, wua_theme()->bg, LV_PART_MAIN);
    lv_obj_set_style_text_color(list, wua_theme()->text, LV_PART_MAIN);
    outline(list, LV_PART_MAIN, wua_theme()->text);
    return d;
}

/* ── Writing a sketch without LVGL ───────────────────────────────────────── */

wua_color_t wua_color(uint32_t rgb) {
    return lv_color_hex(rgb);
}

wua_obj_t *wua_screen(void) {
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, wua_theme()->bg, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr, s_pad, 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(scr, s_pad, 0);
    return scr;
}

wua_obj_t *wua_header(wua_obj_t *parent) {
    lv_obj_t *h = wua_column(parent);
    lv_obj_set_width(h, lv_pct(100));
    return h;
}

wua_obj_t *wua_grid(wua_obj_t *parent) {
    lv_obj_t *g = wua_row(parent);
    lv_obj_set_width(g, lv_pct(100));
    lv_obj_set_flex_grow(g, 1); /* every row the header did not take */
    return g;
}

/* ── Setting values ─────────────────────────────────────────────────────── */

/** Add or remove a state, the shape every boolean setter here shares. */
static void set_state(lv_obj_t *o, lv_state_t state, int32_t on) {
    if (o == NULL)
        return;
    if (on != 0)
        lv_obj_add_state(o, state);
    else
        lv_obj_remove_state(o, state);
}

void wua_button_set_pressed(wua_obj_t *button, int32_t pressed) {
    set_state(button, LV_STATE_PRESSED, pressed);
}

void wua_checkbox_set(wua_obj_t *checkbox, int32_t checked) {
    set_state(checkbox, LV_STATE_CHECKED, checked);
}

void wua_switch_set(wua_obj_t *sw, int32_t on) {
    set_state(sw, LV_STATE_CHECKED, on);
}

void wua_led_set(wua_obj_t *led, int32_t on) {
    set_state(led, LV_STATE_CHECKED, on);
}

void wua_slider_set(wua_obj_t *slider, int32_t value) {
    if (slider != NULL)
        lv_slider_set_value(slider, value, LV_ANIM_OFF);
}

void wua_bar_set(wua_obj_t *bar, int32_t value) {
    if (bar != NULL)
        lv_bar_set_value(bar, value, LV_ANIM_OFF);
}

void wua_arc_set(wua_obj_t *arc, int32_t value) {
    if (arc != NULL)
        lv_arc_set_value(arc, value);
}

void wua_roller_select(wua_obj_t *roller, int32_t index) {
    if (roller != NULL && index >= 0)
        lv_roller_set_selected(roller, (uint32_t)index, LV_ANIM_ON);
}

void wua_dropdown_select(wua_obj_t *dropdown, int32_t index) {
    if (dropdown != NULL && index >= 0)
        lv_dropdown_set_selected(dropdown, (uint32_t)index);
}

/* ── Making things happen ───────────────────────────────────────────────── */

/** Trampoline: LVGL hands the timer back, the application's callback wants
 *  nothing, and the function pointer rides in the timer's user data. */
static void timer_trampoline(lv_timer_t *t) {
    wua_timer_cb_t cb = (wua_timer_cb_t)lv_timer_get_user_data(t);
    if (cb != NULL)
        cb();
}

void wua_timer(uint32_t period_ms, wua_timer_cb_t cb) {
    if (cb == NULL)
        return;
    lv_timer_create(timer_trampoline, period_ms, (void *)cb);
}

void wua_sweep(wua_obj_t *target, wua_anim_cb_t cb,
               int32_t from, int32_t to, uint32_t period_ms) {
    if (target == NULL || cb == NULL)
        return;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, target);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, period_ms);
    lv_anim_set_playback_duration(&a, period_ms);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);

    /* The setters take wua_obj_t* where LVGL passes void*; the animation's var
     * is that same pointer, so the cast only renames the parameter's type.
     * This is the idiom LVGL's own examples use for exec callbacks. */
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)cb);
    lv_anim_start(&a);
}

/* ── The rest of what a sketch needs ────────────────────────────────────── */

void wua_align(wua_obj_t *obj, wua_align_t how) {
    if (obj == NULL)
        return;
    const lv_flex_align_t a = (how == WUA_ALIGN_CENTER) ? LV_FLEX_ALIGN_CENTER
                                                        : LV_FLEX_ALIGN_START;
    lv_obj_set_flex_align(obj, a, a, a);
}

void wua_label_set(wua_obj_t *label, const char *text) {
    if (label != NULL && text != NULL)
        lv_label_set_text(label, text);
}

void wua_label_setf(wua_obj_t *label, const char *fmt, ...) {
    if (label == NULL || fmt == NULL)
        return;

    /* Formatted on the stack rather than through lv_label_set_text_fmt(),
     * which is variadic and offers no va_list form to forward to. */
    char buf[96];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    lv_label_set_text(label, buf);
}

/* The composite widgets keep their own handles, so their setters do not have
 * the wua_anim_cb_t shape.  Each gets a sweep of its own rather than making
 * every call site cast. */

static void gauge_anim(void *var, int32_t v) {
    wua_gauge_set((wua_gauge_t *)var, v);
}

static void meter_anim(void *var, int32_t v) {
    wua_meter_set((wua_meter_t *)var, v);
}

/** Shared body: an endless there-and-back animation over @p var. */
static void sweep_var(void *var, lv_anim_exec_xcb_t exec_cb, int32_t from,
                      int32_t to, uint32_t period_ms) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, var);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, period_ms);
    lv_anim_set_playback_duration(&a, period_ms);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&a, exec_cb);
    lv_anim_start(&a);
}

void wua_gauge_sweep(wua_gauge_t *gauge, int32_t from, int32_t to,
                     uint32_t period_ms) {
    if (gauge != NULL)
        sweep_var(gauge, gauge_anim, from, to, period_ms);
}

void wua_meter_sweep(wua_meter_t *meter, int32_t from, int32_t to,
                     uint32_t period_ms) {
    if (meter != NULL)
        sweep_var(meter, meter_anim, from, to, period_ms);
}

void wua_clear(wua_obj_t *obj) {
    if (obj != NULL)
        lv_obj_clean(obj);
}

void wua_settle(void) {
    lv_obj_update_layout(lv_screen_active());
}
