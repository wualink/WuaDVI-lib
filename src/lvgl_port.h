/**
 * @file lvgl_port.h
 * @brief LVGL glue: display registration, tick source and the flush that
 *        ships dirty rects to the RP2354B.
 *
 * LVGL runs in LV_DISPLAY_RENDER_MODE_PARTIAL with one small render buffer
 * (LV_PARTIAL_BUF_BYTES = a full-width strip of 24 lines): a full RGB565
 * framebuffer would not fit the ESP32-C3 RAM at the larger resolutions, and
 * the RP keeps the real framebuffer anyway.  Every flush callback ships one
 * dirty rect over display_link.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize LVGL: render buffer, display, tick and flush callback.
 *
 * Call once the RP is in display mode (after rp_manager_bring_up()).
 *
 * @return true on success, false when the render buffer allocation or the
 *         display creation fails (details on the serial console).
 */
bool lvgl_port_init(void);

/**
 * @brief Run one LVGL iteration and the housekeeping timers.
 *
 * Calls lv_timer_handler(), performs the periodic self-healing full-screen
 * refresh (WUADVI_FULL_REFRESH_MS) and returns the delay LVGL suggests
 * before the next call.
 *
 * @return Milliseconds the caller may sleep before calling again (capped
 *         to 100 ms).
 */
uint32_t lvgl_port_loop(void);

/** @return Rect packets sent to the RP since boot. */
uint32_t lvgl_port_rects_sent(void);

/** @return Flush rects rejected by display_link since boot (should be 0). */
uint32_t lvgl_port_rects_failed(void);
