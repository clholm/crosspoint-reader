#if defined(CROSSPOINT_EMULATED) && CROSSPOINT_EMULATED == 1

#include <HalDisplay.h>

#include <cstring>

// Internal 1-bit framebuffer (same format as EInkDisplay)
static uint8_t emulatedFrameBuffer[HalDisplay::BUFFER_SIZE];

// QEMU virtual RGB panel (Espressif QEMU fork, hw/display/esp_rgb.c)
// Control registers at 0x21000000, dedicated VRAM at 0x20000000
#define QEMU_RGB_REG_BASE    0x21000000
#define QEMU_RGB_VERSION     (*(volatile uint32_t*)(QEMU_RGB_REG_BASE + 0x00))
#define QEMU_RGB_SIZE        (*(volatile uint32_t*)(QEMU_RGB_REG_BASE + 0x04))
#define QEMU_RGB_UPDATE_FROM (*(volatile uint32_t*)(QEMU_RGB_REG_BASE + 0x08))
#define QEMU_RGB_UPDATE_TO   (*(volatile uint32_t*)(QEMU_RGB_REG_BASE + 0x0C))
#define QEMU_RGB_UPDATE_PTR  (*(volatile uint32_t*)(QEMU_RGB_REG_BASE + 0x10))
#define QEMU_RGB_UPDATE_ST   (*(volatile uint32_t*)(QEMU_RGB_REG_BASE + 0x14))
#define QEMU_RGB_BPP         (*(volatile uint32_t*)(QEMU_RGB_REG_BASE + 0x18))
#define QEMU_VRAM_BASE       ((volatile uint16_t*)0x20000000)

// QEMU output display dimensions — 360x600 preserves the device's 3:5 aspect
// ratio (480x800) at 75% uniform scale, fitting within QEMU's 600px max height.
static constexpr uint32_t QEMU_DISPLAY_WIDTH = 360;
static constexpr uint32_t QEMU_DISPLAY_HEIGHT = 600;

// Logical portrait dimensions (what GfxRenderer sees)
static constexpr uint32_t PORTRAIT_WIDTH = HalDisplay::DISPLAY_HEIGHT;   // 480
static constexpr uint32_t PORTRAIT_HEIGHT = HalDisplay::DISPLAY_WIDTH;   // 800

static bool qemuDisplayAvailable = false;

// Convert 1-bit e-ink framebuffer to RGB565 and write to QEMU VRAM.
// The framebuffer is 800x480 with portrait content stored rotated 90° CW
// (GfxRenderer::Portrait maps logical (x,y) -> physical (y, 479-x)).
// We rotate 90° CCW and scale from 480x800 to 360x600 (uniform 75%).
//
// For output pixel (ox, oy) in 360x600:
//   portrait pixel (px, py) = (ox * 480/360, oy * 800/600)
//   framebuffer (fbX, fbY) = (py, 479 - px)
static void blitToQemuDisplay() {
  if (!qemuDisplayAvailable) return;

  uint16_t* vram = (uint16_t*)0x20000000;  // non-volatile for bulk copy
  uint16_t rowBuf[QEMU_DISPLAY_WIDTH];     // 720 bytes on stack

  for (uint32_t oy = 0; oy < QEMU_DISPLAY_HEIGHT; oy++) {
    // Portrait Y coordinate (0..799) from output Y (0..599)
    const uint32_t py = oy * PORTRAIT_HEIGHT / QEMU_DISPLAY_HEIGHT;

    for (uint32_t ox = 0; ox < QEMU_DISPLAY_WIDTH; ox++) {
      // Portrait X coordinate (0..479) from output X (0..359)
      const uint32_t px = ox * PORTRAIT_WIDTH / QEMU_DISPLAY_WIDTH;

      // Inverse portrait rotation to landscape framebuffer
      const uint32_t fbX = py;
      const uint32_t fbY = (HalDisplay::DISPLAY_HEIGHT - 1) - px;

      // Read 1-bit pixel from framebuffer
      const uint32_t byteIndex = fbY * HalDisplay::DISPLAY_WIDTH_BYTES + (fbX / 8);
      const uint8_t bitPosition = 7 - (fbX % 8);
      const uint8_t byte = emulatedFrameBuffer[byteIndex];

      // bit=1 in framebuffer = white/clear, bit=0 = drawn/black
      rowBuf[ox] = (byte & (1 << bitPosition)) ? 0xFFFF : 0x0000;
    }

    memcpy(vram + oy * QEMU_DISPLAY_WIDTH, rowBuf, sizeof(rowBuf));
  }

  // Trigger display update via control registers (these must be volatile)
  QEMU_RGB_UPDATE_FROM = 0;
  QEMU_RGB_UPDATE_TO = ((uint32_t)QEMU_DISPLAY_WIDTH << 16)
                      | QEMU_DISPLAY_HEIGHT;
  QEMU_RGB_UPDATE_PTR = 0x20000000;
  QEMU_RGB_UPDATE_ST = 1;
}

// Enable QEMU display output. Call this only when you know the virtual
// display device is mapped (QEMU launched with -display sdl).
extern "C" void emuEnableQemuDisplay() {
  // Portrait: 480 wide x 600 tall (native width, cropped to QEMU's 600px max height)
  QEMU_RGB_SIZE = ((uint32_t)QEMU_DISPLAY_WIDTH << 16)
                | QEMU_DISPLAY_HEIGHT;
  QEMU_RGB_BPP = 16;  // RGB565
  qemuDisplayAvailable = true;
  Serial.println("[EMU] QEMU virtual RGB display enabled (360x600 portrait, 75% scale)");
  blitToQemuDisplay();
}

HalDisplay::HalDisplay() {}

HalDisplay::~HalDisplay() {}

void HalDisplay::begin() {
  memset(emulatedFrameBuffer, 0xFF, sizeof(emulatedFrameBuffer));

  // Try to detect if QEMU virtual display is available by checking
  // if we can read from the device registers without faulting.
  // The device is only mapped when QEMU is launched with display support.
  // We probe by installing a temporary exception handler.
  //
  // On ESP-IDF/FreeRTOS, we cannot safely modify the trap vector.
  // Instead, we rely on a build flag or runtime command to enable it.
#if defined(QEMU_DISPLAY)
  emuEnableQemuDisplay();
#else
  Serial.println("[EMU] Display initialized (framebuffer in RAM, serial output only)");
  Serial.println("[EMU] To enable QEMU SDL display, rebuild with -DQEMU_DISPLAY=1");
  Serial.println("[EMU]   and launch QEMU with '-display sdl'");
#endif
}

void HalDisplay::clearScreen(uint8_t color) const {
  memset(emulatedFrameBuffer, color, sizeof(emulatedFrameBuffer));
}

void HalDisplay::drawImage(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                           bool fromProgmem) const {
  for (uint16_t row = 0; row < h && (y + row) < DISPLAY_HEIGHT; row++) {
    uint16_t srcOffset = row * (w / 8);
    uint16_t dstOffset = (y + row) * DISPLAY_WIDTH_BYTES + (x / 8);
    uint16_t copyBytes = w / 8;
    if (dstOffset + copyBytes <= BUFFER_SIZE) {
      if (fromProgmem) {
        memcpy_P(emulatedFrameBuffer + dstOffset, imageData + srcOffset, copyBytes);
      } else {
        memcpy(emulatedFrameBuffer + dstOffset, imageData + srcOffset, copyBytes);
      }
    }
  }
}

void HalDisplay::displayBuffer(RefreshMode mode, bool turnOffScreen) {
  Serial.printf("[EMU] displayBuffer (mode=%d)\n", mode);
  blitToQemuDisplay();
}

void HalDisplay::refreshDisplay(RefreshMode mode, bool turnOffScreen) {
  Serial.printf("[EMU] refreshDisplay (mode=%d, off=%d)\n", mode, turnOffScreen);
  blitToQemuDisplay();
}

void HalDisplay::deepSleep() {
  Serial.println("[EMU] Display deep sleep");
}

uint8_t* HalDisplay::getFrameBuffer() const {
  return const_cast<uint8_t*>(emulatedFrameBuffer);
}

void HalDisplay::copyGrayscaleBuffers(const uint8_t* lsbBuffer, const uint8_t* msbBuffer) {
  if (lsbBuffer) {
    memcpy(emulatedFrameBuffer, lsbBuffer, BUFFER_SIZE);
  }
}

void HalDisplay::copyGrayscaleLsbBuffers(const uint8_t* lsbBuffer) {
  if (lsbBuffer) {
    memcpy(emulatedFrameBuffer, lsbBuffer, BUFFER_SIZE);
  }
}

void HalDisplay::copyGrayscaleMsbBuffers(const uint8_t* msbBuffer) {
  // No-op for emulator
}

void HalDisplay::cleanupGrayscaleBuffers(const uint8_t* bwBuffer) {
  if (bwBuffer) {
    memcpy(emulatedFrameBuffer, bwBuffer, BUFFER_SIZE);
  }
}

void HalDisplay::displayGrayBuffer(bool turnOffScreen) {
  Serial.println("[EMU] displayGrayBuffer");
  blitToQemuDisplay();
}

#endif  // CROSSPOINT_EMULATED
