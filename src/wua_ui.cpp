/**
 * @file wua_ui.cpp
 * @brief WuaDVI UI primitives implementation — see wua_ui.h.
 */
#include <Arduino.h>
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

    if (title != NULL)
        wua_label(tile, title, 6, wua_theme()->dim);

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
    if (d < 24)
        d = 24;

    g->wrapper = wua_container(parent);
    lv_obj_set_size(g->wrapper, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(g->wrapper, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g->wrapper, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(g->wrapper, s_pad * 2, 0);

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
     * value the range allows. */
    char max_text[12];
    snprintf(max_text, sizeof(max_text), "%ld", (long)max);
    const lv_font_t *font = wua_font_fit(d * 35 / 100);
    g->label = lv_label_create(g->wrapper);
    lv_obj_set_style_text_font(g->label, font, 0);
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
