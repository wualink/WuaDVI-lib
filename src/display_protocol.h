/**
 * @file display_protocol.h
 * @brief Framebuffer rect protocol — must match WuaDVI-rp-lite's
 *        include/frame_config.h byte for byte.
 *
 * Once the RP2354B has switched to display mode (see link_protocol.h), the
 * ESP32 streams the LVGL output as dirty rectangles.  Every SPI transaction
 * is exactly RECT_TOTAL_SIZE bytes inside one CS-low envelope:
 *
 *   Offset  Size   Field
 *   ------  ----   -----
 *      0      4    RECT_MAGIC_0..3  (0xA5 0xB6 0xC7 0xE9)
 *      4      2    x1               (uint16 little-endian)   inclusive rect
 *      6      2    y1               (uint16 little-endian)   bounds, top-left
 *      8      2    x2               (uint16 little-endian)   origin
 *     10      2    y2               (uint16 little-endian)
 *     12      N    pixel payload, zero-padded to RECT_PAYLOAD_MAX
 *
 * The fixed total size lets the RP use a fixed-length DMA transfer — the
 * padding trades wire time for zero per-packet reconfiguration.
 *
 * Pixel payload format:
 *   - RGB565 modes: rect_w*rect_h*2 bytes, row-major, RGB565 little-endian
 *     (exactly as LVGL renders with LV_COLOR_FORMAT_RGB565).
 *   - Mono modes: 1 bit/pixel, MSB first, each rect row padded to a whole
 *     byte.  LVGL still renders RGB565; the sender thresholds each pixel by
 *     luminance (WUADVI_MONO_BIT) and packs on the fly.
 */
#pragma once
#include <stdint.h>
#include "wuadvi_config.h"

/**
 * @brief RGB565 → 1-bit conversion by luminance threshold.
 *
 * Returns 1 (lit) or 0 (dark).  MUST match the RP2354B's threshold
 * (WuaDVI::lum_bit in its dvi_config.h) so both ends agree on which pixels
 * are white.
 *
 * @param c  RGB565 pixel value.
 */
#define WUADVI_MONO_BIT(c) \
    ((uint8_t)((((((c) >> 8) & 0xF8) * 30u + (((c) >> 3) & 0xFC) * 59u + (((c) << 3) & 0xF8) * 11u) / 100u) >= 128u ? 1u : 0u))

/* ── Rect protocol constants ─────────────────────────────────────────────── */
#define RECT_MAGIC_0 0xA5u
#define RECT_MAGIC_1 0xB6u
#define RECT_MAGIC_2 0xC7u
#define RECT_MAGIC_3 0xE9u

/** Header: 4 B magic + 4 × uint16 coords = 12 bytes. */
#define RECT_HEADER_SIZE 12u

/*
 * A full-width strip of PARTIAL_BUF_LINES lines is the unit of transfer.
 *
 * Two sizes derive from it:
 *   LV_PARTIAL_BUF_BYTES  LVGL's render buffer — always RGB565 (2 B/px),
 *                         since LVGL renders RGB565 regardless of the wire.
 *   RECT_PAYLOAD_MAX      max pixel bytes actually sent per rect.  Byte
 *                         modes use WIRE_BYTES_PER_PIXEL; mono packs
 *                         1 bit/pixel with byte-padded rows.
 * In the RGB565 modes both sizes are equal.
 */
#define PARTIAL_BUF_LINES 24u

/* WORST CASE across every mode, for statically sized buffers.  The live sizes
 * are runtime values — wua_rect_payload_max(), wua_rect_total_size() and
 * wua_partial_buf_bytes() in wua_resolution.h — because the mode is chosen at
 * runtime.  The widest COLOUR mode dominates the payload (400 px x 24 lines x
 * 2 B); the widest mono mode packs to far less (1280 px -> 160 B/row). */
#define RECT_PAYLOAD_MAX_ABS ((uint32_t)400u * (PARTIAL_BUF_LINES) * 2u)
#define RECT_TOTAL_SIZE_MAX  ((uint32_t)(RECT_HEADER_SIZE) + (RECT_PAYLOAD_MAX_ABS))

/* ── Telemetry back-channel (RP → ESP32 on MISO) ────────────────────────────
 * The rect stream only uses MOSI; the RP's TX line would otherwise carry
 * zeros.  Instead, the FIRST TELEM_SIZE bytes the RP clocks out in every
 * rect transaction form a telemetry packet (the rest of its TX transfer
 * remains zeros):
 *
 *   Offset  Size  Field
 *   ------  ----  -----
 *      0      1   TELEM_MAGIC0 (0x5A)
 *      1      1   TELEM_MAGIC1 (0xC3)
 *      2      2   chip temperature, int16 LE, tenths of °C (analogReadTemp)
 *      4      1   sequence number (increments on every sensor update)
 *      5      1   reserved (0)
 *      6      1   XOR checksum of bytes 0..5
 *      7      1   0x00
 *
 * display_link captures MISO while clocking the rect header out and hunts
 * for the magic at small bit offsets before trusting the packet — the PL022
 * 1-bit slave skew affects this direction too.
 */
#define TELEM_MAGIC0 0x5Au
#define TELEM_MAGIC1 0xC3u
#define TELEM_SIZE   8u
/** Temperature value meaning "sensor unavailable / invalid reading". */
#define TELEM_TEMP_INVALID ((int16_t)0x8000)
