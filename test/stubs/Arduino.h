/// @file Arduino.h
/// @brief Minimal Arduino stub for native testing
#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <cstdlib>

using byte = uint8_t;

static constexpr uint8_t LOW = 0;
static constexpr uint8_t HIGH = 1;
static constexpr uint8_t INPUT_PULLUP = 0x02;
static constexpr uint8_t OUTPUT = 0x03;
static constexpr uint8_t OUTPUT_OPEN_DRAIN = 0x13;

extern uint32_t gMillis;
extern uint32_t gMicros;
extern uint32_t gMillisStep;
extern uint32_t gMicrosStep;

inline uint32_t millis() {
  const uint32_t value = gMillis;
  gMillis += gMillisStep;
  return value;
}

inline uint32_t micros() {
  const uint32_t value = gMicros;
  gMicros += gMicrosStep;
  return value;
}

inline void delay(uint32_t ms) { gMillis += ms; gMicros += ms * 1000U; }
inline void delayMicroseconds(uint32_t us) { gMicros += us; }
inline void yield() {}
struct ArduinoStubPin {
  uint8_t mode = INPUT_PULLUP;
  uint8_t level = HIGH;
  bool heldLow = false;
  uint32_t lowReadsRemaining = 0;
  uint32_t readCalls = 0;
  uint32_t lowWrites = 0;
};
inline ArduinoStubPin arduinoStubPins[64];
inline uint32_t arduinoStubActiveHighWrites = 0;
inline void resetArduinoStubPins() {
  for (auto& pin : arduinoStubPins) pin = ArduinoStubPin{};
  arduinoStubActiveHighWrites = 0;
}
inline void pinMode(int pin, uint8_t mode) {
  arduinoStubPins[pin].mode = mode;
}
inline void digitalWrite(int pin, uint8_t value) {
  auto& state = arduinoStubPins[pin];
  state.level = value;
  if (value == LOW) ++state.lowWrites;
  if (value == HIGH && state.mode == OUTPUT) ++arduinoStubActiveHighWrites;
}
inline int digitalRead(int pin) {
  auto& state = arduinoStubPins[pin];
  ++state.readCalls;
  if (state.heldLow) return LOW;
  if (state.lowReadsRemaining > 0) {
    --state.lowReadsRemaining;
    return LOW;
  }
  return state.level;
}

class SerialClass {
public:
  void begin(uint32_t baud) { (void)baud; }
  void print(const char* s) { (void)s; }
  void println(const char* s = "") { (void)s; }
  void printf(const char* fmt, ...) { (void)fmt; }
  int available() { return 0; }
  int read() { return -1; }
  operator bool() { return true; }
};

extern SerialClass Serial;

class String {
public:
  String() = default;
  String(const char* s) : _data(s ? s : "") {}
  const char* c_str() const { return _data.c_str(); }
  size_t length() const { return _data.length(); }
  void trim() {}
  bool startsWith(const char* prefix) const { return _data.find(prefix) == 0; }
  String substring(size_t start) const { return String(_data.substr(start).c_str()); }
  int toInt() const { return std::stoi(_data); }
  String& operator+=(char c) {
    _data += c;
    return *this;
  }

private:
  std::string _data;
};
