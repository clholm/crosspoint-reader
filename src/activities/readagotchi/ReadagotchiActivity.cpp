#include "ReadagotchiActivity.h"

#include <GfxRenderer.h>

#include "Battery.h"
#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "PetState.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "images/pet/PetEgg.h"
#include "images/pet/PetHappy.h"
#include "images/pet/PetNeutral.h"
#include "images/pet/PetSad.h"
#include "images/pet/PetVerySad.h"
#include "images/pet/PetVeryHappy.h"

void ReadagotchiActivity::taskTrampoline(void* param) {
  auto* self = static_cast<ReadagotchiActivity*>(param);
  self->displayTaskLoop();
}

void ReadagotchiActivity::onEnter() {
  Activity::onEnter();

  renderingMutex = xSemaphoreCreateMutex();

  // Load a random quote if available
  hasQuote = QuoteExtractor::loadRandomQuote(currentQuote);

#if defined(CROSSPOINT_EMULATED) && CROSSPOINT_EMULATED == 1
  render();
  updateRequired = false;
#else
  updateRequired = true;

  xTaskCreate(&ReadagotchiActivity::taskTrampoline, "ReadagotchiTask",
              4096,               // Stack size
              this,               // Parameters
              1,                  // Priority
              &displayTaskHandle  // Task handle
  );
#endif
}

void ReadagotchiActivity::onExit() {
  Activity::onExit();

#if !defined(CROSSPOINT_EMULATED) || CROSSPOINT_EMULATED == 0
  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  if (displayTaskHandle) {
    vTaskDelete(displayTaskHandle);
    displayTaskHandle = nullptr;
  }
  vSemaphoreDelete(renderingMutex);
  renderingMutex = nullptr;
#endif
}

void ReadagotchiActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoBack();
    return;
  }
}

void ReadagotchiActivity::displayTaskLoop() {
  while (true) {
    if (updateRequired) {
      updateRequired = false;
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      render();
      xSemaphoreGive(renderingMutex);
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void ReadagotchiActivity::drawSpeechBubble(const int x, const int y, const int width, const int maxHeight) {
  if (!hasQuote || currentQuote.text.empty()) {
    return;
  }

  constexpr int padding = 12;
  constexpr int tailHeight = 12;
  const int textAreaWidth = width - padding * 2;
  const int lineHeight = renderer.getLineHeight(SMALL_FONT_ID);

  // Word-wrap the quote text
  std::vector<std::string> lines;
  std::string currentLine;
  std::string word;

  for (size_t i = 0; i <= currentQuote.text.size(); i++) {
    const char c = (i < currentQuote.text.size()) ? currentQuote.text[i] : ' ';
    if (c == ' ' || i == currentQuote.text.size()) {
      if (!word.empty()) {
        std::string testLine = currentLine.empty() ? word : currentLine + " " + word;
        if (renderer.getTextWidth(SMALL_FONT_ID, testLine.c_str()) > textAreaWidth && !currentLine.empty()) {
          lines.push_back(currentLine);
          currentLine = word;
        } else {
          currentLine = testLine;
        }
        word.clear();
      }
    } else {
      word += c;
    }
  }
  if (!currentLine.empty()) {
    lines.push_back(currentLine);
  }

  // Limit to lines that fit in maxHeight
  const int maxLines = (maxHeight - padding * 2 - tailHeight) / lineHeight;
  if (static_cast<int>(lines.size()) > maxLines && maxLines > 0) {
    lines.resize(maxLines);
    if (!lines.empty()) {
      lines.back() += "...";
    }
  }

  if (lines.empty()) {
    return;
  }

  const int bubbleHeight = static_cast<int>(lines.size()) * lineHeight + padding * 2;
  const int bubbleY = y;

  // Draw bubble rectangle (white fill with black border)
  renderer.fillRect(x, bubbleY, width, bubbleHeight, false);
  renderer.drawRect(x, bubbleY, width, bubbleHeight);

  // Draw tail triangle pointing down to pet
  const int tailCenterX = x + width / 2;
  const int tailTopY = bubbleY + bubbleHeight;
  const int xPoints[3] = {tailCenterX - 8, tailCenterX + 8, tailCenterX};
  const int yPoints[3] = {tailTopY, tailTopY, tailTopY + tailHeight};
  // White fill the tail
  renderer.fillPolygon(xPoints, yPoints, 3, false);
  // Draw tail border lines
  renderer.drawLine(xPoints[0], yPoints[0], xPoints[2], yPoints[2]);
  renderer.drawLine(xPoints[1], yPoints[1], xPoints[2], yPoints[2]);
  // Erase the bubble bottom border where the tail connects
  renderer.drawLine(xPoints[0] + 1, tailTopY, xPoints[1] - 1, tailTopY, false);

  // Render quote text inside bubble
  int textY = bubbleY + padding;
  for (const auto& line : lines) {
    // Center each line in the bubble
    const int textWidth = renderer.getTextWidth(SMALL_FONT_ID, line.c_str());
    const int textX = x + (width - textWidth) / 2;
    renderer.drawText(SMALL_FONT_ID, textX, textY, line.c_str());
    textY += lineHeight;
  }

  // Draw source attribution below the bubble in italics
  if (!currentQuote.bookTitle.empty()) {
    const int attrY = bubbleY + bubbleHeight + tailHeight + 2;
    std::string attr = "- " + currentQuote.bookTitle;
    // Truncate if too long
    while (renderer.getTextWidth(SMALL_FONT_ID, attr.c_str()) > width && attr.length() > 10) {
      attr.resize(attr.length() - 4);
      attr += "...";
    }
    const int attrWidth = renderer.getTextWidth(SMALL_FONT_ID, attr.c_str());
    const int attrX = x + (width - attrWidth) / 2;
    renderer.drawText(SMALL_FONT_ID, attrX, attrY, attr.c_str(), true, EpdFontFamily::ITALIC);
  }
}

void ReadagotchiActivity::drawPetSprite(const int centerX, const int centerY) {
  constexpr int spriteSize = 64;
  const int spriteX = centerX - spriteSize / 2;
  const int spriteY = centerY - spriteSize / 2;

  const PetState::Mood mood = PET_STATE.computeMood();

  const uint8_t* sprite = nullptr;
  switch (mood) {
    case PetState::Mood::EGG:
      sprite = PetEgg;
      break;
    case PetState::Mood::VERY_HAPPY:
      sprite = PetVeryHappy;
      break;
    case PetState::Mood::HAPPY:
      sprite = PetHappy;
      break;
    case PetState::Mood::NEUTRAL:
      sprite = PetNeutral;
      break;
    case PetState::Mood::SAD:
      sprite = PetSad;
      break;
    case PetState::Mood::VERY_SAD:
      sprite = PetVerySad;
      break;
  }

  if (sprite) {
    renderer.drawImage(sprite, spriteX, spriteY, spriteSize, spriteSize);
  }
}

void ReadagotchiActivity::drawStats(const int y) {
  const auto pageWidth = renderer.getScreenWidth();
  constexpr int margin = 30;
  const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  int currentY = y;

  // Pet name
  renderer.drawCenteredText(UI_12_FONT_ID, currentY, "Bookworm", true, EpdFontFamily::BOLD);
  currentY += renderer.getLineHeight(UI_12_FONT_ID) + 4;

  // Mood
  const PetState::Mood mood = PET_STATE.computeMood();
  const char* moodText = "Unknown";
  switch (mood) {
    case PetState::Mood::EGG:
      moodText = "Waiting to hatch...";
      break;
    case PetState::Mood::VERY_HAPPY:
      moodText = "Overjoyed!";
      break;
    case PetState::Mood::HAPPY:
      moodText = "Happy";
      break;
    case PetState::Mood::NEUTRAL:
      moodText = "Getting hungry...";
      break;
    case PetState::Mood::SAD:
      moodText = "Hungry and sad";
      break;
    case PetState::Mood::VERY_SAD:
      moodText = "Very hungry and very sad!";
      break;
  }
  renderer.drawCenteredText(UI_10_FONT_ID, currentY, moodText);
  currentY += lineHeight + 8;

  // Divider line
  renderer.drawLine(margin, currentY, pageWidth - margin, currentY);
  currentY += 10;

  // Pages read stat
  char buf[64];
  snprintf(buf, sizeof(buf), "Total pages read: %lu", static_cast<unsigned long>(PET_STATE.totalPagesRead));
  renderer.drawCenteredText(UI_10_FONT_ID, currentY, buf);
  currentY += lineHeight + 4;

  // Hunger status
  const uint32_t hunger = PET_STATE.getHunger();
  if (PET_STATE.fedThisSession) {
    snprintf(buf, sizeof(buf), "Fed today!");
  } else if (hunger == 0) {
    snprintf(buf, sizeof(buf), "Well fed");
  } else if (hunger == 1) {
    snprintf(buf, sizeof(buf), "A bit peckish...");
  } else {
    snprintf(buf, sizeof(buf), "Starving! (%lu sessions unfed)", static_cast<unsigned long>(hunger));
  }
  renderer.drawCenteredText(UI_10_FONT_ID, currentY, buf);
  currentY += lineHeight + 4;

  // Session info
  if (PET_STATE.sessionPagesRead > 0) {
    snprintf(buf, sizeof(buf), "Read %u pages this session", PET_STATE.sessionPagesRead);
    renderer.drawCenteredText(SMALL_FONT_ID, currentY, buf);
  }
}

void ReadagotchiActivity::render() {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();

  // Battery indicator
  const bool showBatteryPercentage =
      SETTINGS.hideBatteryPercentage != CrossPointSettings::HIDE_BATTERY_PERCENTAGE::HIDE_ALWAYS;
  auto metrics = UITheme::getInstance().getMetrics();
  GUI.drawBattery(renderer, Rect{pageWidth - 40, 10, metrics.batteryWidth, metrics.batteryHeight}, showBatteryPercentage);

  // Title
  renderer.drawCenteredText(UI_12_FONT_ID, 15, "My Pet", true, EpdFontFamily::BOLD);

  // Layout depends on whether we have a quote
  int spriteY;
  if (hasQuote && !currentQuote.text.empty()) {
    // Speech bubble at top
    constexpr int bubbleX = 40;
    constexpr int bubbleY = 50;
    const int bubbleWidth = pageWidth - bubbleX * 2;
    constexpr int bubbleMaxHeight = 180;
    drawSpeechBubble(bubbleX, bubbleY, bubbleWidth, bubbleMaxHeight);

    // Pet sprite below the bubble area
    spriteY = 300;
  } else {
    // No quote: pet sprite higher up
    spriteY = 180;
  }

  drawPetSprite(pageWidth / 2, spriteY);

  // Stats below the pet
  drawStats(spriteY + 50);

  // Bottom button hints
  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
