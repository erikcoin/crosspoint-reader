#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

// Pure helpers for the BTHome v2 button advertisement Home Assistant reads over
// Bluetooth LE. No Arduino or BLE dependencies, so they run in the host tests.
//
// Service data under UUID 0xFCD2:
//   [device info][object id][value]...   objects in ascending id order
// Device info 0x44 = BTHome version 2 (bits 5-7), trigger based (bit 2),
// not encrypted (bit 0). Button event objects (0x3A) repeat once per button;
// Home Assistant creates "Button 1..N" event entities in that order.
namespace bthome {

constexpr uint16_t SERVICE_UUID = 0xFCD2;
constexpr uint8_t DEVICE_INFO_V2_TRIGGER = 0x44;
constexpr uint8_t OBJ_PACKET_ID = 0x00;
constexpr uint8_t OBJ_BUTTON_EVENT = 0x3A;
constexpr uint8_t EVENT_NONE = 0x00;
constexpr uint8_t EVENT_PRESS = 0x01;
constexpr uint8_t EVENT_DOUBLE_PRESS = 0x02;
constexpr uint8_t EVENT_TRIPLE_PRESS = 0x03;
constexpr uint8_t EVENT_LONG_PRESS = 0x04;
constexpr uint8_t EVENT_LONG_DOUBLE_PRESS = 0x05;
constexpr uint8_t EVENT_LONG_TRIPLE_PRESS = 0x06;

// Maps the Home Assistant event name ("long_press") to its BTHome value, or 0
// (EVENT_NONE) when the name is unknown.
inline uint8_t eventFromName(const char* name) {
  if (!name) return EVENT_NONE;
  struct Pair {
    const char* name;
    uint8_t value;
  };
  static constexpr Pair kEvents[] = {{"press", EVENT_PRESS},
                                     {"double_press", EVENT_DOUBLE_PRESS},
                                     {"triple_press", EVENT_TRIPLE_PRESS},
                                     {"long_press", EVENT_LONG_PRESS},
                                     {"long_double_press", EVENT_LONG_DOUBLE_PRESS},
                                     {"long_triple_press", EVENT_LONG_TRIPLE_PRESS}};
  for (const auto& e : kEvents) {
    if (std::strcmp(name, e.name) == 0) return e.value;
  }
  return EVENT_NONE;
}

// 31-byte advertisement = flags (3) + a 5-character name (7) + service-data
// header (4) + device info and packet id (3) + 7 buttons (14). Exactly full.
constexpr size_t MAX_BUTTONS = 7;
constexpr size_t MAX_PAYLOAD = 3 + 2 * MAX_BUTTONS;

// Writes the payload for "button `pressedIndex` of `buttonCount` had `event`"
// (a plain press unless another event is given).
// `packetId` changes on every press so Home Assistant ignores the repeats of one
// press. Returns the byte count, or 0 when the arguments or buffer do not fit.
inline size_t buildButtonPayload(const uint8_t packetId, const size_t buttonCount, const size_t pressedIndex,
                                 uint8_t* out, const size_t cap, const uint8_t event = EVENT_PRESS) {
  if (event < EVENT_PRESS || event > EVENT_LONG_TRIPLE_PRESS) return 0;
  if (!out || buttonCount == 0 || buttonCount > MAX_BUTTONS || pressedIndex >= buttonCount) return 0;
  const size_t size = 3 + 2 * buttonCount;
  if (cap < size) return 0;
  size_t n = 0;
  out[n++] = DEVICE_INFO_V2_TRIGGER;
  out[n++] = OBJ_PACKET_ID;
  out[n++] = packetId;
  for (size_t i = 0; i < buttonCount; ++i) {
    out[n++] = OBJ_BUTTON_EVENT;
    out[n++] = i == pressedIndex ? event : EVENT_NONE;
  }
  return n;
}

}  // namespace bthome
