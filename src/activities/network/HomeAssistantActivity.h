#pragma once

#include <memory>
#include <string>
#include <vector>

#include "activities/CatalogActivity.h"
#include "network/HomeAssistantClient.h"

/**
 * Remote control for Home Assistant entities. Shows one row per entity from
 * /homeassistant.json on the SD card; tapping a row toggles that entity and
 * shows the state Home Assistant reports back.
 */
class HomeAssistantActivity final : public CatalogActivity {
 public:
  explicit HomeAssistantActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : CatalogActivity("HomeAssistant", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;

 private:
  static constexpr size_t MAX_ENTRIES = 8;
  static constexpr size_t STATE_CAP = 16;

  struct Entry {
    std::string name;
    std::string entityId;
    char state[STATE_CAP] = {};
  };

  std::vector<Entry> entries;
  // Row buffer derived from `entries`, rebuilt only when the list is reloaded
  // so repaints reuse it. Rows point at the entries' strings and state buffers.
  std::vector<freeink::ui::ListItem> rowItems;
  std::unique_ptr<HomeAssistantClient> client;

  int listCount() const override { return state == State::BROWSING ? static_cast<int>(entries.size()) : 0; }
  bool hasSearch() const override { return false; }
  void performSearch(const std::string&) override {}
  void downloadFinished(bool) override {}

  void startBrowse() override;
  void activateIndex(int index) override;
  void buildScreen(UiScreen& screen) override;
  void drawFooter() override;

  bool loadConfig(std::string& url, std::string& token);
  void writeTemplateConfig();
  void rebuildRowItems();
  void setEntryState(Entry& entry, const char* text);
  void failText(const std::string& message);
  void failFor(HomeAssistantClient::Status status);
};
