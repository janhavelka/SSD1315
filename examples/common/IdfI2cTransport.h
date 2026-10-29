/**
 * @file IdfI2cTransport.h
 * @brief ESP-IDF I2C adapter for SSD1315 examples.
 *
 * Example-only glue. The library itself remains transport-callback based and
 * does not own the ESP-IDF I2C bus.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "ssd1315/SSD1315.h"

struct Ssd1315IdfI2c {
  i2c_master_bus_handle_t bus = nullptr;
  i2c_master_dev_handle_t dev = nullptr;
  SemaphoreHandle_t mutex = nullptr;
  uint8_t address = 0x3C;
  esp_err_t lastError = ESP_OK;
};

namespace transport {

Ssd1315IdfI2c& idfContext();

/**
 * @brief Replace the example-owned I2C bus/device and mutex resources.
 *
 * Calls deinitWire() first, then allocates a mutex and native IDF bus/device.
 * This synchronous setup does not configure transfer deadlines; the driver
 * supplies each wireWrite() budget from SSD1315::Config::i2cTimeoutMs,
 * potentially reduced by an operation deadline.
 *
 * @param sda Board-valid SDA GPIO number.
 * @param scl Board-valid SCL GPIO number.
 * @param freq Positive I2C clock frequency in Hz, supported by the board/panel.
 * @param address Unshifted 7-bit display address (0x3C or 0x3D for SSD1315).
 * @return true on success; false with the native error in lastInitError().
 * @note Task context only. The application must serialize setup/teardown
 *       against all users of these resources; the transfer mutex does not
 *       protect resource lifetime. The core driver does not own the bus.
 */
bool initIdfI2c(int sda, int scl, uint32_t freq, uint8_t address);

/** @brief Best-effort release of example resources; requires exclusive task access. */
void deinitWire();
/** @brief Return the raw ESP-IDF error from the last initialization attempt. */
esp_err_t lastInitError();
/** @brief Return the example context for SSD1315::Config::i2cUser. */
void* configUser();

/**
 * @brief Perform at most one native transmit attempt with no retry or recovery.
 *
 * @param addr Unshifted 7-bit address matching the initialized device.
 * @param data Non-null buffer, valid until this synchronous callback returns.
 * @param len Positive byte count, including the SSD1315 control byte.
 * @param timeoutMs Total mutex-wait plus transmit budget in milliseconds,
 *        supplied by the driver from SSD1315::Config::i2cTimeoutMs (possibly
 *        clipped to a remaining operation deadline). Contention time is rounded
 *        up to whole milliseconds and subtracted before the backend attempt.
 * @param user Initialized Ssd1315IdfI2c context from configUser().
 * @return Terminal result with raw esp_err_t detail on failure. ESP_ERR_TIMEOUT
 *         maps to timeout; other backend failures map to ambiguous bus error,
 *         without claiming an address/data NACK distinction.
 * @note Task context only; may block within the supplied budget. Every acquired
 *       mutex is released before return. The application owns resource lifetime
 *       and driver serialization; this callback must not race setup/teardown.
 */
SSD1315::TransportResult wireWrite(uint8_t addr, const uint8_t* data, size_t len,
                                   uint32_t timeoutMs, void* user);

uint32_t nowMs(void* user);
void cooperativeYield(void* user);

}  // namespace transport
