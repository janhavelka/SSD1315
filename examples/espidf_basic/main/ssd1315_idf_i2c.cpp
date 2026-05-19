/**
 * @file ssd1315_idf_i2c.cpp
 * @brief ESP-IDF I2C adapter for SSD1315 examples.
 */

#include "ssd1315_idf_i2c.h"

#include <climits>

#include <esp_err.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

int timeoutArg(uint32_t timeoutMs) {
  if (timeoutMs > static_cast<uint32_t>(INT_MAX)) {
    return INT_MAX;
  }
  return static_cast<int>(timeoutMs);
}

SSD1315::Status mapEspError(esp_err_t err, const char* context) {
  if (err == ESP_OK) {
    return SSD1315::Status::Ok();
  }
  if (err == ESP_ERR_TIMEOUT) {
    return SSD1315::Status::Error(SSD1315::Err::I2C_TIMEOUT, context, err);
  }
  if (err == ESP_ERR_INVALID_ARG) {
    return SSD1315::Status::Error(SSD1315::Err::INVALID_CONFIG, context, err);
  }
  if (err == ESP_ERR_INVALID_RESPONSE || err == ESP_ERR_NOT_FOUND) {
    return SSD1315::Status::Error(SSD1315::Err::I2C_BUS_ERROR, context, err);
  }
  return SSD1315::Status::Error(SSD1315::Err::I2C_BUS_ERROR, context, err);
}

Ssd1315IdfI2c* checkedContext(uint8_t addr, void* user) {
  if (user == nullptr) {
    return nullptr;
  }
  Ssd1315IdfI2c* ctx = static_cast<Ssd1315IdfI2c*>(user);
  if (ctx->dev == nullptr || ctx->address != addr) {
    return nullptr;
  }
  return ctx;
}

}  // namespace

SSD1315::Status ssd1315IdfWrite(uint8_t addr, const uint8_t* data, size_t len,
                                uint32_t timeoutMs, void* user) {
  if (data == nullptr || len == 0U) {
    return SSD1315::Status::Error(SSD1315::Err::INVALID_CONFIG,
                                  "Invalid I2C write params");
  }
  Ssd1315IdfI2c* ctx = checkedContext(addr, user);
  if (ctx == nullptr) {
    return SSD1315::Status::Error(SSD1315::Err::INVALID_CONFIG,
                                  "Invalid I2C adapter context");
  }
  return mapEspError(i2c_master_transmit(ctx->dev, data, len, timeoutArg(timeoutMs)),
                     "I2C write failed");
}

SSD1315::Status ssd1315IdfWriteRead(uint8_t addr, const uint8_t* txData,
                                    size_t txLen, uint8_t* rxData, size_t rxLen,
                                    uint32_t timeoutMs, void* user) {
  if (txData == nullptr || txLen == 0U || rxData == nullptr || rxLen == 0U) {
    return SSD1315::Status::Error(SSD1315::Err::INVALID_CONFIG,
                                  "Invalid I2C write-read params");
  }
  Ssd1315IdfI2c* ctx = checkedContext(addr, user);
  if (ctx == nullptr) {
    return SSD1315::Status::Error(SSD1315::Err::INVALID_CONFIG,
                                  "Invalid I2C adapter context");
  }
  return mapEspError(i2c_master_transmit_receive(ctx->dev, txData, txLen, rxData,
                                                 rxLen, timeoutArg(timeoutMs)),
                     "I2C write-read failed");
}

uint32_t ssd1315IdfNowMs(void* user) {
  (void)user;
  return static_cast<uint32_t>(esp_timer_get_time() / 1000LL);
}

void ssd1315IdfYield(void* user) {
  (void)user;
  vTaskDelay(pdMS_TO_TICKS(1U));
}
