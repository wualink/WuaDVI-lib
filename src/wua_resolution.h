/**
 * @file wua_resolution.h
 * @brief The display modes the board can run, and the one it is running.
 *
 * The mode is a RUNTIME choice: WuaDVI::begin() takes it and
 * WuaDVI::setResolution() changes it. Nothing here depends on a build flag.
 *
 * This mirrors the table the display-engine firmware already keeps on its side,
 * and the two must agree — the `id` values are the resolution ids of the
 * control-link protocol (see link_protocol.h).
 *
 * Two pixel pipelines exist, selected per mode by `mono`:
 *
 *   ── Full-colour modes (RGB565, 2 bytes/pixel) ────────────────────────────
 *   The engine pixel-doubles them on both axes, so square pixels are preserved.
 *   Limited by the framebuffer size in the engine's 520 KB SRAM:
 *
 *     320x240 = 150 KB  ->  scanned out as 640x480p60
 *     400x240 = 192 KB  ->  scanned out as 800x480p60
 *
 *   ── Monochrome modes (1 bit/pixel) ───────────────────────────────────────
 *   The 1-bit encoder is far cheaper, so it reaches native resolutions that
 *   full colour cannot (640x480 in RGB565 would need ~600 KB):
 *
 *     640x480   =  37.5 KB    800x600  = 58.6 KB    1280x720 = 112.5 KB
 *
 *   1280x720 runs at 30 Hz — at 60 Hz its TMDS bit clock would be ~742 MHz,
 *   far beyond the engine. 800x600 has the least scanout margin of the set.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/** Display modes. Values are the control-link resolution ids. */
typedef enum {
    WUA_RES_320x240 = 1,    /**< RGB565, scanned out as 640x480p60.       */
    WUA_RES_400x240 = 2,    /**< RGB565, scanned out as 800x480p60.       */
    WUA_RES_640x480x1 = 3,  /**< 1-bit monochrome, native 640x480p60.     */
    WUA_RES_800x600x1 = 4,  /**< 1-bit monochrome, native 800x600p60.     */
    WUA_RES_1280x720x1 = 5, /**< 1-bit monochrome, native 1280x720p30.    */
} wua_resolution_id_t;

/** Everything that distinguishes one mode from another. */
typedef struct {
    uint8_t id;       /**< wua_resolution_id_t / control-link id.       */
    uint16_t width;   /**< Logical width in pixels (the LVGL size).     */
    uint16_t height;  /**< Logical height in pixels.                    */
    bool mono;        /**< true = 1 bit/pixel on the wire, false RGB565.*/
    uint8_t wire_bpp; /**< Wire bytes per pixel in the byte-based modes.*/
    const char *name; /**< Human-readable caption, e.g. "640x480 mono". */
} wua_resolution_t;

/** Widest mode of any kind — dimensions the worst-case buffers must cover. */
#define WUA_RES_MAX_WIDTH 1280u
/** Widest COLOUR mode; the payload worst case, since RGB565 costs 2 B/px. */
#define WUA_RES_MAX_COLOR_WIDTH 400u

/**
 * @brief Look up a mode by its id.
 * @param id  A wua_resolution_id_t value.
 * @return The mode, or NULL if the id is unknown.
 */
const wua_resolution_t *wua_resolution_by_id(uint8_t id);

/**
 * @brief The mode currently running.
 * @return Never NULL — the default mode until begin() selects another.
 */
const wua_resolution_t *wua_resolution(void);

/**
 * @brief Adopt a mode and latch every size derived from it.
 *
 * Called by WuaDVI::begin(); an application selects the mode there rather than
 * here.
 *
 * @param id  A wua_resolution_id_t value.
 * @return false if the id is unknown (the active mode is left unchanged).
 */
bool wua_resolution_set_active(uint8_t id);

/** @return Active mode width in pixels. */
uint16_t wua_screen_w(void);

/** @return Active mode height in pixels. */
uint16_t wua_screen_h(void);

/** @return true when the active mode packs pixels to 1 bit on the wire. */
bool wua_screen_is_mono(void);

/* ── Sizes derived from the active mode ─────────────────────────────────────
 * Latched by wua_resolution_set_active() so the hot paths do no arithmetic. */

/** @return Packed bytes per pixel row in mono modes; 0 in colour modes. */
uint32_t wua_rect_row_bytes(void);

/** @return Maximum pixel bytes carried by one packet in the active mode. */
uint32_t wua_rect_payload_max(void);

/** @return Total wire size of one packet (header + padded payload). */
uint32_t wua_rect_total_size(void);

/** @return Bytes of the LVGL partial render buffer for the active mode. */
uint32_t wua_partial_buf_bytes(void);
