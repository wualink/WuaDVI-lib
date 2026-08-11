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
#include "wua_resolution.h"
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
     * Runs in whatever mode setResolution() last selected, or 640x480 mono if
     * it was never called. The resolution belongs to setResolution(); begin()
     * takes no say in it.
     *
     * @return true when the display is running; false with lastError() set.
     */
    bool begin(void);

    /**
     * @brief Select the display mode. The one function that decides it.
     *
     * Call it in setup(), before begin(), and it simply sets the mode the
     * board is about to run in — nothing is displaying yet, so nothing has to
     * be torn down:
     *
     * @code
     *   void setup() {
     *       dvi.setResolution(WUA_RES_800x600x1);
     *       dvi.begin();
     *   }
     * @endcode
     *
     * Call it **after** begin() and it is a change rather than a choice, so it
     * restarts the board into the new mode and **does not return**. setup()
     * runs again, and the mode you asked for wins over the one it selects.
     *
     * The restart is not an implementation shortcut: the display engine
     * reboots to change mode (its clocks and framebuffer are fixed once
     * scanout starts), this side has to resize the render buffer, and the
     * widget primitives resolve their pixel sizes when they are created — so
     * the interface has to be rebuilt at the new size regardless. Restarting
     * both ends together is the one sequence that is always consistent, and it
     * puts the rebuild in setup(), where it already lives.
     *
     * A mode changed this way survives that one restart and no more: a power
     * cycle comes up in whatever setup() selects. Changing mode without a
     * restart is planned and will be a separate call — it needs the
     * application to rebuild its own widgets, which this one avoids.
     *
     * @param res  Mode to run in.
     * @return true when selected before begin(); false if @p res is not a
     *         valid mode. Does not return when called after begin().
     */
    bool setResolution(wua_resolution_id_t res);

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
    uint16_t width(void) const { return wua_screen_w(); }

    /** @return Screen height in pixels for the active mode. */
    uint16_t height(void) const { return wua_screen_h(); }

    /** @return Human-readable name of the active mode, e.g. "640x480 mono". */
    const char *resolutionName(void) const { return wua_resolution()->name; }

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
