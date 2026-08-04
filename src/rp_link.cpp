/**
 * @file rp_link.cpp
 * @brief Control-link master — see rp_link.h.
 */
#include <Arduino.h>
#include <string.h>
#include "rp_link.h"
#include "spi_bus.h"
#include "link_protocol.h"

/* 1 MHz, mode 1 (CPOL=0, CPHA=1) — matches the RP slave (RP2040-E2 erratum
 * requires CPHA=1 for PL022 slave reception). */
static const SPISettings kLinkSettings(1000000u, MSBFIRST, SPI_MODE1);

/**
 * @brief Full-duplex transfer of a buffer inside one CS-low envelope.
 * @param buf  Bytes to send; overwritten in place with the received bytes.
 * @param n    Buffer length.
 */
static void xfer(uint8_t *buf, uint32_t n) {
    spi_bus().beginTransaction(kLinkSettings);
    spi_bus_select();
    spi_bus().transferBytes(buf, buf, n);
    spi_bus_deselect();
    spi_bus().endTransaction();
}

/**
 * @brief Build and send one 32-byte command packet.
 * @param type  LINK_TYPE_* command.
 * @param arg   Type-specific argument (0 when unused).
 */
static void send_packet(uint8_t type, uint8_t arg) {
    uint8_t pkt[LINK_PKT_SIZE];
    memset(pkt, 0, sizeof(pkt));
    pkt[0] = LINK_MAGIC0;
    pkt[1] = LINK_MAGIC1;
    pkt[2] = type;
    pkt[3] = arg;
    pkt[LINK_PKT_SIZE - 1] = link_checksum(pkt, LINK_PKT_SIZE - 1);
    xfer(pkt, sizeof(pkt));
}

bool rp_link_probe(rp_link_info_t *out) {
    send_packet(LINK_TYPE_PING, 0);

    /* The slave needs this long to notice the PING and preload its TX FIFO. */
    delay(LINK_PONG_DELAY_MS);

    uint8_t resp[LINK_RESP_SIZE];
    memset(resp, 0, sizeof(resp));
    xfer(resp, sizeof(resp));

    if (resp[0] != LINK_RMAGIC0 || resp[1] != LINK_RMAGIC1)
        return false;
    if (link_checksum(resp, 7) != resp[7])
        return false;

    if (out) {
        out->major = resp[2];
        out->minor = resp[3];
        out->patch = resp[4];
        out->mode = resp[5];
        out->res_id = resp[6];
    }
    return true;
}

void rp_link_set_resolution(uint8_t res_id) {
    send_packet(LINK_TYPE_SET_RESOLUTION, res_id);
}

void rp_link_display_start(void) {
    send_packet(LINK_TYPE_DISPLAY_START, 0);
}
