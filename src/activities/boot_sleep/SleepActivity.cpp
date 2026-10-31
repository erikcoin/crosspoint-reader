#include "SleepActivity.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <HalStorage.h>
#include <Logging.h>
#include <PngToFramebufferConverter.h>

#include <algorithm>
#include <string>
#include <vector>

#include "CrossPointSettings.h"
#include "fontIds.h"
#include "images/MoonIcon.h"

namespace {
// The Metalio panel needs its full waveform for a clean white page.
#if FREEINK_DEVICE_METALIO_EINK4
constexpr auto kSleepClean = HalDisplay::FULL_REFRESH;
#else
constexpr auto kSleepClean = HalDisplay::HALF_REFRESH;
#endif

constexpr char ICON_PNG[] = "/sleep-icon.png";
constexpr char ICON_BMP[] = "/sleep-icon.bmp";
constexpr int ICON_MAX_SIZE = 128;
constexpr int ICON_TEXT_GAP = 24;
constexpr int SIDE_MARGIN = 24;
constexpr int MAX_TEXT_LINES = 3;

int scaledSize(const int size, const int w, const int h, const int maxSize) {
  if (w <= 0 || h <= 0) return 0;
  const float scale = std::min(1.0f, std::min(static_cast<float>(maxSize) / w, static_cast<float>(maxSize) / h));
  return std::max(1, static_cast<int>(size * scale));
}
}  // namespace

int SleepActivity::drawCustomIcon(const int cx, const int top, const int maxSize) const {
  if (Storage.exists(ICON_PNG)) {
    ImageDimensions dims;
    if (PngToFramebufferConverter::getDimensionsStatic(ICON_PNG, dims) && dims.width > 0 && dims.height > 0) {
      const int w = scaledSize(dims.width, dims.width, dims.height, maxSize);
      const int h = scaledSize(dims.height, dims.width, dims.height, maxSize);
      RenderConfig config;
      config.x = cx - w / 2;
      config.y = top;
      config.maxWidth = maxSize;
      config.maxHeight = maxSize;
      config.useDithering = false;
      config.preserveAlpha = true;  // transparent PNGs (like MDI icons) keep the white page
      PngToFramebufferConverter converter;
      if (converter.decodeToFramebuffer(ICON_PNG, renderer, config)) return h;
      LOG_ERR("SLP", "Cannot draw %s", ICON_PNG);
    }
  }

  HalFile file;
  if (Storage.openFileForRead("SLP", ICON_BMP, file)) {
    Bitmap bitmap(file);
    int drawn = 0;
    if (bitmap.parseHeaders() == BmpReaderError::Ok) {
      const int w = scaledSize(bitmap.getWidth(), bitmap.getWidth(), bitmap.getHeight(), maxSize);
      const int h = scaledSize(bitmap.getHeight(), bitmap.getWidth(), bitmap.getHeight(), maxSize);
      if (renderer.drawBitmap(bitmap, cx - w / 2, top, maxSize, maxSize)) drawn = h;
    } else {
      LOG_ERR("SLP", "Invalid %s", ICON_BMP);
    }
    file.close();
    return drawn;
  }
  return 0;
}

void SleepActivity::onEnter() {
  Activity::onEnter();

  // Always a normal-polarity white page, even if night mode was active.
  display.setInverted(false);

  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int fontId = UI_12_FONT_ID;
  const int lineHeight = renderer.getLineHeight(fontId);

  std::vector<std::string> lines;
  if (SETTINGS.sleepText[0] != '\0') {
    lines = renderer.wrappedText(fontId, SETTINGS.sleepText, pageWidth - 2 * SIDE_MARGIN, MAX_TEXT_LINES);
  }
  const int textHeight = static_cast<int>(lines.size()) * lineHeight;

  // Center icon plus text as one group. The icon height is only known after
  // drawing a custom one, so reserve the built-in height for the layout.
  const int groupHeight = MOONICON_HEIGHT + (textHeight > 0 ? ICON_TEXT_GAP + textHeight : 0);
  const int top = std::max(0, (pageHeight - groupHeight) / 2);

  renderer.clearScreen();
  int iconHeight = drawCustomIcon(pageWidth / 2, top, ICON_MAX_SIZE);
  if (iconHeight == 0) {
    renderer.drawImage(MoonIcon, (pageWidth - MOONICON_WIDTH) / 2, top, MOONICON_WIDTH, MOONICON_HEIGHT);
    iconHeight = MOONICON_HEIGHT;
  }

  int y = top + iconHeight + ICON_TEXT_GAP;
  for (const auto& line : lines) {
    renderer.drawCenteredText(fontId, y, line.c_str());
    y += lineHeight;
  }

  renderer.displayBuffer(kSleepClean);
}
