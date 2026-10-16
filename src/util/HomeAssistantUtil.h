#pragma once

#include <cstddef>
#include <cstring>
#include <string>

// Pure helpers for the Home Assistant remote: entity-id checks, REST paths,
// URL splitting and reading the "state" value out of a response head. No
// Arduino or network dependencies, so they run in the host tests.
namespace haUtil {

// "light.kitchen_lamp": lowercase domain, one dot, object id. The id ends up in
// a request path, so anything else is refused instead of escaped.
inline bool isValidEntityId(const std::string& id) {
  const size_t dot = id.find('.');
  if (dot == std::string::npos || dot == 0 || dot + 1 >= id.size() || id.size() > 100) return false;
  if (id.find('.', dot + 1) != std::string::npos) return false;
  for (const char c : id) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.';
    if (!ok) return false;
  }
  return true;
}

inline std::string domainOf(const std::string& entityId) { return entityId.substr(0, entityId.find('.')); }

inline std::string statePath(const std::string& entityId) { return "/api/states/" + entityId; }

// homeassistant.toggle would work for every domain, but the domain's own toggle
// service is the one that exists for lights, switches and media players.
inline std::string toggleServicePath(const std::string& entityId) {
  return "/api/services/" + domainOf(entityId) + "/toggle";
}

inline std::string toggleBody(const std::string& entityId) { return "{\"entity_id\":\"" + entityId + "\"}"; }

struct UrlParts {
  std::string scheme;  // "http" or "https"
  std::string host;
  std::string port;  // empty when the URL has none
};

// Accepts http(s)://host[:port][/]. A path is not supported: Home Assistant's
// API sits at the root of its origin.
inline bool splitBaseUrl(const std::string& url, UrlParts& out) {
  size_t schemeEnd = url.find("://");
  if (schemeEnd == std::string::npos) return false;
  out.scheme = url.substr(0, schemeEnd);
  if (out.scheme != "http" && out.scheme != "https") return false;

  std::string rest = url.substr(schemeEnd + 3);
  while (!rest.empty() && rest.back() == '/') rest.pop_back();
  if (rest.empty() || rest.find('/') != std::string::npos || rest.find('[') != std::string::npos) return false;

  const size_t colon = rest.find(':');
  if (colon == std::string::npos) {
    out.host = rest;
    out.port.clear();
  } else {
    out.host = rest.substr(0, colon);
    out.port = rest.substr(colon + 1);
    if (out.port.empty() || out.port.size() > 5) return false;
    for (const char c : out.port) {
      if (c < '0' || c > '9') return false;
    }
  }
  return !out.host.empty();
}

inline std::string joinBaseUrl(const UrlParts& parts) {
  std::string url = parts.scheme + "://" + parts.host;
  if (!parts.port.empty()) url += ":" + parts.port;
  return url;
}

// True for "homeassistant.local": the lwIP resolver cannot answer these, so the
// caller asks mDNS for the address instead.
inline bool isMdnsHost(const std::string& host) {
  static constexpr char kSuffix[] = ".local";
  constexpr size_t suffixLen = sizeof(kSuffix) - 1;
  if (host.size() <= suffixLen) return false;
  const char* tail = host.c_str() + host.size() - suffixLen;
  for (size_t i = 0; i < suffixLen; ++i) {
    const char c = tail[i] >= 'A' && tail[i] <= 'Z' ? static_cast<char>(tail[i] + ('a' - 'A')) : tail[i];
    if (c != kSuffix[i]) return false;
  }
  return true;
}

// Copies the first top-level-looking "state" string of `body` into `out`
// (NUL-terminated, first letter upper-cased for display). `body` may be only
// the head of a response: a value cut off before its closing quote counts as
// not found. Returns false when no complete state value is present.
inline bool parseState(const char* body, const size_t len, char* out, const size_t cap) {
  if (!body || !out || cap < 2) return false;
  static constexpr char kKey[] = "\"state\"";
  constexpr size_t keyLen = sizeof(kKey) - 1;
  for (size_t i = 0; i + keyLen <= len; ++i) {
    if (std::memcmp(body + i, kKey, keyLen) != 0) continue;
    size_t p = i + keyLen;
    while (p < len && body[p] == ' ') ++p;
    if (p >= len || body[p] != ':') continue;
    ++p;
    while (p < len && body[p] == ' ') ++p;
    if (p >= len || body[p] != '"') continue;
    ++p;
    size_t n = 0;
    while (p < len && body[p] != '"' && n + 1 < cap) out[n++] = body[p++];
    // Stopped for lack of input rather than at the closing quote or a full
    // buffer: the value was cut off, so do not show half of it.
    if (p >= len && n + 1 < cap) return false;
    if (n == 0) return false;
    out[n] = '\0';
    if (out[0] >= 'a' && out[0] <= 'z') out[0] = static_cast<char>(out[0] - ('a' - 'A'));
    return true;
  }
  return false;
}

}  // namespace haUtil
