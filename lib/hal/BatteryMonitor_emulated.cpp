#if defined(CROSSPOINT_EMULATED) && CROSSPOINT_EMULATED == 1

#include "BatteryMonitor.h"

BatteryMonitor::BatteryMonitor(uint8_t adcPin, float dividerMultiplier)
  : _adcPin(adcPin), _dividerMultiplier(dividerMultiplier)
{
}

uint16_t BatteryMonitor::readPercentage() const {
  return 75;  // Fake 75% battery
}

uint16_t BatteryMonitor::readMillivolts() const {
  return 3800;  // ~75% LiPo voltage
}

uint16_t BatteryMonitor::readRawMillivolts() const {
  return 1900;  // Half of 3800 (voltage divider)
}

double BatteryMonitor::readVolts() const {
  return 3.8;
}

uint16_t BatteryMonitor::percentageFromMillivolts(uint16_t millivolts) {
  double volts = millivolts / 1000.0;
  double y = -144.9390 * volts * volts * volts +
             1655.8629 * volts * volts -
             6158.8520 * volts +
             7501.3202;
  if (y < 0.0) y = 0.0;
  if (y > 100.0) y = 100.0;
  return static_cast<uint16_t>(y + 0.5);
}

uint16_t BatteryMonitor::millivoltsFromRawAdc(uint16_t adc_raw) {
  return adc_raw;  // No calibration in emulated mode
}

#endif  // CROSSPOINT_EMULATED
