/**
 * @file rp_manager.cpp
 * @brief RP2354B bring-up state machine — see rp_manager.h.
 */
#include <Arduino.h>
#include "rp_manager.h"
#include "rp_boot.h"
#include "link_protocol.h"
#include "wuadvi_config.h"
#include "wuadvi_rp_payload.h"

/**
 * @brief Probe repeatedly until the RP answers or the window expires.
 * @param window_ms  Total time to keep probing.
 * @param out        Filled with the decoded PONG on success.
 * @return true on the first valid PONG.
 */
static bool probe_until(uint32_t window_ms, rp_link_info_t *out) {
    uint32_t t0 = millis();
    while (millis() - t0 < window_ms) {
        if (rp_link_probe(out))
            return true;
        delay(WUADVI_PROBE_PERIOD_MS);
    }
    return false;
}

/**
 * @brief Check whether a PONG matches the embedded firmware payload.
 *
 * Only the semantic version matters: the RP image is universal (every
 * resolution compiled in, the active one chosen at runtime), so a running RP
 * never needs re-flashing just because this build targets a different
 * resolution — we simply tell it which mode to run.
 *
 * @param info  Decoded PONG.
 * @return true when nothing needs to be flashed.
 */
static bool matches_payload(const rp_link_info_t *info) {
    return info->major == RP_PAYLOAD_VERSION_MAJOR &&
           info->minor == RP_PAYLOAD_VERSION_MINOR &&
           info->patch == RP_PAYLOAD_VERSION_PATCH;
}

/**
 * @brief Poll the link until the RP reports READY.
 *
 * The RP answers pings during its whole boot splash; READY means the splash
 * minimum time was served and DISPLAY_START will be honored.
 *
 * @param window_ms  Total time to wait (covers the splash duration).
 * @param out        Updated with the last PONG.
 * @return true once mode == LINK_MODE_READY.
 */
static bool wait_ready(uint32_t window_ms, rp_link_info_t *out) {
    uint32_t t0 = millis();
    while (millis() - t0 < window_ms) {
        if (rp_link_probe(out) && out->mode == LINK_MODE_READY)
            return true;
        delay(WUADVI_PROBE_PERIOD_MS);
    }
    return false;
}

bool rp_manager_bring_up(rp_manager_status_t *out) {
    rp_manager_status_t st = {};

    Serial.printf("[RP] embedded payload: v%u.%u.%u %s (%u B, CRC32 0x%08lX)\n",
                  (unsigned)RP_PAYLOAD_VERSION_MAJOR,
                  (unsigned)RP_PAYLOAD_VERSION_MINOR,
                  (unsigned)RP_PAYLOAD_VERSION_PATCH,
                  WUADVI_RES_TEXT,
                  (unsigned)RP_PAYLOAD_FW_SIZE,
                  (unsigned long)RP_PAYLOAD_FW_CRC32);

    Serial.println("[RP] normal reset + SPI probing...");
    rp_boot_reset_normal();

    bool need_flash = true;
    if (probe_until(WUADVI_PROBE_WINDOW_MS, &st.info)) {
        Serial.printf("[RP] alive: firmware v%u.%u.%u res_id=%u mode=%u\n",
                      st.info.major, st.info.minor, st.info.patch,
                      st.info.res_id, st.info.mode);
        if (matches_payload(&st.info)) {
            Serial.println("[RP] version and resolution match - no flash needed");
            need_flash = false;
        } else {
            Serial.println("[RP] version/resolution differ from the embedded payload - updating");
        }
    } else {
        Serial.println("[RP] no answer - assuming blank/broken firmware, flashing");
    }

#if defined(WUADVI_FORCE_RP_FLASH)
    /* Development aid: re-flash even on a version match.  Needed when the
     * embedded RP payload changed content without a version bump (an
     * unreleased version being iterated on). */
    if (!need_flash) {
        Serial.println("[RP] WUADVI_FORCE_RP_FLASH set - flashing anyway");
        need_flash = true;
    }
#endif

    if (need_flash) {
        bool flashed_ok = false;
        for (uint32_t attempt = 1; attempt <= WUADVI_MAX_FLASH_ATTEMPTS; ++attempt) {
            Serial.printf("[RP] --- flash attempt %lu/%u ---\n",
                          (unsigned long)attempt, (unsigned)WUADVI_MAX_FLASH_ATTEMPTS);
            if (!rp_boot_flash()) {
                Serial.println("[RP] UART boot dialog failed, retrying");
                continue;
            }
            ++st.flash_count;
            Serial.println("[RP] waiting for the stub to program and the firmware to boot...");
            if (probe_until(WUADVI_POSTFLASH_WINDOW_MS, &st.info) &&
                matches_payload(&st.info)) {
                Serial.printf("[RP] SUCCESS: running v%u.%u.%u\n",
                              st.info.major, st.info.minor, st.info.patch);
                flashed_ok = true;
                break;
            }
            Serial.println("[RP] RP did not come up with the expected firmware");
            rp_boot_reset_normal();
        }
        if (!flashed_ok) {
            Serial.println("[RP] FAILED: could not bring the RP up to date");
            if (out)
                *out = st;
            return false;
        }
    }

    /* The RP image is universal and boots with its DVI off: tell it which
     * mode this build drives.  It then applies that mode's clocks/voltage,
     * allocates the framebuffer, starts the DVI and shows its splash. */
    Serial.printf("[RP] setting resolution: %s (id %u)\n",
                  WUADVI_RES_TEXT, (unsigned)WUADVI_LINK_RES_ID);
    rp_link_set_resolution(WUADVI_LINK_RES_ID);

    /* Let the boot splash serve its minimum time, then take the screen. */
    if (!wait_ready(WUADVI_READY_WINDOW_MS, &st.info)) {
        Serial.println("[RP] FAILED: RP never reported READY");
        if (out)
            *out = st;
        return false;
    }
    Serial.println("[RP] READY - sending DISPLAY_START");
    rp_link_display_start();

    /* Quiet window: the RP tears down the link and arms the rect DMA.  The
     * extra settle margin protects the very first paint — losing its opening
     * rects is what left stale splash-era pixels on screen during bring-up. */
    delay(LINK_DISPLAY_SWITCH_MS + WUADVI_DISPLAY_SETTLE_MS);

    st.alive = true;
    if (out)
        *out = st;
    Serial.println("[RP] display stream armed - the ESP32 owns the screen now");
    return true;
}
