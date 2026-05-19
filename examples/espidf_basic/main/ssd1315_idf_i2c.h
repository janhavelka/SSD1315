/**
 * @file ssd1315_idf_i2c.h
 * @brief ESP-IDF I2C adapter for the SSD1315 example.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <driver/i2c_master.h>

#include "ssd1315/SSD1315.h"

struct Ssd1315IdfI2c {
  i2c_master_bus_handle_t bus = nullptr;
  i2c_master_dev_handle_t dev = nullptr;
  uint8_t address = 0x3C;
};

SSD1315::Status ssd1315IdfWrite(uint8_t addr, const uint8_t* data, size_t len,
                                uint32_t timeoutMs, void* user);

SSD1315::Status ssd1315IdfWriteRead(uint8_t addr, const uint8_t* txData,
                                    size_t txLen, uint8_t* rxData, size_t rxLen,
                                    uint32_t timeoutMs, void* user);

uint32_t ssd1315IdfNowMs(void* user);
void ssd1315IdfYield(void* user);
