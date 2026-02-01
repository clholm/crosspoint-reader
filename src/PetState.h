#pragma once
#include <cstdint>

class PetState {
  static PetState instance;

 public:
  enum class Mood : uint8_t {
    EGG = 0,        // Never read yet
    VERY_HAPPY = 1,  // Just fed this session
    HAPPY = 2,       // Fed recently (hunger == 0)
    NEUTRAL = 3,     // Hunger == 1
    SAD = 4,         // Hunger == 2
    VERY_SAD = 5,    // Hunger >= 3
  };

  uint32_t bootCount = 0;
  uint32_t lastFedBoot = 0;
  uint32_t totalPagesRead = 0;
  uint16_t sessionPagesRead = 0;
  bool hatched = false;
  bool fedThisSession = false;

  static constexpr uint16_t FEED_THRESHOLD = 10;  // Pages to read to feed pet

  ~PetState() = default;

  static PetState& getInstance() { return instance; }

  bool saveToFile() const;
  bool loadFromFile();

  // Called once per boot after loadFromFile()
  void onBootInit();

  // Called on every page turn in the reader
  void recordPageTurn();

  // Compute current mood based on state
  Mood computeMood() const;

  // Hunger level: boots since last fed
  uint32_t getHunger() const;
};

#define PET_STATE PetState::getInstance()
