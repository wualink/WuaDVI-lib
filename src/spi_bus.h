/**
 * @file spi_bus.h
 * @brief Shared SPI bus to the RP2354B (FSPI peripheral, manual CS).
 *
 * Two protocols run on the same wires at different speeds, always
 * sequentially and from the same task:
 *
 *   - Control link (rp_link):     1 MHz, 32-byte commands / 8-byte PONG.
 *   - Display stream (display_link): 25 MHz, fixed-size rect packets.
 *
 * This module owns the single SPIClass instance and the chip-select pin so
 * the two protocol modules can never fight over the peripheral.  Each caller
 * wraps its transfers in its own SPISettings via beginTransaction().
 */
#pragma once
#include <SPI.h>

/**
 * @brief Initialize the FSPI bus on the WuaDVI pins with CS de-asserted.
 *
 * Call once at boot, before any rp_link/display_link traffic.
 */
void spi_bus_init(void);

/**
 * @brief Access the shared SPI instance.
 * @return Reference to the FSPI SPIClass owned by this module.
 */
SPIClass &spi_bus(void);

/** @brief Assert the chip select (drive CS low). */
void spi_bus_select(void);

/** @brief De-assert the chip select (drive CS high). */
void spi_bus_deselect(void);
