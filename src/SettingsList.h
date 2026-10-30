#pragma once

#include <BoardConfig.h>
#include <HalClock.h>
#include <HalFrontlight.h>
#include <I18n.h>

#include <algorithm>
#include <cstring>
#include <iterator>
#include <string>
#include <vector>

#include "CrossPointSettings.h"
#include "HomeButtonSettings.h"
#include "activities/settings/SettingsActivity.h"
#include "components/UITheme.h"

inline std::vector<StrId> buildShortPowerButtonValues() {
  std::vector<StrId> values = {StrId::STR_IGNORE, StrId::STR_SLEEP, StrId::STR_FORCE_REFRESH};
  values.reserve(CrossPointSettings::SHORT_PWRBTN_COUNT);
  if (BoardConfig::hasTouch()) values.push_back(StrId::STR_CONFIRM);
  return values;
}

inline uint8_t shortPowerButtonDisplayValue() {
  const uint8_t raw = SETTINGS.shortPwrBtn;
  if (raw <= CrossPointSettings::FORCE_REFRESH) return raw;
  if (raw == CrossPointSettings::PWR_CONFIRM && BoardConfig::hasTouch()) return CrossPointSettings::PWR_CONFIRM;
  return CrossPointSettings::IGNORE;
}

inline void setShortPowerButtonFromDisplayValue(const uint8_t displayValue) { SETTINGS.shortPwrBtn = displayValue; }

inline SettingInfo buildShortPowerButtonSetting() {
  return SettingInfo::DynamicEnum(StrId::STR_SHORT_PWR_BTN, buildShortPowerButtonValues(), shortPowerButtonDisplayValue,
                                  setShortPowerButtonFromDisplayValue, "shortPwrBtn", StrId::STR_CAT_CONTROLS);
}

inline std::vector<StrId> homeThemeValues() {
  return {StrId::STR_THEME_CLASSIC, StrId::STR_THEME_LYRA, StrId::STR_THEME_ROUNDEDRAFF};
}

// Shared settings list used by both the device settings UI and the web settings API.
// Each entry has a key (for JSON API) and category (for grouping).
// ACTION-type entries and entries without a key are device-only.
//
// The static list is constructed exactly once (master's optimization, #1086 +
// #1636) so the per-entry SettingInfo cost is paid once; every call then copies
// it.
// forPersistence: fromJson()/toJson() pass true so the runtime frontlight probe
// cannot drop a persisted key. Frontlight.present() is false until
// Frontlight.begin() runs the I2C probe, which happens AFTER SETTINGS.loadFromFile();
// filtering the persistence list on it would fail to restore (and later overwrite)
// STR_RESTORE_LIGHT_ON_WAKE on frontlit EEGO A4 units. The UI list still filters it.
inline std::vector<SettingInfo> getSettingsList(bool forPersistence = false) {
  static const std::vector<SettingInfo> baseList = [] {
    // Enum settings are persisted as numeric values. Assign these labels by enum
    // value so a reordered menu or enum cannot silently swap their behavior.
    std::vector<StrId> sleepScreenValues(CrossPointSettings::SLEEP_SCREEN_MODE_COUNT);
    sleepScreenValues[CrossPointSettings::DARK] = StrId::STR_DARK;
    sleepScreenValues[CrossPointSettings::LIGHT] = StrId::STR_LIGHT;
    sleepScreenValues[CrossPointSettings::CUSTOM] = StrId::STR_CUSTOM;
    sleepScreenValues[CrossPointSettings::BLANK] = StrId::STR_NONE_OPT;
    sleepScreenValues[CrossPointSettings::QUICK_RESUME] = StrId::STR_QUICK_RESUME;
    sleepScreenValues[CrossPointSettings::TRANSPARENT_CUSTOM] = StrId::STR_TRANSPARENT;

    std::vector<SettingInfo> v = {
        // --- Display ---
        SettingInfo::Enum(StrId::STR_SLEEP_SCREEN, &CrossPointSettings::sleepScreen, std::move(sleepScreenValues),
                          "sleepScreen", StrId::STR_CAT_DISPLAY),
        SettingInfo::Enum(StrId::STR_SLEEP_COVER_MODE, &CrossPointSettings::sleepScreenCoverMode,
                          {StrId::STR_FIT, StrId::STR_CROP}, "sleepScreenCoverMode", StrId::STR_CAT_DISPLAY),
        SettingInfo::Enum(StrId::STR_SLEEP_COVER_FILTER, &CrossPointSettings::sleepScreenCoverFilter,
                          {StrId::STR_NONE_OPT, StrId::STR_FILTER_CONTRAST, StrId::STR_INVERTED},
                          "sleepScreenCoverFilter", StrId::STR_CAT_DISPLAY),
        SettingInfo::Enum(StrId::STR_QUICK_RESUME_TIMEOUT, &CrossPointSettings::quickResumeSleepScreen,
                          {StrId::STR_STATE_OFF, StrId::STR_STATE_ON}, "quickResumeSleepScreen",
                          StrId::STR_CAT_DISPLAY),
        SettingInfo::Enum(StrId::STR_HIDE_BATTERY, &CrossPointSettings::hideBatteryPercentage,
                          {StrId::STR_NEVER, StrId::STR_IN_READER, StrId::STR_ALWAYS}, "hideBatteryPercentage",
                          StrId::STR_CAT_DISPLAY),
        SettingInfo::Enum(StrId::STR_UI_THEME, &CrossPointSettings::uiTheme, homeThemeValues(), "uiTheme",
                          StrId::STR_CAT_DISPLAY),
        SettingInfo::Enum(
            StrId::STR_ORIENTATION, &CrossPointSettings::orientation,
            {StrId::STR_PORTRAIT, StrId::STR_LANDSCAPE_CW, StrId::STR_ORIENTATION_INVERTED, StrId::STR_LANDSCAPE_CCW},
            "orientation", StrId::STR_CAT_DISPLAY),
        SettingInfo::Toggle(StrId::STR_SUNLIGHT_FADING_FIX, &CrossPointSettings::fadingFix, "fadingFix",
                            StrId::STR_CAT_DISPLAY),
#if FREEINK_CAP_FRONTLIGHT
        SettingInfo::Toggle(StrId::STR_RESTORE_LIGHT_ON_WAKE, &CrossPointSettings::frontlightRestoreOnWake,
                            "frontlightRestoreOnWake", StrId::STR_CAT_DISPLAY),
#endif
        // Night mode = inverted output polarity everywhere (ActivityManager
        // applies it to every activity), so it lives in the Display category.
        SettingInfo::Toggle(StrId::STR_NIGHT_MODE, &CrossPointSettings::screenInverted, "screenInverted",
                            StrId::STR_CAT_DISPLAY),

    // --- Controls ---
#if FREEINK_CAP_HAPTIC
        SettingInfo::Enum(StrId::STR_VIBRATION, &CrossPointSettings::vibration,
                          {StrId::STR_VIBRATION_TOUCH, StrId::STR_VIBRATION_TOUCH_PAGE, StrId::STR_STATE_OFF},
                          "vibration", StrId::STR_CAT_CONTROLS),
        SettingInfo::Enum(StrId::STR_HAPTIC_INTENSITY, &CrossPointSettings::hapticIntensity,
                          {StrId::STR_HAPTIC_LOW, StrId::STR_HAPTIC_MEDIUM, StrId::STR_HAPTIC_HIGH}, "hapticIntensity",
                          StrId::STR_CAT_CONTROLS),
#endif
        // Erased below unless the board is an X4 Pro.
        SettingInfo::Toggle(StrId::STR_DBL_CLICK_PWR_LIGHT, &CrossPointSettings::doubleClickPwrLight,
                            "doubleClickPwrLight", StrId::STR_CAT_CONTROLS),
        SettingInfo::Toggle(StrId::STR_RIGHT_BTN_LIGHT, &CrossPointSettings::rightButtonLight, "rightButtonLight",
                            StrId::STR_CAT_CONTROLS),
        buildShortPowerButtonSetting(),

        // --- System ---
        SettingInfo::Value(
            StrId::STR_TIME_TO_SLEEP, &CrossPointSettings::sleepTimeoutMinutes,
            {CrossPointSettings::MIN_SLEEP_TIMEOUT_MINUTES, CrossPointSettings::MAX_SLEEP_TIMEOUT_MINUTES, 1},
            "sleepTimeoutMinutes", StrId::STR_CAT_SYSTEM),
        SettingInfo::Toggle(StrId::STR_HA_POWER_SAVE, &CrossPointSettings::homeAssistantPowerSave,
                            "homeAssistantPowerSave", StrId::STR_CAT_SYSTEM),
        SettingInfo::Enum(StrId::STR_HA_LINK, &CrossPointSettings::homeAssistantLink,
                          {StrId::STR_WIFI, StrId::STR_BLUETOOTH}, "homeAssistantLink", StrId::STR_CAT_SYSTEM),
        // Frontlight quick-panel state: persisted and web-exposed, but hidden
        // from the on-device Settings screen because the swipe panel owns it.
        SettingInfo::Value(StrId::STR_BRIGHTNESS, &CrossPointSettings::frontlightBrightness, {0, 100, 5},
                           "frontlightBrightness"),
#if FREEINK_CAP_WARMLIGHT
        SettingInfo::Value(StrId::STR_WARMTH, &CrossPointSettings::frontlightWarmth, {0, 100, 5}, "frontlightWarmth"),
#endif
        SettingInfo::Toggle(StrId::STR_FRONTLIGHT, &CrossPointSettings::frontlightOn, "frontlightOn"),

        // Clock entries (persistence + web settings; the device UI is
        // ClockSettingsActivity under System settings).
        // LEGACY: retired quarter-hour UTC offset (biased by 48). Persisted so
        // timezones::activeIndex() can migrate it into clockTimezone.
        SettingInfo::Value(StrId::STR_CLOCK, &CrossPointSettings::clockUtcOffsetQ, {0, 104, 1}, "clockUtcOffsetQ",
                           StrId::STR_CUSTOMISE_STATUS_BAR),
        SettingInfo::Enum(StrId::STR_CLOCK_FORMAT, &CrossPointSettings::clockFormat,
                          {StrId::STR_CLOCK_FORMAT_24H, StrId::STR_CLOCK_FORMAT_12H}, "clockFormat",
                          StrId::STR_CUSTOMISE_STATUS_BAR),
        // Index into the append-only table in src/util/Timezones.cpp; 255 = unset.
        SettingInfo::Value(StrId::STR_TIMEZONE, &CrossPointSettings::clockTimezone, {0, 255, 1}, "clockTimezone",
                           StrId::STR_CUSTOMISE_STATUS_BAR),
        SettingInfo::Enum(StrId::STR_CLOCK_DST, &CrossPointSettings::clockDst,
                          {StrId::STR_CLOCK_DST_AUTO, StrId::STR_STATE_ON, StrId::STR_STATE_OFF}, "clockDst",
                          StrId::STR_CUSTOMISE_STATUS_BAR),
        SettingInfo::Toggle(StrId::STR_CLOCK_IN_HEADER, &CrossPointSettings::clockShowInHeader, "clockShowHeader",
                            StrId::STR_CUSTOMISE_STATUS_BAR),
        // Persistence flag for NTP debounce. Resetting from the web UI forces a re-sync
        // on next WiFi connect, which is useful when crossing time zones.
        SettingInfo::Toggle(StrId::STR_CLOCK_SYNCED, &CrossPointSettings::clockHasBeenSynced, "clockHasBeenSynced",
                            StrId::STR_CUSTOMISE_STATUS_BAR),
    };
    // Erasing keeps the list at its initial allocation; inserting into a full
    // vector would reallocate it at double capacity for the process lifetime.
    const auto eraseEntry = [&v](const StrId nameId) {
      v.erase(std::find_if(v.begin(), v.end(), [nameId](const SettingInfo& s) { return s.nameId == nameId; }));
    };
    // Double-click power frontlight shortcut only exists on the X4 Pro.
    if (!BoardConfig::isX4Pro()) eraseEntry(StrId::STR_DBL_CLICK_PWR_LIGHT);
    return v;
  }();

  std::vector<SettingInfo> v = baseList;
  if (BoardConfig::hasHomeKey()) {
    v.reserve(v.size() + 3);
    for (unsigned i = 0; i < 3; ++i) {
      v.push_back(SettingInfo::StaticEnum(home_button::GESTURE_LABELS[i], home_button::FIELDS[i],
                                          home_button::ACTION_LABELS, home_button::KEYS[i], StrId::STR_CAT_CONTROLS));
    }
  }
  if (BoardConfig::hasTouch()) {
    v.erase(std::remove_if(v.begin(), v.end(),
                           [](const SettingInfo& s) { return s.nameId == StrId::STR_SUNLIGHT_FADING_FIX; }),
            v.end());
  }
  // Frontlit and lightless variants of a board share one binary (EEGO A4);
  // presence is the I2C probe result from Frontlight.begin(). UI-only: the probe
  // has not run at load time, so persistence must keep the key (forPersistence).
  if (!forPersistence && !Frontlight.present()) {
    v.erase(std::remove_if(v.begin(), v.end(),
                           [](const SettingInfo& s) {
                             return s.nameId == StrId::STR_RESTORE_LIGHT_ON_WAKE || s.nameId == StrId::STR_RIGHT_BTN_LIGHT;
                           }),
            v.end());
  }
  return v;
}
