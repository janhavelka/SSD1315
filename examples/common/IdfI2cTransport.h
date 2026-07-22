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
  uint16_t initTimeoutMs = 50;
  esp_err_t lastError = ESP_OK;
};

namespace transport {

Ssd1315IdfI2c& idfContext();

bool initWire(int sda, int scl, uint32_t freq = 400000U,
              uint16_t timeoutMs = 50U, uint8_t address = 0x3CU);
void deinitWire();
esp_err_t lastInitError();
void* configUser();

SSD1315::TransportResult wireWrite(uint8_t addr, const uint8_t* data, size_t len,
                                   uint32_t timeoutMs, void* user);

uint32_t nowMs(void* user);
void cooperativeYield(void* user);

}  // namespace transport
