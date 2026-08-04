/**
 * @file link_protocol.h
 * @brief WuaDVI control-link protocol v2 — shared between the ESP32-C3
 *        master (this repo) and the RP2354B slave (WuaDVI-rp-lite repo).
 *
 * IMPORTANT: a byte-identical copy of this file lives in each repository.
 * Any change here must be mirrored there — the wire format is the contract.
 *
 * v2 (universal RP firmware): the RP ships a SINGLE firmware image that has
 * every resolution compiled in and boots WITHOUT starting the DVI output.  It
 * waits for the master to tell it which resolution to run (SET_RESOLUTION),
 * and only then configures the clocks, builds the framebuffer and shows the
 * splash.  The master picks the resolution at ITS build time, so changing
 * resolution means reflashing only the ESP32 — the RP image never changes.
 *
 * The control link runs on the same SPI wires as the framebuffer stream, but
 * only BEFORE the display stream starts (boot handshake).  Wiring on the
 * WuaDVI PCB (nets SPI1_*, verified against the KiCad netlist):
 *
 *   ESP32-C3 (master)          RP2354B (slave, SPI1)
 *   ------------------         -----------------------
 *   SCK   GPIO4   ──────────►  GPIO10  SCK
 *   MISO  GPIO5   ◄──────────  GPIO11  TX
 *   MOSI  GPIO6   ──────────►  GPIO12  RX
 *   CS    GPIO7   ──────────►  GPIO13  CS
 *
 * Bus: mode 1 (CPOL=0, CPHA=1), MSB first, 1 MHz.  CPHA=1 is required by the
 * RP2040-E2 erratum (PL022 slave first-bit reception); 1 MHz because this is
 * a low-rate control channel and the slave services it by polling.
 *
 * ── Boot sequence ────────────────────────────────────────────────────────
 *
 *   1. RP boots, starts the control link, DVI still off.
 *      PING → PONG with mode = UNCONFIGURED, res_id = LINK_RES_NONE.
 *   2. Master compares the PONG version against its embedded payload and
 *      re-flashes the RP through ROM UART boot if they differ.
 *   3. Master sends SET_RESOLUTION(res_id).
 *      RP applies that mode's clocks/voltage, allocates the framebuffer,
 *      starts the DVI, draws the splash → mode becomes SPLASH, then READY.
 *   4. Master sends DISPLAY_START; the RP re-arms its SPI as the fixed-size
 *      framebuffer-rect DMA receiver and the pixel stream begins.
 *
 * ── Master → slave: fixed 32-byte command packet, one CS-low envelope ────
 *
 *   Offset  Size  Field
 *   ------  ----  ------------------------------------------------------
 *      0      1   LINK_MAGIC0 (0xA5)
 *      1      1   LINK_MAGIC1 (0x3C)
 *      2      1   type        (LINK_TYPE_*)
 *      3      1   arg         (type-specific, 0 when unused)
 *      4     27   payload     (type-specific, zero-filled when unused)
 *     31      1   checksum    (XOR of bytes 0..30)
 *
 *   Types:
 *     LINK_TYPE_PING            Slave loads its PONG response into the SPI TX
 *                               FIFO.  The master waits >= LINK_PONG_DELAY_MS,
 *                               then clocks out LINK_RESP_SIZE bytes (sending
 *                               zeros) to read it.
 *     LINK_TYPE_SET_RESOLUTION  `arg` carries the LINK_RES_* id to run.  Only
 *                               acted on while UNCONFIGURED; ignored once the
 *                               display is up (the resolution is fixed for the
 *                               session — reboot the RP to change it).
 *     LINK_TYPE_DISPLAY_START   Slave leaves the control link and re-arms its
 *                               SPI as the framebuffer-rect DMA receiver (see
 *                               frame_config.h).  Only honored in READY mode.
 *                               After sending it the master must stay off the
 *                               bus for LINK_DISPLAY_SWITCH_MS before the
 *                               first rect packet.
 *
 * ── Slave → master: 8-byte PONG response ─────────────────────────────────
 *
 *   Offset  Size  Field
 *   ------  ----  ------------------------------------------------------
 *      0      1   LINK_RMAGIC0 (0x3C)
 *      1      1   LINK_RMAGIC1 (0xA5)
 *      2      1   version major
 *      3      1   version minor
 *      4      1   version patch
 *      5      1   mode          (LINK_MODE_*)
 *      6      1   resolution id (LINK_RES_*, LINK_RES_NONE until configured)
 *      7      1   checksum      (XOR of bytes 0..6)
 *
 *   8 bytes fit entirely in the PL022 TX FIFO (8 x 8 bit), so the slave can
 *   preload the response without DMA and the master reads it in one envelope.
 *
 * The version tells the master whether the RP firmware must be re-flashed.
 * Since the image is universal, the resolution is NOT part of that decision.
 */
#pragma once
#include <stdint.h>

/* ── Packet framing ──────────────────────────────────────────────────────── */
#define LINK_PKT_SIZE  32u /**< Master → slave packet size.      */
#define LINK_RESP_SIZE 8u  /**< Slave → master PONG size.        */

#define LINK_MAGIC0  0xA5u
#define LINK_MAGIC1  0x3Cu
#define LINK_RMAGIC0 0x3Cu
#define LINK_RMAGIC1 0xA5u

/* ── Command types ───────────────────────────────────────────────────────── */
#define LINK_TYPE_PING 0x01u
/* 0x02 reserved (TEXT in the proof of concept — not part of the protocol).  */
#define LINK_TYPE_DISPLAY_START  0x03u
#define LINK_TYPE_SET_RESOLUTION 0x04u

/* ── Slave modes (PONG byte 5) ───────────────────────────────────────────── */
#define LINK_MODE_UNCONFIGURED 0x00u /**< Booted, DVI off, awaiting the
                                            SET_RESOLUTION command.         */
#define LINK_MODE_SPLASH       0x01u /**< Boot splash showing, not ready.  */
#define LINK_MODE_READY        0x02u /**< Splash time served; DISPLAY_START
                                            is accepted.                     */
#define LINK_MODE_DISPLAY      0x03u /**< Rect DMA armed (control link off).
                                            Never actually seen in a PONG —
                                            the slave stops answering once it
                                            switches.  Reserved for symmetry. */

/* ── Resolution identifiers (PONG byte 6 / SET_RESOLUTION arg) ───────────── */
#define LINK_RES_NONE      0x00u /**< Not configured yet.              */
#define LINK_RES_320x240   0x01u /**< 320x240  RGB565, 640x480p60 out. */
#define LINK_RES_400x240   0x02u /**< 400x240  RGB565, 800x480p60 out. */
#define LINK_RES_640x480x1 0x03u /**< 640x480  mono native.            */
#define LINK_RES_800x600x1 0x04u /**< 800x600  mono native (RB).       */
/* EXPERIMENTAL — 1280x720 mono at 30 Hz (372 MHz bit clock).  Under
 * evaluation; needs an RP built from feat/1280x720-experiment.              */
#define LINK_RES_1280x720x1 0x05u /**< 1280x720 mono native, 30 Hz.     */

/* ── Timing ──────────────────────────────────────────────────────────────── */
/** Master wait between sending PING and clocking the PONG read. */
#define LINK_PONG_DELAY_MS 25u
/** Master quiet time after DISPLAY_START before the first rect packet
 *  (gives the slave time to tear down the link and arm the rect DMA). */
#define LINK_DISPLAY_SWITCH_MS 50u

/**
 * @brief XOR checksum used by both packet directions.
 * @param data  Bytes to sum.
 * @param len   Number of bytes.
 * @return XOR of all bytes (0 for an empty range).
 */
static inline uint8_t link_checksum(const uint8_t *data, uint32_t len) {
    uint8_t x = 0;
    for (uint32_t i = 0; i < len; ++i)
        x ^= data[i];
    return x;
}
