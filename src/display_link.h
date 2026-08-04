/**
 * @file display_link.h
 * @brief Framebuffer rect stream master (SPI, 25 MHz) — the pixel path from
 *        LVGL to the RP2354B display engine.
 *
 * Implements the master side of include/display_protocol.h on the shared
 * SPI bus.  Only valid once rp_manager_bring_up() has switched the RP to
 * display mode; before that the bus belongs to the control link.
 *
 * Bus settings (must match the RP slave): mode 1 (CPOL=0, CPHA=1), MSB
 * first, 25 MHz.  CS is driven manually so the 12-byte header and the pixel
 * payload land in the same CS-low envelope — the RP's fixed-length DMA
 * counts on it.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "display_protocol.h"

/** SPI clock of the display stream (documented for the RP side). */
#define DISPLAY_LINK_SPI_HZ 25000000u

/**
 * Minimum quiet time between the end of one rect packet and the start of the
 * next.  The RP2354B needs it to blit and re-arm its fixed-length RX DMA
 * (8-byte SPI FIFO); without it, back-to-back packets get dropped and stale
 * areas remain on screen.  Hardware-validated at 4 ms during the JPEGDEC
 * bring-up — do not lower without re-testing the mono modes on the board.
 */
#define DISPLAY_LINK_RECT_GAP_US 4000u

/**
 * @brief Send one dirty rectangle to the RP (as one or more packets).
 *
 * @param x1      Left edge (inclusive), 0 ≤ x1 ≤ x2 < SCREEN_W.
 * @param y1      Top edge (inclusive), 0 ≤ y1 ≤ y2 < SCREEN_H.
 * @param x2      Right edge (inclusive).
 * @param y2      Bottom edge (inclusive).
 * @param pixels  rect_w*rect_h RGB565 little-endian pixels, row-major,
 *                top-left origin — exactly as LVGL renders.  In the mono
 *                modes they are luminance-thresholded and packed to
 *                1 bit/pixel internally before hitting the wire.
 *
 * Rects whose wire payload exceeds RECT_PAYLOAD_MAX are split internally
 * into horizontal bands, each sent as its own fixed-size packet inside its
 * own CS-low envelope (LVGL's partial renderer can emit tall narrow chunks
 * whose mono payload — rows padded to whole bytes — is larger than a
 * full-width 24-line strip).  Every packet is RECT_TOTAL_SIZE bytes: header
 * plus the payload zero-padded to RECT_PAYLOAD_MAX (the slave DMA needs a
 * fixed length), and the inter-packet gap is enforced between bands.
 *
 * Blocking; returns after the full transfer has been clocked out.
 *
 * @return true on success, false on invalid arguments (NULL pixels or
 *         out-of-range rect).
 */
bool display_link_send_rect(uint16_t x1, uint16_t y1,
                            uint16_t x2, uint16_t y2,
                            const uint8_t *pixels);

/**
 * @brief Get the RP chip temperature received over the telemetry channel.
 *
 * The RP piggybacks a telemetry packet on MISO at the start of every rect
 * transaction (see display_protocol.h); this returns the newest valid one.
 * No SPI traffic of its own — freshness depends on the rect stream running.
 *
 * @param temp_c10  Filled with the temperature in tenths of °C (e.g. 235 =
 *                  23.5 °C).  May be NULL.
 * @param age_ms    Filled with the milliseconds elapsed since that reading
 *                  arrived.  May be NULL.
 * @return true once at least one valid telemetry packet has been received.
 */
bool display_link_get_rp_temp(int16_t *temp_c10, uint32_t *age_ms);

/**
 * @brief Time since the last valid telemetry packet from the RP.
 *
 * The RP piggybacks a valid-magic telemetry packet on MISO on every rect
 * while it is in display mode, so this stays near zero during healthy
 * operation.  A large value means the RP has reset (back to its splash) and
 * is no longer receiving the rect stream — the main loop uses this to detect
 * a dead RP and re-run the bring-up.
 *
 * @return Milliseconds since the last valid telemetry, or 0 before the first
 *         one has ever arrived (so it never trips the watchdog at startup).
 */
uint32_t display_link_telemetry_age(void);
