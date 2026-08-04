/**
 * @file wua_resolution.cpp
 * @brief Display mode table and active-mode state — see wua_resolution.h.
 */
#include "wua_resolution.h"
#include "display_protocol.h"

/* Every mode the board can run.  Order is irrelevant; lookup is by id.
 * Hand-aligned into columns; kept out of clang-format so it stays readable. */
// clang-format off
static const wua_resolution_t s_modes[] = {
    { WUA_RES_320x240,    320, 240, false, 2, "320x240 RGB565"  },
    { WUA_RES_400x240,    400, 240, false, 2, "400x240 RGB565"  },
    { WUA_RES_640x480x1,  640, 480, true,  1, "640x480 mono"    },
    { WUA_RES_800x600x1,  800, 600, true,  1, "800x600 mono"    },
    { WUA_RES_1280x720x1, 1280, 720, true, 1, "1280x720 mono"   },
};
// clang-format on
#define MODE_COUNT (sizeof(s_modes) / sizeof(s_modes[0]))

/* 640x480x1 by default: the mode with the most scanout margin of the set, so a
 * sketch that never picks one still gets the most reliable picture. */
static const wua_resolution_t *s_active = &s_modes[2];

/* Sizes derived from the active mode, latched so the hot paths do no maths. */
static uint32_t s_row_bytes = 0;
static uint32_t s_payload_max = 0;
static uint32_t s_total_size = 0;
static uint32_t s_partial_buf = 0;

/** Recompute everything that depends on the active mode. */
static void latch_sizes(void) {
    const wua_resolution_t *r = s_active;

    if (r->mono) {
        /* 1 bit/pixel, MSB first, each row padded to a whole byte so the
         * engine addresses rows with a fixed stride. */
        s_row_bytes = ((uint32_t)r->width + 7u) / 8u;
        s_payload_max = s_row_bytes * PARTIAL_BUF_LINES;
    } else {
        s_row_bytes = 0;
        s_payload_max = (uint32_t)r->width * PARTIAL_BUF_LINES * r->wire_bpp;
    }
    s_total_size = RECT_HEADER_SIZE + s_payload_max;

    /* LVGL always renders RGB565 regardless of what goes on the wire — the
     * mono packing happens on the way out. */
    s_partial_buf = (uint32_t)r->width * PARTIAL_BUF_LINES * 2u;
}

/** Latch the default mode before anything asks for a size. */
static const bool s_initialized = (latch_sizes(), true);

const wua_resolution_t *wua_resolution_by_id(uint8_t id) {
    for (uint32_t i = 0; i < MODE_COUNT; ++i) {
        if (s_modes[i].id == id)
            return &s_modes[i];
    }
    return nullptr;
}

const wua_resolution_t *wua_resolution(void) {
    (void)s_initialized;
    return s_active;
}

bool wua_resolution_set_active(uint8_t id) {
    const wua_resolution_t *r = wua_resolution_by_id(id);
    if (r == nullptr)
        return false;
    s_active = r;
    latch_sizes();
    return true;
}

uint16_t wua_screen_w(void) {
    return s_active->width;
}

uint16_t wua_screen_h(void) {
    return s_active->height;
}

bool wua_screen_is_mono(void) {
    return s_active->mono;
}

uint32_t wua_rect_row_bytes(void) {
    return s_row_bytes;
}

uint32_t wua_rect_payload_max(void) {
    return s_payload_max;
}

uint32_t wua_rect_total_size(void) {
    return s_total_size;
}

uint32_t wua_partial_buf_bytes(void) {
    return s_partial_buf;
}
