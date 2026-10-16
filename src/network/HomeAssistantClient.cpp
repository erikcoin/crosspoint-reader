#include "HomeAssistantClient.h"

#include <ESPmDNS.h>
#include <Logging.h>
#include <SecureHttpClient.h>

#include <cstring>

#include "WifiPowerSaveGuard.h"
#include "util/HomeAssistantUtil.h"

namespace {
// A LAN host that does not answer should not freeze the screen for a minute.
constexpr uint32_t REQUEST_TIMEOUT_MS = 6000;
constexpr uint32_t MDNS_QUERY_TIMEOUT_MS = 3000;
// "state" sits right after "entity_id" at the start of the JSON, so the head
// of the body is enough; attributes after it can run to several KB.
constexpr size_t RESPONSE_HEAD_BYTES = 256;
constexpr const char* MDNS_NAME = "crosspoint-reader";
}  // namespace

HomeAssistantClient::Status HomeAssistantClient::begin() {
  resolvedBase.clear();
  haUtil::UrlParts parts;
  if (token.empty() || !haUtil::splitBaseUrl(baseUrl, parts)) return Status::BadConfig;

  if (haUtil::isMdnsHost(parts.host)) {
    if (!mdnsStarted) {
      mdnsStarted = MDNS.begin(MDNS_NAME);
      if (!mdnsStarted) {
        LOG_ERR("HA", "mDNS did not start");
        return Status::CannotResolve;
      }
    }
    const std::string name = parts.host.substr(0, parts.host.size() - 6);  // drop ".local"
    const IPAddress ip = MDNS.queryHost(name.c_str(), MDNS_QUERY_TIMEOUT_MS);
    if (ip == IPAddress(0, 0, 0, 0)) {
      LOG_ERR("HA", "mDNS lookup failed for %s", parts.host.c_str());
      return Status::CannotResolve;
    }
    parts.host = ip.toString().c_str();
    LOG_DBG("HA", "%s resolved to %s", name.c_str(), parts.host.c_str());
  }
  resolvedBase = haUtil::joinBaseUrl(parts);
  return Status::Ok;
}

void HomeAssistantClient::end() {
  if (mdnsStarted) {
    MDNS.end();
    mdnsStarted = false;
  }
}

HomeAssistantClient::Status HomeAssistantClient::fetchState(const std::string& entityId, char* stateOut,
                                                            const size_t cap) {
  if (!haUtil::isValidEntityId(entityId)) return Status::BadConfig;
  return request("GET", haUtil::statePath(entityId), "", stateOut, cap);
}

HomeAssistantClient::Status HomeAssistantClient::toggle(const std::string& entityId, char* stateOut,
                                                        const size_t cap) {
  if (!haUtil::isValidEntityId(entityId)) return Status::BadConfig;
  return request("POST", haUtil::toggleServicePath(entityId), haUtil::toggleBody(entityId), stateOut, cap);
}

HomeAssistantClient::Status HomeAssistantClient::request(const char* method, const std::string& path,
                                                         const std::string& body, char* stateOut, const size_t cap) {
  if (stateOut && cap > 0) stateOut[0] = '\0';
  if (resolvedBase.empty()) return Status::BadConfig;

  // Modem sleep adds seconds of latency to a request/response exchange.
  WifiPowerSaveGuard psGuard;
  freeink::SecureHttpClient http;
  http.setInsecure();  // https only matters for a self-signed local install
  http.setReuse(false);
  http.setTimeout(REQUEST_TIMEOUT_MS);
  if (!http.begin(resolvedBase + path)) return Status::BadConfig;
  http.addHeader("Authorization", "Bearer " + token);
  if (!body.empty()) http.addHeader("Content-Type", "application/json");

  char head[RESPONSE_HEAD_BYTES];
  size_t headLen = 0;
  const uint8_t* payload = body.empty() ? nullptr : reinterpret_cast<const uint8_t*>(body.data());
  const int status = http.sendRequest(method, payload, body.size(), [&](const uint8_t* data, const size_t len) {
    const size_t take = len < sizeof(head) - headLen ? len : sizeof(head) - headLen;
    std::memcpy(head + headLen, data, take);
    headLen += take;
    return true;  // keep reading so the transfer completes cleanly
  });
  http.end();

  LOG_DBG("HA", "%s %s -> %d", method, path.c_str(), status);
  if (status < 0) return Status::Unreachable;
  if (status == 401 || status == 403) return Status::Unauthorized;
  if (status == 404) return Status::NotFound;
  if (status < 200 || status >= 300) return Status::ServerError;

  if (stateOut && cap > 0) haUtil::parseState(head, headLen, stateOut, cap);
  return Status::Ok;
}
