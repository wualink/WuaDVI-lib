/**
 * @file display_link.cpp
 * @brief Framebuffer rect stream master — see display_link.h.
 */
#include <Arduino.h>
#include <string.h>
#include "display_link.h"
#include "spi_bus.h"

static const SPISettings kRectSettings(DISPLAY_LINK_SPI_HZ, MSBFIRST, SPI_MODE1);

/*
 * Static zero buffer used to pad short rects up to RECT_PAYLOAD_MAX so every
 * SPI transaction is exactly RECT_TOTAL_SIZE bytes — the RP2354B slave DMA
 * is configured for a fixed-length transfer and would stall on a short
 * packet.
 */
static const uint8_t s_pad_zero[RECT_PAYLOAD_MAX] = {0};

/* End-of-transmission timestamp of the previous rect (see the gap below). */
static uint32_t s_last_end_us = 0;

/* MISO capture of the header phase — carries the RP telemetry packet. */
static uint8_t s_rx_hdr[RECT_HEADER_SIZE];
static int16_t s_temp_c10 = 0;
static uint32_t s_temp_ms = 0;
static bool s_temp_valid = false;
/* Heartbeat: time of the last valid-magic telemetry packet, regardless of the
 * temperature value.  Present on every rect while the RP is in display mode;
 * its absence means the RP has reset (used by the ESP32 health watchdog). */
static uint32_t s_telem_ms = 0;
static bool s_telem_ever = false;

/**
 * @brief Extract one byte from a buffer viewed as a bitstream.
 * @param buf      Source bytes (MSB-first bit order, as SPI shifts them).
 * @param bit_off  Bit position of the byte to extract.
 * @return The 8 bits starting at bit_off.
 */
static inline uint8_t bits_at(const uint8_t *buf, uint32_t bit_off) {
    const uint32_t byte = bit_off >> 3;
    const uint32_t sh = bit_off & 7u;
    if (sh == 0)
        return buf[byte];
    return (uint8_t)((buf[byte] << sh) | (buf[byte + 1] >> (8u - sh)));
}

/**
 * @brief Hunt for the RP telemetry packet in the captured MISO bytes.
 *
 * The RP loads the packet at the start of its TX transfer, but the PL022
 * slave skew (see the RP firmware) and FIFO priming can offset the stream by
 * a few bits either way.  Scanning every bit offset that still leaves a full
 * packet inside the capture window is ~30 cheap checks: magic first (fails
 * fast), then the XOR checksum before accepting.
 */
static void parse_telemetry(void) {
    const uint32_t total_bits = RECT_HEADER_SIZE * 8u;
    for (uint32_t off = 0; off + TELEM_SIZE * 8u <= total_bits; ++off) {
        if (bits_at(s_rx_hdr, off) != TELEM_MAGIC0)
            continue;
        if (bits_at(s_rx_hdr, off + 8u) != TELEM_MAGIC1)
            continue;

        uint8_t pkt[TELEM_SIZE];
        for (uint32_t i = 0; i < TELEM_SIZE; ++i)
            pkt[i] = bits_at(s_rx_hdr, off + i * 8u);

        const uint8_t sum = (uint8_t)(pkt[0] ^ pkt[1] ^ pkt[2] ^
                                      pkt[3] ^ pkt[4] ^ pkt[5]);
        if (sum != pkt[6])
            continue;

        /* Valid packet → the RP is alive and displaying: refresh the
         * heartbeat regardless of the temperature value. */
        s_telem_ms = millis();
        s_telem_ever = true;

        const int16_t temp = (int16_t)((uint16_t)pkt[2] | ((uint16_t)pkt[3] << 8));
        if (temp == TELEM_TEMP_INVALID)
            return; /* sensor down: keep last temp */

        s_temp_c10 = temp;
        s_temp_ms = millis();
        s_temp_valid = true;
        return;
    }
}

/**
 * @brief Enforce the minimum quiet time between consecutive rect packets.
 *
 * The RP2354B needs ~4 ms after each packet to blit the rect and re-arm its
 * fixed-length RX DMA (its SPI FIFO is only 8 bytes ≈ 2.5 µs at 25 MHz).
 * Back-to-back packets overrun the FIFO, knock the DMA byte counter out of
 * step and get whole rects dropped — hardware-verified during the JPEGDEC
 * bring-up, and visible as stale/fragmented screen areas when unpaced.  The
 * RP's bad-magic resync also relies on this gap being much longer than its
 * quiet-bus threshold (300 µs).
 *
 * Only the remaining time is waited: if rendering the next rect already took
 * longer than the gap (typical in the RGB565 modes), this returns instantly.
 * Waits ≥ 1 ms go through delay() so the RTOS idle task keeps running.
 */
static void wait_inter_packet_gap(void) {
    const uint32_t elapsed = micros() - s_last_end_us; /* wrap-safe */
    if (elapsed >= DISPLAY_LINK_RECT_GAP_US)
        return;

    uint32_t remaining = DISPLAY_LINK_RECT_GAP_US - elapsed;
    if (remaining >= 1000u) {
        delay(remaining / 1000u);
        remaining %= 1000u;
    }
    if (remaining > 0u)
        delayMicroseconds(remaining);
}

#if defined(WUADVI_COLOR_MONO)
/*
 * Scratch buffer holding the rect packed to 1 bit/pixel (MSB first, each row
 * padded to a whole byte).  Sized for the largest possible rect.
 */
static uint8_t s_bits_buf[RECT_PAYLOAD_MAX];
#endif

bool display_link_send_rect(uint16_t x1, uint16_t y1,
                            uint16_t x2, uint16_t y2,
                            const uint8_t *pixels) {
    if (pixels == nullptr)
        return false;
    if (x2 < x1 || y2 < y1)
        return false;
    if (x2 >= SCREEN_W || y2 >= SCREEN_H)
        return false;

    const uint32_t rect_w = (uint32_t)(x2 - x1 + 1);
    const uint32_t rect_h = (uint32_t)(y2 - y1 + 1);

    /* Wire bytes of ONE pixel row of this rect. */
#if defined(WUADVI_COLOR_MONO)
    const uint32_t row_payload = (rect_w + 7u) / 8u;
#else
    const uint32_t row_payload = rect_w * 2u;
#endif

    /*
     * Split the rect into horizontal bands that respect RECT_PAYLOAD_MAX.
     *
     * LVGL's partial renderer limits the AREA of a dirty chunk (its RGB565
     * bytes must fit the render buffer) but not its SHAPE: a tall narrow
     * chunk — e.g. 33 x 581 px at 800x600 — fits the render buffer yet its
     * mono payload (rows padded to whole bytes: 5 B x 581 = 2905 B) exceeds
     * the 24-full-width-lines packet budget (2400 B).  Sending it whole
     * would overflow the packing buffer; each band below is bounded by
     * construction.  row_payload can never exceed RECT_PAYLOAD_MAX on its
     * own (a full-width row is exactly 1/24th of it), so max_rows >= 1.
     */
    const uint32_t max_rows = RECT_PAYLOAD_MAX / row_payload;

    for (uint32_t band_y = 0; band_y < rect_h; band_y += max_rows) {
        const uint32_t remaining = rect_h - band_y;
        const uint32_t band_h = (remaining < max_rows) ? remaining : max_rows;

        /* Band pixels: rows are contiguous inside the rect's row-major
         * RGB565 buffer, so the band is just an offset into it. */
        const uint8_t *band_pixels = pixels + (size_t)band_y * rect_w * 2u;

        /* Pixel payload that actually goes on the wire. */
        const uint8_t *payload;
        const uint32_t pix_size = row_payload * band_h;
        if (pix_size > RECT_PAYLOAD_MAX)
            return false; /* unreachable guard */

#if defined(WUADVI_COLOR_MONO)
        /* Pack the RGB565 LE band to 1 bit/pixel (MSB first), each row
         * padded to a whole byte so the RP addresses rows with a fixed
         * stride. */
        for (uint32_t row = 0; row < band_h; ++row) {
            uint8_t *drow = s_bits_buf + row * row_payload;
            for (uint32_t b = 0; b < row_payload; ++b)
                drow[b] = 0;
            for (uint32_t col = 0; col < rect_w; ++col) {
                const uint32_t i = row * rect_w + col;
                const uint16_t c = (uint16_t)(band_pixels[2u * i] |
                                              ((uint16_t)band_pixels[2u * i + 1u] << 8));
                if (WUADVI_MONO_BIT(c))
                    drow[col >> 3] |= (uint8_t)(0x80u >> (col & 7));
            }
        }
        payload = s_bits_buf;
#else
        payload = band_pixels;
#endif

        const uint16_t band_y1 = (uint16_t)(y1 + band_y);
        const uint16_t band_y2 = (uint16_t)(band_y1 + band_h - 1u);

        /* Build the 12-byte header on the stack; uint16 coords little-endian
         * so the RP (also LE) reads them straight as uint16_t. */
        uint8_t hdr[RECT_HEADER_SIZE];
        hdr[0] = RECT_MAGIC_0;
        hdr[1] = RECT_MAGIC_1;
        hdr[2] = RECT_MAGIC_2;
        hdr[3] = RECT_MAGIC_3;
        hdr[4] = (uint8_t)(x1 & 0xFF);
        hdr[5] = (uint8_t)(x1 >> 8);
        hdr[6] = (uint8_t)(band_y1 & 0xFF);
        hdr[7] = (uint8_t)(band_y1 >> 8);
        hdr[8] = (uint8_t)(x2 & 0xFF);
        hdr[9] = (uint8_t)(x2 >> 8);
        hdr[10] = (uint8_t)(band_y2 & 0xFF);
        hdr[11] = (uint8_t)(band_y2 >> 8);

        wait_inter_packet_gap();

        spi_bus().beginTransaction(kRectSettings);
        spi_bus_select();

        /* Full-duplex on the header phase: while the 12 header bytes go out,
         * the RP clocks its 8-byte telemetry packet back on MISO. */
        spi_bus().transferBytes(hdr, s_rx_hdr, RECT_HEADER_SIZE);
        spi_bus().writeBytes(payload, pix_size);
        const uint32_t pad = RECT_PAYLOAD_MAX - pix_size;
        if (pad > 0)
            spi_bus().writeBytes(s_pad_zero, pad);

        spi_bus_deselect();
        spi_bus().endTransaction();

        s_last_end_us = micros();

        /* The telemetry repeats on every packet — one parse per call is
         * plenty. */
        if (band_y == 0)
            parse_telemetry();
    }
    return true;
}

bool display_link_get_rp_temp(int16_t *temp_c10, uint32_t *age_ms) {
    if (!s_temp_valid)
        return false;
    if (temp_c10 != nullptr)
        *temp_c10 = s_temp_c10;
    if (age_ms != nullptr)
        *age_ms = millis() - s_temp_ms;
    return true;
}

uint32_t display_link_telemetry_age(void) {
    if (!s_telem_ever)
        return 0; /* not started yet — don't trip the watchdog */
    return millis() - s_telem_ms;
}
