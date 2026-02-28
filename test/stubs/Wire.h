/// @file Wire.h
/// @brief Minimal Wire stub for native testing
#pragma once

#include <cstdint>

class TwoWire {
public:
  void begin(int sda = -1, int scl = -1) {
    (void)sda;
    (void)scl;
  }
  void setClock(uint32_t freq) { (void)freq; }
  void setTimeOut(uint32_t timeoutMs) { (void)timeoutMs; }
};

extern TwoWire Wire;
