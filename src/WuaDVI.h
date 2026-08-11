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
     * @return true when the display is running; false with lastError() set.
     */
    bool begin(wua_resolution_id_t res = WUA_RES_640x480x1);

    /**
     * @brief Change the display mode and restart into it.
     *
     * Stores the mode so it survives the restart, then reboots the ESP32.
     * **This call does not return.**
     *
     * A restart is how the mode changes rather than an implementation
     * shortcut: the display engine itself reboots into a new mode (its clocks
     * and framebuffer are fixed once its scanout starts), and this side has to
     * resize the render buffer and rebuild the interface anyway. Restarting
     * both together is the one sequence that is always consistent.
     *
     * The stored mode is picked up by the next begin() that is called without
     * an explicit argument, so a sketch that does `dvi.begin()` follows it.
     *
     * @param res  Mode to switch to.
     * @return Only on failure — false if @p res is not a valid mode.
     */
    bool setResolution(wua_resolution_id_t res);

    /**
     * @brief The mode stored by setResolution(), if any.
     * @param out_res  Receives the stored mode.
     * @return false when nothing has been stored (a fresh device).
     */
    static bool storedResolution(wua_resolution_id_t *out_res);

    /** @brief Forget the stored mode, so begin() uses its argument again. */
    static void clearStoredResolution(void);

    /**
     * @brief Handle one keypress from the serial console.
     *
     * Gives any sketch a way to change the display mode without a rebuild —
     * drop this in your loop() and the board answers on the serial monitor:
     *
     * @code
     *   while (Serial.available() > 0)
     *       dvi.consoleKey((char)Serial.read());
     * @endcode
     *
     * Keys: `1`..`5` switch mode (stores and restarts), `c` forgets the stored
     * mode, `i` prints status, `?` prints this list. Anything else is ignored,
     * so a sketch can read its own keys from the same stream.
     *
     * @param key  Character received.
     * @return true if the key was one of ours.
     */
    bool consoleKey(char key);

    /** @brief Print the console key list to Serial. */
    static void printConsoleHelp(void);

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
