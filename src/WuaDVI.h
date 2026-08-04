/**
 * @file WuaDVI.h
 * @brief Public entry point of WuaDVI-lib — the WuaDVI board.
 *
 * Include this and you have the board: bring it up, draw with LVGL, and pixels
 * come out of the HDMI connector.
 *
 * @code
 *   #include <WuaDVI.h>
 *   WuaDVI dvi;
 *
 *   void setup() {
 *       if (!dvi.begin()) { Serial.println(dvi.lastError()); return; }
 *       lv_obj_t *tile = wua_tile(lv_screen_active(), "Hello", 90, 60,
 *                                 lv_color_hex(0x0E5A50));
 *       wua_label(tile, "WuaDVI", 12, wua_theme()->text);
 *   }
 *   void loop() { dvi.loop(); }
 * @endcode
 *
 * The RP2354B display engine is managed for you — probed, re-flashed when its
 * firmware differs from the one this library ships, and recovered if it resets.
 * A sketch never has to know it exists.
 *
 * The resolution is a build-time choice in this version (`-DWUADVI_RES_*`, see
 * wuadvi_config.h); runtime selection lands in 0.3.0 and will arrive as an
 * optional argument to begin(), so this signature stays valid.
 */
#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include <stdint.h>

#include "wuadvi_config.h"
#include "wua_ui.h"

/**
 * @brief The WuaDVI board.
 *
 * Create one instance, call begin() once and loop() from your loop().
 * Everything else is optional.
 */
class WuaDVI {
  public:
    /**
     * @brief Bring the board up: display engine, LVGL and the pixel stream.
     *
     * Releases the RP strap pins, starts the SPI bus, brings up the RP2354B
     * (flashing it if its firmware differs), then starts LVGL. On return the
     * active LVGL screen is ready to be populated.
     *
     * Safe to call again after a failure — it retries the whole sequence.
     *
     * @return true when the display is running; false with lastError() set.
     */
    bool begin(void);

    /**
     * @brief Service the board. Call from your loop(), as often as possible.
     *
     * Pumps LVGL, streams whatever it redrew, and watches the display engine's
     * health: if the RP stops reporting telemetry it has reset, and the
     * pipeline is rebuilt automatically. Your LVGL objects survive that — the
     * screen is simply repainted.
     *
     * Blocks only for the idle time LVGL asks for, so it paces itself.
     */
    void loop(void);

    /** @return true once begin() has succeeded and the stream is running. */
    bool ready(void) const { return m_running; }

    /**
     * @brief Last failure from begin(), for logging.
     * @return Human-readable reason, or "" if nothing has failed.
     */
    const char *lastError(void) const { return m_error; }

    /**
     * @brief On-die temperature of the RP2354B display engine.
     *
     * Reported by the RP on the back-channel of every pixel packet.
     *
     * @param out_c10  Receives the temperature in tenths of a degree Celsius.
     * @return true when a reading is available and recent.
     */
    bool temperature(int16_t *out_c10) const;

    /** @return Screen width in pixels for the active mode. */
    uint16_t width(void) const { return SCREEN_W; }

    /** @return Screen height in pixels for the active mode. */
    uint16_t height(void) const { return SCREEN_H; }

    /** @return Human-readable name of the active mode, e.g. "640x480 mono". */
    const char *resolutionName(void) const { return WUADVI_RES_TEXT; }

    /** @return Pixel packets successfully sent since boot. */
    uint32_t rectsSent(void) const;

    /** @return Pixel packets that failed to send since boot. */
    uint32_t rectsFailed(void) const;

    /** @return Version of the RP2354B firmware this library ships. */
    static const char *displayEngineVersion(void);

  private:
    bool startPipeline(void);

    bool m_running = false;
    bool m_lvgl_started = false;
    const char *m_error = "";
};
