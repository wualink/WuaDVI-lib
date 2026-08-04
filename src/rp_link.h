/**
 * @file rp_link.h
 * @brief Control-link master: version handshake with the RP2354B firmware.
 *
 * Implements the master side of include/link_protocol.h over the shared SPI
 * bus (spi_bus) at 1 MHz.  Used only during boot, before the display stream
 * starts — once DISPLAY_START is sent the RP stops serving this protocol.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

/** Decoded PONG response of the RP firmware. */
typedef struct {
    uint8_t major;  /**< Firmware version, major.                    */
    uint8_t minor;  /**< Firmware version, minor.                    */
    uint8_t patch;  /**< Firmware version, patch.                    */
    uint8_t mode;   /**< LINK_MODE_SPLASH / LINK_MODE_READY.         */
    uint8_t res_id; /**< LINK_RES_* resolution the RP was built for. */
} rp_link_info_t;

/**
 * @brief Probe the RP firmware with a PING and read its PONG.
 *
 * Blocking for LINK_PONG_DELAY_MS (~25 ms).
 *
 * @param out  Filled with the decoded PONG on success (may be NULL).
 * @return true if a magic- and checksum-valid PONG was received.
 */
bool rp_link_probe(rp_link_info_t *out);

/**
 * @brief Tell the RP which display mode to run.
 *
 * The RP firmware is universal (one image, every resolution compiled in) and
 * boots with its DVI off, waiting for this command.  On receiving it the RP
 * applies that mode's clocks and voltage, allocates the framebuffer, starts
 * the DVI and shows its splash.  Only acted on while the RP is
 * UNCONFIGURED — the resolution is fixed for the session.
 *
 * @param res_id  LINK_RES_* id (this build's WUADVI_LINK_RES_ID).
 */
void rp_link_set_resolution(uint8_t res_id);

/**
 * @brief Command the RP to switch to the framebuffer display stream.
 *
 * Only honored by the RP in READY mode.  After calling this, stay off the
 * SPI bus for LINK_DISPLAY_SWITCH_MS before sending the first rect packet
 * (rp_manager handles that wait).
 */
void rp_link_display_start(void);
