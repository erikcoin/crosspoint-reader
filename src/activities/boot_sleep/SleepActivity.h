#pragma once

#include "activities/Activity.h"

// White page with an icon and a short text, drawn once before deep sleep.
// The text comes from the "Sleep Screen Text" setting. The icon is the built-in
// moon, or /sleep-icon.png or /sleep-icon.bmp from the SD card when present.
class SleepActivity final : public Activity {
 public:
  explicit SleepActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bool fromTimeout = false)
      : Activity("Sleep", renderer, mappedInput), fromTimeout(fromTimeout) {}
  void onEnter() override;

 private:
  // Draws the SD card icon centered at (cx, top) and returns its height, or 0 if there is none.
  int drawCustomIcon(int cx, int top, int maxSize) const;

  bool fromTimeout = false;
};
