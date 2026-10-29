#include "BleButtonBeacon.h"

#if CROSSPOINT_HA_BLE

#include <Logging.h>
#include <NimBLEDevice.h>

#include "util/BthomeUtil.h"

namespace {
constexpr const char* DEVICE_NAME = "CrossPoint";
// Home Assistant scans in duty cycles, so one advertisement can be missed. Keep
// repeating the same packet (same packet id) and let Home Assistant drop copies.
constexpr uint32_t ADVERTISE_MS = 3000;
constexpr uint16_t ADV_INTERVAL = 160;  // 160 * 0.625 ms = 100 ms
}  // namespace

bool BleButtonBeacon::begin() {
  if (started) return true;
  if (!NimBLEDevice::init(DEVICE_NAME)) {
    LOG_ERR("BLE", "NimBLE init failed");
    return false;
  }
  started = true;
  return true;
}

bool BleButtonBeacon::press(const size_t index, const size_t count) {
  if (!started) return false;

  uint8_t payload[bthome::MAX_PAYLOAD];
  const size_t size = bthome::buildButtonPayload(++packetId, count, index, payload, sizeof(payload));
  if (size == 0) return false;

  NimBLEAdvertisementData data;
  data.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  data.setName(DEVICE_NAME);
  if (!data.setServiceData(NimBLEUUID(bthome::SERVICE_UUID), payload, size)) return false;

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->stop();
  adv->enableScanResponse(false);
  adv->setConnectableMode(BLE_GAP_CONN_MODE_NON);
  adv->setMinInterval(ADV_INTERVAL);
  adv->setMaxInterval(ADV_INTERVAL);
  if (!adv->setAdvertisementData(data)) return false;
  const bool ok = adv->start(ADVERTISE_MS);
  LOG_DBG("BLE", "Button %u/%u packet %u -> %s", static_cast<unsigned>(index + 1), static_cast<unsigned>(count),
          static_cast<unsigned>(packetId), ok ? "advertising" : "failed");
  return ok;
}

void BleButtonBeacon::end() {
  if (!started) return;
  NimBLEDevice::getAdvertising()->stop();
  NimBLEDevice::deinit(true);
  started = false;
}

#else  // no Bluetooth in this build

bool BleButtonBeacon::begin() { return false; }
bool BleButtonBeacon::press(size_t, size_t) { return false; }
void BleButtonBeacon::end() {}

#endif
