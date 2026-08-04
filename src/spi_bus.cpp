/**
 * @file spi_bus.cpp
 * @brief Shared SPI bus to the RP2354B — see spi_bus.h.
 */
#include "spi_bus.h"
#include <Arduino.h>
#include "wuadvi_config.h"

/*
 * FSPI (SPI2) — the only general-purpose SPI peripheral on the ESP32-C3
 * (SPI0/SPI1 are tied to the flash).
 *
 * IMPORTANT: construct from the bus id (FSPI), NOT from the global `SPI`
 * object.  `SPIClass s(SPI)` invokes the copy-constructor: it copies the
 * global `SPI`, including its FreeRTOS mutex handle (paramLock).  The two
 * objects live in different translation units, so their static-init order is
 * undefined — if this one is constructed first it copies SPI's still-NULL
 * paramLock, and the first beginTransaction() then asserts inside
 * xSemaphoreTake ("pxQueue" == NULL) → boot loop.  Passing FSPI runs the
 * real constructor, which creates a valid mutex.
 */
static SPIClass s_spi(FSPI);

void spi_bus_init(void) {
    /* Manual CS so multi-part payloads (header + pixels) land in a single
     * CS-low envelope; hardware CS would toggle between writeBytes() calls. */
    pinMode(WUADVI_PIN_SPI_CS, OUTPUT);
    digitalWrite(WUADVI_PIN_SPI_CS, HIGH);

    s_spi.begin(WUADVI_PIN_SPI_SCK,
                WUADVI_PIN_SPI_MISO,
                WUADVI_PIN_SPI_MOSI,
                -1 /* CS handled manually */);
}

SPIClass &spi_bus(void) { return s_spi; }
void spi_bus_select(void) { digitalWrite(WUADVI_PIN_SPI_CS, LOW); }
void spi_bus_deselect(void) { digitalWrite(WUADVI_PIN_SPI_CS, HIGH); }
