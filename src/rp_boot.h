/**
 * @file rp_boot.h
 * @brief RP2354B flashing through the RP2350 ROM UART boot (datasheet §5.8).
 *
 * Implements the ROM protocol, 1 Mbaud 8N1 on QSPI_SD2 (RP TX) / QSPI_SD3
 * (RP RX), reachable from the ESP32-C3 as UART0 (source:
 * pico-bootrom-rp2350/src/nsboot/nsboot_uart_client.S):
 *
 *   - On entry the ROM prints "RP2350" and stays silent until it receives
 *     the knock sequence 0x56 0xff 0x8b 0xe4.
 *   - Single-byte commands, echoed by the ROM on completion:
 *       'w' + 32 bytes   write to SRAM and advance the pointer
 *       'r'              read back 32 bytes + echo
 *       'c'              reset the pointer to SRAM_BASE
 *       'x'              reboot executing the downloaded RAM image
 *
 * rp_boot_flash() assembles in the RP's SRAM:
 *   0x20000000  flash stub  (RP_STUB_BIN, zero-padded to 4 KB)
 *   0x20001000  header      (magic "WDFL", size, CRC32, version)
 *   0x20001100  firmware    (RP_FW_BIN, 0xFF-padded to a 256 multiple)
 * and executes it: the stub programs the internal flash, verifies and
 * performs a normal reboot.  With RP_BOOT_VERIFY=1 the SRAM content is also
 * read back and compared before executing.
 *
 * GOLDEN RULE: outside this sequence every strap/UART pin must idle in high
 * impedance — they are the live QSPI bus of the RP's in-package flash.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Flash the embedded RP firmware payload through ROM UART boot.
 *
 * Blocking (a few seconds; ~2 s more with RP_BOOT_VERIFY=1).  On return the
 * pins are released; the stub is erasing/programming asynchronously and the
 * result must be confirmed by probing the SPI control link (rp_link).
 *
 * @return true if the whole UART dialog completed (splash, knock, SRAM
 *         write/verify, 'x' accepted); false on any timeout or echo error.
 */
bool rp_boot_flash(void);

/**
 * @brief Normal RP reset: pulse RUN low without touching the boot straps.
 */
void rp_boot_reset_normal(void);

/**
 * @brief Release RUN, BOOTSEL, SD1 and both UART pins to high impedance.
 *
 * Call at boot and after any boot-strap manipulation.
 */
void rp_boot_release_pins(void);
