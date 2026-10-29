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

  // Starts the BLE stack. False when this build has no Bluetooth or it failed.
  bool begin();
  // Advertises "button `index` of `count` pressed" for a few seconds.
  bool press(size_t index, size_t count);
  // Stops advertising and releases the BLE stack. Safe to call repeatedly.
  void end();

 private:
  bool started = false;
  unsigned char packetId = 0;
};
