/**
 * @file rp_manager.h
 * @brief RP2354B bring-up state machine: reset → probe → flash-if-needed →
 *        READY → DISPLAY_START.
 *
 * This is the module that guarantees the display engine runs the firmware
 * embedded in this build (assets/rp-firmware, see tools/embed_rp_firmware.py)
 * before the LVGL stream starts:
 *
 *   1. Normal reset of the RP, then PING probing over the control link.
 *   2. If it answers with the SAME version and resolution as the embedded
 *      payload → nothing to flash (the everyday path, ~1 s).
 *   3. If it does not answer (blank/broken RP) or reports a different
 *      version/resolution → flash through ROM UART boot (rp_boot) and
 *      re-probe, up to WUADVI_MAX_FLASH_ATTEMPTS times.
 *   4. Wait for the RP to report READY (its boot splash minimum time), send
 *      DISPLAY_START and wait LINK_DISPLAY_SWITCH_MS.
 *
 * After a successful bring-up the RP no longer serves the control link — the
 * bus belongs to the display stream (display_link).
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "rp_link.h"

/** Bring-up result details. */
typedef struct {
    bool alive;           /**< RP runs the expected firmware and is in
                                      display mode.                          */
    rp_link_info_t info;  /**< Last PONG decoded during bring-up.     */
    uint32_t flash_count; /**< UART-boot flash cycles performed.      */
} rp_manager_status_t;

/**
 * @brief Run the full bring-up sequence (blocking, seconds to ~1 min worst
 *        case when flashing with verify enabled).
 *
 * Requires spi_bus_init() to have been called.  Safe to call again after a
 * failure — every attempt starts with a clean RP reset.
 *
 * @param out  Filled with the bring-up outcome (may be NULL).
 * @return true when the RP is verified and switched to display mode.
 */
bool rp_manager_bring_up(rp_manager_status_t *out);
