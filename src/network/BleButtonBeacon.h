#pragma once

#include <cstddef>

// Set by the board's PlatformIO env (with the NimBLE-Arduino lib_dep). Without
// it the beacon links as a stub that reports Bluetooth as unavailable.
#ifndef CROSSPOINT_HA_BLE
#define CROSSPOINT_HA_BLE 0
#endif

/**
 * Broadcasts BTHome button events that Home Assistant's Bluetooth integration
 * turns into event entities. Send-only: no connection, pairing or reply.
 */
class BleButtonBeacon {
 public:
  ~BleButtonBeacon() { end(); }

  // True when this firmware was built with Bluetooth.
  static constexpr bool available() { return CROSSPOINT_HA_BLE != 0; }

  // Advertises "button `index` of `count` pressed" for a few seconds. The radio
  // is started by the first press and stays off until then.
  bool press(size_t index, size_t count);
  // Call regularly: shuts the radio down once the advertisement has finished.
  void service();
  // Stops advertising and releases the BLE stack. Safe to call repeatedly.
  void end();

 private:
  bool start();

  bool started = false;
  unsigned char packetId = 0;
  unsigned long stopAtMs = 0;
};
