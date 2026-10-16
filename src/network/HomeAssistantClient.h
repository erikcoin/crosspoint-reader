#pragma once

#include <cstddef>
#include <string>
#include <utility>

/**
 * Minimal Home Assistant REST client: read an entity's state and toggle it.
 * Talks to http(s)://host[:port]/api with a long-lived access token. One
 * short-lived connection per call; the caller owns Wi-Fi.
 */
class HomeAssistantClient {
 public:
  enum class Status {
    Ok,
    BadConfig,      // unusable URL or entity id
    CannotResolve,  // .local name not answered over mDNS
    Unreachable,    // no HTTP response (Wi-Fi, host down, timeout)
    Unauthorized,   // 401/403: token missing, wrong or revoked
    NotFound,       // 404: no such entity or service
    ServerError,    // any other non-2xx
  };

  HomeAssistantClient(std::string baseUrl, std::string token) : baseUrl(std::move(baseUrl)), token(std::move(token)) {}
  ~HomeAssistantClient() { end(); }
  HomeAssistantClient(const HomeAssistantClient&) = delete;
  HomeAssistantClient& operator=(const HomeAssistantClient&) = delete;

  // Validates the URL and, for a ".local" host, resolves it over mDNS. Call
  // once after Wi-Fi is up; the other calls need it to have returned Ok.
  Status begin();
  // Stops mDNS if begin() started it. Safe to call repeatedly.
  void end();

  // `stateOut` receives the display text ("On", "Off", ...), or "" when the
  // response carried none. It is NUL-terminated within `cap` bytes.
  Status fetchState(const std::string& entityId, char* stateOut, size_t cap);
  // Calls <domain>.toggle. When the reply reports the changed state it is
  // returned in `stateOut`, otherwise `stateOut` is "".
  Status toggle(const std::string& entityId, char* stateOut, size_t cap);

 private:
  Status request(const char* method, const std::string& path, const std::string& body, char* stateOut, size_t cap);

  std::string baseUrl;
  std::string token;
  std::string resolvedBase;  // baseUrl with a .local host replaced by its address
  bool mdnsStarted = false;
};
