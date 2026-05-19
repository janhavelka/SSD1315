/**
 * @file main.cpp
 * @brief Minimal ESP-IDF SSD1315 bring-up example.
 */

#include <stdint.h>

#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "ssd1315/SSD1315.h"
#include "ssd1315_idf_i2c.h"

namespace {

static constexpr char TAG[] = "ssd1315_idf";
static constexpr gpio_num_t I2C_SDA = GPIO_NUM_8;
static constexpr gpio_num_t I2C_SCL = GPIO_NUM_9;
static constexpr uint32_t I2C_FREQ_HZ = 400000U;
static constexpr uint8_t OLED_ADDR = 0x3C;
static constexpr uint8_t OLED_WIDTH = 128;
static constexpr uint8_t OLED_HEIGHT = 64;
static constexpr uint8_t OLED_PAGES = OLED_HEIGHT / 8U;
static constexpr uint32_t LOOP_DELAY_MS = 10U;

SSD1315::SSD1315 display;
Ssd1315IdfI2c i2c;
uint8_t framebuffer[OLED_WIDTH * OLED_PAGES] = {};

esp_err_t initI2c() {
  i2c_master_bus_config_t busConfig = {};
  busConfig.i2c_port = I2C_NUM_0;
  busConfig.sda_io_num = I2C_SDA;
  busConfig.scl_io_num = I2C_SCL;
  busConfig.clk_source = I2C_CLK_SRC_DEFAULT;
  busConfig.glitch_ignore_cnt = 7;
  busConfig.flags.enable_internal_pullup = true;

  esp_err_t err = i2c_new_master_bus(&busConfig, &i2c.bus);
  if (err != ESP_OK) {
    return err;
  }

  i2c_device_config_t devConfig = {};
  devConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  devConfig.device_address = OLED_ADDR;
  devConfig.scl_speed_hz = I2C_FREQ_HZ;

  err = i2c_master_bus_add_device(i2c.bus, &devConfig, &i2c.dev);
  if (err != ESP_OK) {
    (void)i2c_del_master_bus(i2c.bus);
    i2c.bus = nullptr;
    return err;
  }

  i2c.address = OLED_ADDR;
  return ESP_OK;
}

void deinitI2c() {
  if (i2c.dev != nullptr) {
    (void)i2c_master_bus_rm_device(i2c.dev);
    i2c.dev = nullptr;
  }
  if (i2c.bus != nullptr) {
    (void)i2c_del_master_bus(i2c.bus);
    i2c.bus = nullptr;
  }
}

}  // namespace

extern "C" void app_main(void) {
  esp_err_t err = initI2c();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "I2C init failed: %ld", static_cast<long>(err));
    return;
  }

  SSD1315::Config cfg;
  cfg.width = OLED_WIDTH;
  cfg.height = OLED_HEIGHT;
  cfg.i2cAddress = OLED_ADDR;
  cfg.i2cWrite = ssd1315IdfWrite;
  cfg.i2cWriteRead = ssd1315IdfWriteRead;
  cfg.i2cUser = &i2c;
  cfg.nowMs = ssd1315IdfNowMs;
  cfg.cooperativeYield = ssd1315IdfYield;
  cfg.externalBuffer = framebuffer;
  cfg.pageBufferPages = OLED_PAGES;
  cfg.byteBudgetPerTick = 64;

  SSD1315::Status st = display.begin(cfg);
  if (!st.ok()) {
    ESP_LOGE(TAG, "Display begin failed: %s (%d, %ld)", st.msg,
             static_cast<int>(st.code), static_cast<long>(st.detail));
    deinitI2c();
    return;
  }

  display.clear();
  display.drawText(0, 0, "SSD1315 IDF");
  display.drawText(0, 12, "Framework-neutral");
  display.requestFlush();

  while (true) {
    display.tick(ssd1315IdfNowMs(nullptr));
    vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS));
  }
}
