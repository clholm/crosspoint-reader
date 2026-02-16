#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <functional>

#include "../Activity.h"
#include "QuoteExtractor.h"

class ReadagotchiActivity final : public Activity {
  TaskHandle_t displayTaskHandle = nullptr;
  SemaphoreHandle_t renderingMutex = nullptr;
  bool updateRequired = false;
  const std::function<void()> onGoBack;
  CachedQuote currentQuote;
  bool hasQuote = false;

  static void taskTrampoline(void* param);
  [[noreturn]] void displayTaskLoop();
  void render();
  void drawSpeechBubble(int x, int y, int width, int maxHeight);
  void drawPetSprite(int centerX, int centerY);
  void drawStats(int y);

#if defined(CROSSPOINT_EMULATED) && CROSSPOINT_EMULATED == 1
  void handleDebugInput();
  void setMoodState(uint8_t targetMood);
  uint8_t debugQuoteIndex = 0;
#endif

 public:
  explicit ReadagotchiActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                              const std::function<void()>& onGoBack)
      : Activity("Readagotchi", renderer, mappedInput), onGoBack(onGoBack) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
};
