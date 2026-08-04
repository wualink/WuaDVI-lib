/*
 * HelloWuaDVI — smoke test for WuaDVI-lib.
 *
 * Prints the RP2354B display-engine firmware version this library ships, which
 * confirms the library is installed and its build hook produced the pinned
 * firmware payload.
 *
 * NOTE: this library is at 0.1.0 and only the packaging layer exists. The board
 * API (dvi.begin(), the LVGL widgets) lands in 0.2.0 — this example grows with
 * it. See the README roadmap.
 *
 * Hardware: WuaDVI board (ESP32-C3 + RP2354B).
 * Console : native USB-CDC at 115200 baud.
 *
 * Copyright (c) 2026 Wualabs LTD. MIT licensed.
 */
#include <WuaDVI.h>

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {
        /* Give the USB-CDC console a moment to enumerate, but never hang
         * waiting for a host that may not be there. */
    }

    Serial.println();
    Serial.println("WuaDVI-lib smoke test");
    Serial.printf("RP2354B firmware shipped by this library: v%s\n",
                  wuadvi_rp_firmware_version());
}

void loop() {
    delay(1000);
}
