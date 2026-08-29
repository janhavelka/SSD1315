/**
 * @file IdfI2cTransport.cpp
 * @brief ESP-IDF I2C adapter for SSD1315 examples.
 */

#if defined(SSD1315_EXAMPLE_PLATFORM_IDF) || (defined(ESP_PLATFORM) && !defined(ARDUINO))

#include "examples/common/IdfI2cTransport.h"

#include <climits>

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace {

Ssd1315IdfI2c gI2c;

int timeoutArg(uint32_t timeoutMs) {
  if (timeoutMs > static_cast<uint32_t>(INT_MAX)) {
    return INT_MAX;
  }
  return static_cast<int>(timeoutMs);
}

SSD1315::TransportResult mapEspError(esp_err_t err) {
  if (err == ESP_OK) {
    return SSD1315::TransportResult::Ok();
  }
  if (err == ESP_ERR_TIMEOUT) {
    return SSD1315::TransportResult::Timeout(err);
  }
  // ESP-IDF's master calls report ACK failure (ESP_ERR_INVALID_RESPONSE /
  // ESP_ERR_NOT_FOUND) without exposing whether the address or a later data
  // byte NACKed. Every remaining failure is therefore reported as an ambiguous
  // bus error with the raw esp_err_t preserved in detail.
  return SSD1315::TransportResult::BusError(err);
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

namespace transport {

Ssd1315IdfI2c& idfContext() {
  return gI2c;
}

bool initWire(int sda, int scl, uint32_t freq, uint16_t timeoutMs, uint8_t address) {
  deinitWire();
  if (timeoutMs == 0U) {
    gI2c.lastError = ESP_ERR_INVALID_ARG;
    return false;
  }

  gI2c.mutex = xSemaphoreCreateMutex();
  if (gI2c.mutex == nullptr) {
    gI2c.lastError = ESP_ERR_NO_MEM;
    return false;
  }

  i2c_master_bus_config_t busConfig = {};
  busConfig.i2c_port = I2C_NUM_0;
  busConfig.sda_io_num = static_cast<gpio_num_t>(sda);
  busConfig.scl_io_num = static_cast<gpio_num_t>(scl);
  busConfig.clk_source = I2C_CLK_SRC_DEFAULT;
  busConfig.glitch_ignore_cnt = 7;
  busConfig.flags.enable_internal_pullup = true;

  esp_err_t err = i2c_new_master_bus(&busConfig, &gI2c.bus);
  if (err != ESP_OK) {
    gI2c.lastError = err;
    vSemaphoreDelete(gI2c.mutex);
    gI2c.mutex = nullptr;
    return false;
  }

  i2c_device_config_t devConfig = {};
  devConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  devConfig.device_address = address;
  devConfig.scl_speed_hz = freq;

  err = i2c_master_bus_add_device(gI2c.bus, &devConfig, &gI2c.dev);
  if (err != ESP_OK) {
    (void)i2c_del_master_bus(gI2c.bus);
    gI2c.bus = nullptr;
    vSemaphoreDelete(gI2c.mutex);
    gI2c.mutex = nullptr;
    gI2c.lastError = err;
    return false;
  }

  gI2c.address = address;
  gI2c.lastError = ESP_OK;
  return true;
}

void deinitWire() {
  if (gI2c.dev != nullptr) {
    (void)i2c_master_bus_rm_device(gI2c.dev);
    gI2c.dev = nullptr;
  }
  if (gI2c.bus != nullptr) {
    (void)i2c_del_master_bus(gI2c.bus);
    gI2c.bus = nullptr;
  }
  if (gI2c.mutex != nullptr) {
    vSemaphoreDelete(gI2c.mutex);
    gI2c.mutex = nullptr;
  }
}

esp_err_t lastInitError() {
  return gI2c.lastError;
}

void* configUser() {
  return &gI2c;
}

SSD1315::TransportResult wireWrite(uint8_t addr, const uint8_t* data, size_t len,
                                   uint32_t timeoutMs, void* user) {
  if (data == nullptr || len == 0U) {
    return SSD1315::TransportResult::BusError(ESP_ERR_INVALID_ARG);
  }
  Ssd1315IdfI2c* ctx = checkedContext(addr, user);
  if (ctx == nullptr) {
    return SSD1315::TransportResult::BusError(ESP_ERR_INVALID_STATE);
  }
  if (ctx->mutex == nullptr) {
    return SSD1315::TransportResult::BusError(ESP_ERR_INVALID_STATE);
  }
  const int64_t startedUs = esp_timer_get_time();
  if (xSemaphoreTake(ctx->mutex, pdMS_TO_TICKS(timeoutMs)) != pdTRUE) {
    return SSD1315::TransportResult::Timeout(ESP_ERR_TIMEOUT);
  }
  // Truncate, do not round up: an uncontended take costs a few microseconds,
  // and rounding that to a whole millisecond made every 1 ms budget - exactly
  // what the driver passes on the last attempt before an operation deadline -
  // fail here without ever reaching the bus. xSemaphoreTake was already bounded
  // by timeoutMs, so at least one transmit attempt is always allowed.
  const uint64_t elapsedUs = static_cast<uint64_t>(esp_timer_get_time() - startedUs);
  const uint32_t elapsedMs = static_cast<uint32_t>(elapsedUs / 1000ULL);
  const uint32_t remainingMs = (timeoutMs > elapsedMs) ? (timeoutMs - elapsedMs) : 1U;
  const esp_err_t err =
      i2c_master_transmit(ctx->dev, data, len, timeoutArg(remainingMs));
  xSemaphoreGive(ctx->mutex);
  return mapEspError(err);
}

uint32_t nowMs(void* user) {
  (void)user;
  return static_cast<uint32_t>(esp_timer_get_time() / 1000LL);
}

void cooperativeYield(void* user) {
  (void)user;
  vTaskDelay(pdMS_TO_TICKS(1U));
}

}  // namespace transport

#endif  // IDF example build
