#include "HomeAssistantActivity.h"

#include <ArduinoJson.h>
#include <Arduino.h>
#include <GfxRenderer.h>
#include <HalGPIO.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <PersistableStore.h>
#include <WiFi.h>

#include <cstdio>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "util/HomeAssistantUtil.h"

namespace fui = freeink::ui;

namespace {
constexpr const char* CONFIG_PATH = "/homeassistant.json";

// Written on first open so the file only needs the token filled in.
constexpr const char* DEFAULT_URL = "http://homeassistant.local:8123";
struct DefaultEntity {
  const char* name;
  const char* entityId;
};
constexpr DefaultEntity DEFAULT_ENTITIES[] = {
    {"Bibliotheeklamp", "light.bibliotheeklamp"},
    {"Keukentafellamp", "light.keukentafellamp"},
    {"Soundbar", "media_player.jbl_bar_9_1_3818"},
    {"Staande lampen", "light.staandelampen"},
};

// Home Assistant can take a moment to publish the new state after a toggle.
constexpr unsigned long STATE_SETTLE_MS = 600;

std::string trimmed(const char* text) {
  std::string s = text ? text : "";
  const size_t first = s.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return "";
  const size_t last = s.find_last_not_of(" \t\r\n");
  return s.substr(first, last - first + 1);
}
}  // namespace

void HomeAssistantActivity::onEnter() {
  CatalogActivity::onEnter();
  sleeping = false;
  lastInteractionMs = millis();
  state = State::CHECK_WIFI;
  statusMessage = tr(STR_CHECKING_WIFI);
  checkAndConnectWifi();
}

void HomeAssistantActivity::loop() {
  if (gpio.wasAnyPressed() || gpio.wasAnyReleased() || gpio.wasTouchActivity()) lastInteractionMs = millis();
  if (!sleeping && state == State::BROWSING && SETTINGS.homeAssistantPowerSave &&
      millis() - lastInteractionMs >= POWER_SAVE_IDLE_MS) {
    enterPowerSave();
  }
  CatalogActivity::loop();
}

// Wi-Fi off and a quiet screen until the next touch. The entries keep their
// last known states for when the list comes back.
void HomeAssistantActivity::enterPowerSave() {
  LOG_DBG("HA", "Power save: Wi-Fi off");
  sleeping = true;
  client.reset();  // stops mDNS before the radio goes away
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  requestUpdate();
}

// Same path as opening the screen: reconnect (saved network), then reload states.
void HomeAssistantActivity::wakeFromPowerSave() {
  LOG_DBG("HA", "Power save: waking");
  sleeping = false;
  lastInteractionMs = millis();
  state = State::CHECK_WIFI;
  statusMessage = tr(STR_CHECKING_WIFI);
  requestUpdate(true);
  checkAndConnectWifi();
}

bool HomeAssistantActivity::handleCustomInput() {
  if (!sleeping) return CatalogActivity::handleCustomInput();
  int x = 0, y = 0;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onBackButton();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) || mappedInput.wasScreenTapped(x, y)) {
    wakeFromPowerSave();
  }
  return true;  // nothing else is live while sleeping
}

void HomeAssistantActivity::onExit() {
  // Stops mDNS while Wi-Fi is still up; CatalogActivity::onExit() then drops
  // Wi-Fi and may restart, so nothing after it is guaranteed to run.
  client.reset();
  closeRouting();
  std::vector<fui::ListItem>().swap(rowItems);
  std::vector<Entry>().swap(entries);
  CatalogActivity::onExit();
}

void HomeAssistantActivity::failText(const std::string& message) {
  state = State::ERROR;
  errorMessage = message;
  requestUpdate();
}

void HomeAssistantActivity::failFor(const HomeAssistantClient::Status status) {
  using S = HomeAssistantClient::Status;
  switch (status) {
    case S::BadConfig:
      failText("Check the url, token and entities in /homeassistant.json");
      break;
    case S::CannotResolve:
      failText("Cannot find the Home Assistant host. Put its IP address in /homeassistant.json");
      break;
    case S::Unreachable:
      failText("Cannot reach Home Assistant. Check Wi-Fi and the url");
      break;
    case S::Unauthorized:
      failText("Home Assistant rejected the access token");
      break;
    case S::NotFound:
      failText("Home Assistant does not know that entity");
      break;
    case S::ServerError:
    case S::Ok:
      failText("Home Assistant returned an error");
      break;
  }
}

void HomeAssistantActivity::writeTemplateConfig() {
  JsonDocument doc;
  doc["url"] = DEFAULT_URL;
  doc["token"] = "";
  JsonArray list = doc["entities"].to<JsonArray>();
  for (const auto& e : DEFAULT_ENTITIES) {
    JsonObject obj = list.add<JsonObject>();
    obj["name"] = e.name;
    obj["entity_id"] = e.entityId;
  }
  if (!PersistableStoreBase::writeDocToFile(CONFIG_PATH, doc)) LOG_ERR("HA", "Could not write %s", CONFIG_PATH);
}

// Reads url, token and entities. Returns false after putting the reason on the
// error screen. The existing file is never overwritten.
bool HomeAssistantActivity::loadConfig(std::string& url, std::string& token) {
  if (!Storage.exists(CONFIG_PATH)) {
    writeTemplateConfig();
    failText("Created /homeassistant.json on the SD card. Add your Home Assistant access token to it, then open this "
             "screen again");
    return false;
  }

  JsonDocument doc;
  if (!PersistableStoreBase::readDocFromFile(CONFIG_PATH, doc)) {
    failText("Cannot read /homeassistant.json. Is it valid JSON?");
    return false;
  }
  const JsonVariantConst root = doc.as<JsonVariantConst>();
  url = trimmed(root["url"] | "");
  token = trimmed(root["token"] | "");
  if (url.empty() || token.empty()) {
    failText("Add the url and access token to /homeassistant.json");
    return false;
  }

  std::vector<Entry> loaded;
  loaded.reserve(MAX_ENTRIES);
  for (JsonObjectConst obj : root["entities"].as<JsonArrayConst>()) {
    if (loaded.size() >= MAX_ENTRIES) break;
    Entry entry;
    entry.entityId = trimmed(obj["entity_id"] | "");
    if (!haUtil::isValidEntityId(entry.entityId)) {
      LOG_ERR("HA", "Skipping invalid entity id '%s'", entry.entityId.c_str());
      continue;
    }
    entry.name = trimmed(obj["name"] | "");
    if (entry.name.empty()) entry.name = entry.entityId;
    loaded.push_back(std::move(entry));
  }
  if (loaded.empty()) {
    failText("No valid entities in /homeassistant.json");
    return false;
  }

  // The app's interaction table points at the old rows; stop routing touches
  // against it until the next render.
  closeRouting();
  {
    RenderLock lock(*this);
    entries = std::move(loaded);
    rebuildRowItems();
  }
  return true;
}

void HomeAssistantActivity::rebuildRowItems() {
  rowItems.clear();
  rowItems.reserve(entries.size());
  for (const auto& entry : entries) {
    fui::ListItem item;
    item.label = entry.name.c_str();
    item.subtitle = entry.entityId.c_str();
    item.value = entry.state;
    item.actionValue = static_cast<int16_t>(rowItems.size());
    rowItems.push_back(item);
  }
}

void HomeAssistantActivity::setEntryState(Entry& entry, const char* text) {
  // The render task reads this buffer through the row's value pointer.
  RenderLock lock(*this);
  std::snprintf(entry.state, sizeof(entry.state), "%s", text);
}

void HomeAssistantActivity::startBrowse() {
  using S = HomeAssistantClient::Status;
  nav.reset();
  beginLoading();
  client.reset();

  std::string url;
  std::string token;
  if (!loadConfig(url, token)) return;

  client = makeUniqueNoThrow<HomeAssistantClient>(url, token);
  if (!client) {
    LOG_ERR("HA", "OOM: client");
    fail(StrId::STR_MEMORY_ERROR);
    return;
  }
  if (const S begun = client->begin(); begun != S::Ok) {
    failFor(begun);
    return;
  }

  // Current states, so the list shows what each device is doing right now.
  for (auto& entry : entries) {
    char text[STATE_CAP];
    const S status = client->fetchState(entry.entityId, text, sizeof(text));
    if (status == S::Ok) {
      setEntryState(entry, text[0] ? text : "?");
    } else if (status == S::NotFound) {
      setEntryState(entry, "Not found");
    } else {
      // Token, host and Wi-Fi problems affect every row alike.
      failFor(status);
      return;
    }
  }
  state = State::BROWSING;
  requestUpdate();
}

void HomeAssistantActivity::activateIndex(const int index) {
  using S = HomeAssistantClient::Status;
  if (state != State::BROWSING || !client || index < 0 || index >= static_cast<int>(entries.size())) return;
  Entry& entry = entries[index];

  char text[STATE_CAP];
  S status = client->toggle(entry.entityId, text, sizeof(text));
  if (status == S::Ok && text[0] == '\0') {
    // The reply listed no change yet; ask for the state once it has settled.
    delay(STATE_SETTLE_MS);
    status = client->fetchState(entry.entityId, text, sizeof(text));
  }

  if (status == S::Ok) {
    setEntryState(entry, text[0] ? text : "Sent");
  } else if (status == S::NotFound) {
    setEntryState(entry, "Not found");
  } else if (status == S::ServerError) {
    setEntryState(entry, "Error");
  } else {
    failFor(status);
    return;
  }
  requestUpdate();
}

void HomeAssistantActivity::buildScreen(UiScreen& screen) {
  if (sleeping) {
    screenHeader(screen, tr(STR_SLEEPING));
    screen.centeredText(tr(STR_TOUCH_TO_WAKE), screen.theme().bodyText);
    return;
  }
  screenHeader(screen, tr(STR_HOME_ASSISTANT));
  if (buildStatusScreen(screen)) return;

  if (rowItems.empty()) {
    screen.centeredText(tr(STR_NO_ENTRIES), screen.theme().bodyText);
    return;
  }
  fui::ListProps props;
  props.items = rowItems.data();
  props.count = static_cast<uint16_t>(rowItems.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;  // physical buttons stay in loop()
  syncListViewport(screen, props);
  screen.list(props);
}

void HomeAssistantActivity::drawFooter() {
  MappedInputManager::Labels labels;
  if (sleeping) {
    labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    return;
  }
  switch (state) {
    case State::BROWSING:
      labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
      break;
    case State::ERROR:
      labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_RETRY), "", "");
      break;
    default:
      labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
      break;
  }
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}
