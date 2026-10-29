#pragma once

#include <cstddef>
#include <cstdint>

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

// 31-byte advertisement minus flags (3) and a 10-character name (12), minus the
// service-data header (4), leaves 12 bytes: device info + packet id (3) + 4 buttons.
constexpr size_t MAX_BUTTONS = 4;
constexpr size_t MAX_PAYLOAD = 3 + 2 * MAX_BUTTONS;

// Writes the payload for "button `pressedIndex` of `buttonCount` was pressed".
// `packetId` changes on every press so Home Assistant ignores the repeats of one
// press. Returns the byte count, or 0 when the arguments or buffer do not fit.
inline size_t buildButtonPayload(const uint8_t packetId, const size_t buttonCount, const size_t pressedIndex,
                                 uint8_t* out, const size_t cap) {
  if (!out || buttonCount == 0 || buttonCount > MAX_BUTTONS || pressedIndex >= buttonCount) return 0;
  const size_t size = 3 + 2 * buttonCount;
  if (cap < size) return 0;
  size_t n = 0;
  out[n++] = DEVICE_INFO_V2_TRIGGER;
  out[n++] = OBJ_PACKET_ID;
  out[n++] = packetId;
  for (size_t i = 0; i < buttonCount; ++i) {
    out[n++] = OBJ_BUTTON_EVENT;
    out[n++] = i == pressedIndex ? EVENT_PRESS : EVENT_NONE;
  }
  return n;
}

}  // namespace bthome
