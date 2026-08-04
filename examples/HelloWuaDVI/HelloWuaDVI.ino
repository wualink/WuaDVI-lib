/*
 * HelloWuaDVI — the smallest complete WuaDVI sketch.
 *
 * Brings the board up and puts one tile on the HDMI output. The RP2354B
 * display engine is probed, flashed if needed and kept healthy by the library;
 * this sketch never mentions it.
 *
 * Hardware: WuaDVI board (ESP32-C3 + RP2354B), HDMI monitor.
 * Console : native USB-CDC at 115200 baud.
 *
 * The display mode is chosen at runtime — pass it to begin(), or change it
 * later with dvi.setResolution(), which stores it and restarts into it.
 *
 * Copyright (c) 2026 Wualabs LTD. MIT licensed.
 */
#include <WuaDVI.h>

WuaDVI dvi;

void setup() {
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0); /* never block when no host is attached */
    delay(1500);              /* margin to open the serial monitor */

    Serial.printf("\nWuaDVI %s — display engine v%s\n",
                  dvi.resolutionName(), WuaDVI::displayEngineVersion());

    if (!dvi.begin(WUA_RES_640x480x1)) {
        Serial.printf("[ERROR] %s\n", dvi.lastError());
        return; /* loop() keeps retrying */
    }

    /* Everything below is ordinary LVGL, drawn with primitives that resolve
     * their own sizes for whichever resolution was built. */
    lv_obj_t *tile = wua_tile(lv_screen_active(), "Hello", 90, 60,
                              wua_theme()->tile);
    wua_label(tile, "WuaDVI", 14, wua_theme()->accent);
    wua_label(tile, dvi.resolutionName(), 7, wua_theme()->dim);

    Serial.printf("[OK] %ux%u running\n", dvi.width(), dvi.height());
}

void loop() {
    dvi.loop();

    /* Changing mode is one call. It stores the choice and restarts into it —
     * so it does not return, and the next begin() follows the stored mode.
     *
     *   dvi.setResolution(WUA_RES_1280x720x1);
     */

    static uint32_t last = 0;
    if (millis() - last >= 5000) {
        last = millis();
        int16_t t;
        if (dvi.temperature(&t)) {
            Serial.printf("rects=%lu  engine %d.%d C\n",
                          (unsigned long)dvi.rectsSent(), t / 10, abs(t % 10));
        }
    }
}
