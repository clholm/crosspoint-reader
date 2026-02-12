#if defined(CROSSPOINT_EMULATED) && CROSSPOINT_EMULATED == 1

#include <HalGPIO.h>
#include <esp_task_wdt.h>
#include <soc/timer_group_reg.h>

// Button state tracking for UART-based input
static uint8_t currentButtonState = 0;
static uint8_t previousButtonState = 0;
static unsigned long buttonPressStartTime = 0;

void HalGPIO::begin() {
  // Disable watchdog timers for QEMU - the emulated instruction timing
  // is too slow and triggers the 300ms interrupt WDT.
  // The interrupt WDT uses Timer Group 1, and the task WDT uses Timer Group 0.
  // Disable both by writing to the WDT config registers directly.

  // Disable Timer Group 0 WDT (task watchdog)
  WRITE_PERI_REG(TIMG_WDTWPROTECT_REG(0), TIMG_WDT_WKEY_VALUE);
  WRITE_PERI_REG(TIMG_WDTCONFIG0_REG(0), 0);
  WRITE_PERI_REG(TIMG_WDTWPROTECT_REG(0), 0);

  // Disable Timer Group 1 WDT (interrupt watchdog)
  WRITE_PERI_REG(TIMG_WDTWPROTECT_REG(1), TIMG_WDT_WKEY_VALUE);
  WRITE_PERI_REG(TIMG_WDTCONFIG0_REG(1), 0);
  WRITE_PERI_REG(TIMG_WDTWPROTECT_REG(1), 0);

  // Also disable the task WDT via the API if possible
  esp_task_wdt_deinit();

  Serial.begin(115200);
  Serial.println("[EMU] HalGPIO::begin() - using UART input (h/j/k/l=nav, c=confirm, b=back, p=power)");
  Serial.println("[EMU] Watchdog timers disabled for QEMU compatibility");
}

void HalGPIO::update() {
  previousButtonState = currentButtonState;
  currentButtonState = 0;

  while (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 'b': currentButtonState |= (1 << BTN_BACK); break;
      case 'c':
      case '\n': currentButtonState |= (1 << BTN_CONFIRM); break;
      case 'h': currentButtonState |= (1 << BTN_LEFT); break;
      case 'l': currentButtonState |= (1 << BTN_RIGHT); break;
      case 'k': currentButtonState |= (1 << BTN_UP); break;
      case 'j': currentButtonState |= (1 << BTN_DOWN); break;
      case 'p': currentButtonState |= (1 << BTN_POWER); break;
    }
  }

  if (currentButtonState && !previousButtonState) {
    buttonPressStartTime = millis();
  }
}

bool HalGPIO::isPressed(uint8_t buttonIndex) const {
  return currentButtonState & (1 << buttonIndex);
}

bool HalGPIO::wasPressed(uint8_t buttonIndex) const {
  return (currentButtonState & (1 << buttonIndex)) && !(previousButtonState & (1 << buttonIndex));
}

bool HalGPIO::wasAnyPressed() const {
  return currentButtonState && !previousButtonState;
}

bool HalGPIO::wasReleased(uint8_t buttonIndex) const {
  return !(currentButtonState & (1 << buttonIndex)) && (previousButtonState & (1 << buttonIndex));
}

bool HalGPIO::wasAnyReleased() const {
  return !currentButtonState && previousButtonState;
}

unsigned long HalGPIO::getHeldTime() const {
  if (currentButtonState) {
    return millis() - buttonPressStartTime;
  }
  return 0;
}

void HalGPIO::startDeepSleep() {
  Serial.println("[EMU] Deep sleep requested - halting emulator (reset to restart)");
  while (true) {
    delay(1000);
  }
}

int HalGPIO::getBatteryPercentage() const {
  return 75;  // Fake battery level
}

bool HalGPIO::isUsbConnected() const {
  return true;  // Always connected in emulator
}

HalGPIO::WakeupReason HalGPIO::getWakeupReason() const {
  return WakeupReason::AfterFlash;  // Skip power button verification in emulator
}

#endif  // CROSSPOINT_EMULATED
