/**
 * @file wuadvi_config.h
 * @brief Board-level configuration of the WuaDVI ESP32-C3 firmware:
 *        identity strings, pinout and display resolution selection.
 *
 * The pinout is verified against the WuaDVI KiCad netlist — do not change
 * without a board revision.
 */
#pragma once
#include <stdint.h>

/* ── Product / organization identity ─────────────────────────────────────── */
#define WUADVI_ORG_NAME     "Wualink"
#define WUADVI_ORG_PARENT   "Wualabs"
#define WUADVI_ORG_URL      "wualabs.com"
#define WUADVI_PRODUCT_NAME "WuaDVI v1.0"

/** ESP32 application firmware version (this project). */
#define WUADVI_ESP_FW_VERSION "1.0.0"

/* ── ESP32-C3 ↔ RP2354B pinout (WuaDVI PCB) ─────────────────────────────────
 *
 *   ESP32-C3            net             RP2354B
 *   ---------           -------------   -----------------------------
 *   GPIO10          →   RST-RP          RUN            (+ button + pull-up)
 *   GPIO1           →   GPIO1           QSPI_SS/BOOTSEL (+ button)
 *   GPIO3           →   QSPI_SD1        QSPI_SD1
 *   GPIO21 (TX0)    →   QSPI_SD3_RX     QSPI_SD3 = ROM UART boot RX
 *   GPIO20 (RX0)    ←   QSPI_SD2_TX     QSPI_SD2 = ROM UART boot TX
 *   GPIO4  (SCK)    →   SPI1_CLK        GPIO10
 *   GPIO5  (MISO)   ←   SPI1_MISO       GPIO11
 *   GPIO6  (MOSI)   →   SPI1_MOSI       GPIO12
 *   GPIO7  (CS)     →   SPI1_CS         GPIO13
 *
 * GOLDEN RULE: outside the UART-boot sequence, every strap/UART pin below
 * must idle in high impedance (INPUT, no pull).  QSPI_SD1/2/3 and CSn are
 * the live bus of the RP2354B in-package flash during XIP — forcing any
 * level there corrupts its flash reads.  This is also why logs go over
 * native USB-CDC and never over UART0.                                      */
#define WUADVI_PIN_RP_RUN     10 /**< RP RUN (reset, open-drain style).  */
#define WUADVI_PIN_RP_BOOTSEL 1  /**< RP QSPI_CSn — BOOTSEL strap.       */
#define WUADVI_PIN_RP_SD1     3  /**< RP QSPI_SD1 — UART-boot strap.     */
#define WUADVI_PIN_UART_TX    21 /**< UART0 TX → QSPI_SD3 (ROM RX).      */
#define WUADVI_PIN_UART_RX    20 /**< UART0 RX ← QSPI_SD2 (ROM TX).      */

#define WUADVI_PIN_SPI_SCK  4
#define WUADVI_PIN_SPI_MISO 5
#define WUADVI_PIN_SPI_MOSI 6
#define WUADVI_PIN_SPI_CS   7

/* The display mode is a RUNTIME choice — see wua_resolution.h.
 * It used to be selected here by a -DWUADVI_RES_* build flag. */

/* ── Timing / behavior tuning ────────────────────────────────────────────── */
/** Window to wait for the RP to answer a probe after a normal reset (its
 *  splash starts answering pings within ~1 s of power-up). */
#define WUADVI_PROBE_WINDOW_MS 8000u
/** Window after a UART-boot flash: stub erase+program+verify plus reboot. */
#define WUADVI_POSTFLASH_WINDOW_MS 20000u
/** Delay between consecutive probe attempts. */
#define WUADVI_PROBE_PERIOD_MS 250u
/** Maximum consecutive UART-boot flash attempts before giving up. */
#define WUADVI_MAX_FLASH_ATTEMPTS 3u
/** Window to wait for the RP to report READY once alive.  Must comfortably
 *  exceed the RP boot-splash minimum time (4 s in WuaDVI-rp-lite). */
#define WUADVI_READY_WINDOW_MS 10000u
/** Period of the self-healing full-screen refresh (0 disables it).  With the
 *  inter-rect gap enforced by display_link this is pure insurance — it only
 *  matters if a rect is lost to noise/glitches, so it can be slow. */
#define WUADVI_FULL_REFRESH_MS 30000u
/** Extra quiet time after DISPLAY_START on top of LINK_DISPLAY_SWITCH_MS
 *  before the first rect is sent.  The protocol minimum is enough for the RP
 *  to re-arm its DMA; the margin also covers its splash→display redraw. */
#define WUADVI_DISPLAY_SETTLE_MS 100u
