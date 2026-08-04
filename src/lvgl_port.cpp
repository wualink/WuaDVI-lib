/**
 * @file lvgl_port.cpp
 * @brief LVGL glue — see lvgl_port.h.
 */
#include <Arduino.h>
#include <lvgl.h>
#include <esp_heap_caps.h>
#include "lvgl_port.h"
#include "display_link.h"
#include "wuadvi_config.h"
#include "wua_resolution.h"

static uint8_t *s_partial_buf = nullptr;
static lv_display_t *s_disp = nullptr;
/* Flush and readers both run from loop() context — no volatile needed. */
static uint32_t s_rects_sent = 0;
static uint32_t s_rects_failed = 0;
static uint32_t s_last_full_ms = 0;

/**
 * @brief LVGL tick source (milliseconds since boot).
 * @return millis().
 */
static uint32_t lvgl_tick_cb(void) {
    return millis();
}

/**
 * @brief Forward LVGL log lines to the USB-CDC console.
 *
 * LV_LOG_PRINTF is disabled on purpose: printf would land on UART0, whose TX
 * pin is wired to the RP2354B QSPI_SD3 strap (see lv_conf.h).  Routing the
 * logs here makes LVGL warnings — e.g. a skipped image draw — visible on the
 * monitor for the first time.
 *
 * @param level  LVGL log level (unused; the level is part of the text).
 * @param buf    Formatted, NUL-terminated log line.
 */
static void lvgl_log_cb(lv_log_level_t level, const char *buf) {
    LV_UNUSED(level);
    Serial.print("[LVGL] ");
    Serial.println(buf);
}

/**
 * @brief Ordered-dither a freshly rendered rect in place (RGB565 modes only).
 *
 * A perfectly constant color region makes the HDMI TMDS encoder emit faint
 * periodic banding (regular vertical lines) even in 16-bit color.  Nudging
 * each pixel by at most 1 LSB per channel, in a 4x4 ordered pattern keyed to
 * the pixel's absolute position, gives every neighborhood variation so the
 * flat-field pattern never forms — while staying invisible (≤1 LSB) and not
 * touching high-contrast content like text.  Done here, on the pixels
 * themselves, it is independent of how LVGL fills backgrounds (a tiled
 * background image did not render in this setup, and a gradient posterizes).
 *
 * Near-black pixels are left untouched: the black screen background does not
 * band, and the +1 nudge (it can only add on black, never subtract) would
 * turn it into visible speckle — the very-first step out of black is the most
 * noticeable one.
 *
 * @param px    Rect pixels, RGB565 little-endian, row-major (modified in place).
 * @param area  Rect bounds (inclusive) — absolute screen coords for the pattern.
 */
static void dither_rect(uint8_t *px, const lv_area_t *area) {
    /* 4x4 Bayer, split into three 1-bit "add +1 here" masks (one per channel)
     * so R, G and B dither on different sub-patterns. */
    static const uint8_t bayer[4][4] = {
        {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    uint16_t *p = (uint16_t *)px;
    const int32_t w = area->x2 - area->x1 + 1;
    const int32_t h = area->y2 - area->y1 + 1;
    for (int32_t ry = 0; ry < h; ++ry) {
        const int32_t ay = area->y1 + ry;
        for (int32_t rx = 0; rx < w; ++rx) {
            uint16_t c = p[(size_t)ry * w + rx];
            int r = (c >> 11) & 0x1F, g = (c >> 5) & 0x3F, b = c & 0x1F;
            if (r < 2 && g < 4 && b < 2)
                continue; /* leave (near-)black alone */
            const uint8_t t = bayer[ay & 3][(area->x1 + rx) & 3];
            if ((t & 1u) && r < 31)
                ++r;
            if ((t & 2u) && g < 63)
                ++g;
            if ((t & 4u) && b < 31)
                ++b;
            p[(size_t)ry * w + rx] = (uint16_t)((r << 11) | (g << 5) | b);
        }
    }
}

/**
 * @brief LVGL flush callback: ship one dirty rect to the RP2354B.
 *
 * In partial mode px_map points to the rect's pixels packed row-major
 * (rect_w * rect_h * 2 bytes of RGB565).  The rect coords travel with the
 * pixels so the RP blits them at the right position of its framebuffer.
 *
 * @param disp    LVGL display.
 * @param area    Dirty rect bounds (inclusive).
 * @param px_map  Rendered pixels of that rect.
 */
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    /* Break the TMDS flat-field banding before the pixels go out.  Only the
     * colour modes need it: the mono packing thresholds to 1 bit anyway, and
     * a +/-1 LSB nudge there would only flip pixels near the threshold. */
    if (!wua_screen_is_mono())
        dither_rect(px_map, area);
    if (display_link_send_rect((uint16_t)area->x1, (uint16_t)area->y1,
                               (uint16_t)area->x2, (uint16_t)area->y2,
                               px_map)) {
        ++s_rects_sent;
    } else {
        /* A rejected rect means a bug upstream (invalid coords) — make it
         * loud instead of silently losing screen content. */
        ++s_rects_failed;
        Serial.printf("[LVGL] flush REJECTED rect (%d,%d)-(%d,%d)\n",
                      (int)area->x1, (int)area->y1, (int)area->x2, (int)area->y2);
    }
    lv_display_flush_ready(disp);
}

bool lvgl_port_init(void) {
    /* LVGL renders RGB565 (2 B/px) in every mode, so the render buffer is
     * wua_partial_buf_bytes(); in the mono modes the wire payload is smaller
     * (the packing happens inside display_link). */
    s_partial_buf = (uint8_t *)heap_caps_malloc(wua_partial_buf_bytes(), MALLOC_CAP_8BIT);
    if (s_partial_buf == nullptr) {
        size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        Serial.printf("[LVGL] render buffer alloc failed (%lu B, largest free block %u B)\n",
                      (unsigned long)wua_partial_buf_bytes(), (unsigned)largest);
        return false;
    }

    lv_init();
    lv_tick_set_cb(lvgl_tick_cb);
    lv_log_register_print_cb(lvgl_log_cb);

    s_disp = lv_display_create(wua_screen_w(), wua_screen_h());
    if (s_disp == nullptr) {
        Serial.println("[LVGL] lv_display_create failed");
        return false;
    }

    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(s_disp,
                           s_partial_buf, nullptr,
                           wua_partial_buf_bytes(),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_disp, lvgl_flush_cb);

    /* First self-healing refresh due one full period from now — the initial
     * paint already covers the whole screen, and an immediate extra repaint
     * would only double the startup burst on the SPI link. */
    s_last_full_ms = millis();

    Serial.printf("[LVGL] ready: %ux%u, render buffer %lu B (%u lines)\n",
                  wua_screen_w(), wua_screen_h(),
                  (unsigned long)wua_partial_buf_bytes(), (unsigned)PARTIAL_BUF_LINES);
    return true;
}

uint32_t lvgl_port_loop(void) {
    uint32_t next_ms = lv_timer_handler();
    if (next_ms > 100u)
        next_ms = 100u;

#if WUADVI_FULL_REFRESH_MS > 0
    /* Self-healing: re-send the whole screen periodically.  The pixels are
     * identical so nothing visibly changes, but the picture recovers from
     * any rect lost to noise or glitches.  With the inter-rect gap enforced
     * by display_link this is pure insurance, hence the slow period. */
    uint32_t now = millis();
    if (now - s_last_full_ms >= WUADVI_FULL_REFRESH_MS) {
        s_last_full_ms = now;
        lv_obj_invalidate(lv_screen_active());
    }
#endif

    return next_ms;
}

uint32_t lvgl_port_rects_sent(void) {
    return s_rects_sent;
}

uint32_t lvgl_port_rects_failed(void) {
    return s_rects_failed;
}
