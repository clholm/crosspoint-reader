#include "PetState.h"

#include <HardwareSerial.h>
#include <SDCardManager.h>
#include <Serialization.h>

namespace {
constexpr uint8_t PET_FILE_VERSION = 1;
constexpr char PET_FILE[] = "/.crosspoint/pet.bin";
}  // namespace

PetState PetState::instance;

bool PetState::saveToFile() const {
  FsFile outputFile;
  if (!SdMan.openFileForWrite("PET", PET_FILE, outputFile)) {
    return false;
  }

  serialization::writePod(outputFile, PET_FILE_VERSION);
  serialization::writePod(outputFile, bootCount);
  serialization::writePod(outputFile, lastFedBoot);
  serialization::writePod(outputFile, totalPagesRead);
  serialization::writePod(outputFile, hatched);
  outputFile.close();
  return true;
}

bool PetState::loadFromFile() {
  FsFile inputFile;
  if (!SdMan.openFileForRead("PET", PET_FILE, inputFile)) {
    return false;
  }

  uint8_t version;
  serialization::readPod(inputFile, version);
  if (version > PET_FILE_VERSION) {
    Serial.printf("[%lu] [PET] Deserialization failed: Unknown version %u\n", millis(), version);
    inputFile.close();
    return false;
  }

  serialization::readPod(inputFile, bootCount);
  serialization::readPod(inputFile, lastFedBoot);
  serialization::readPod(inputFile, totalPagesRead);
  serialization::readPod(inputFile, hatched);
  inputFile.close();
  return true;
}

void PetState::onBootInit() {
  bootCount++;
  sessionPagesRead = 0;
  fedThisSession = false;
  saveToFile();
  Serial.printf("[%lu] [PET] Boot #%u, hunger=%u, hatched=%d\n", millis(), bootCount, getHunger(), hatched);
}

void PetState::recordPageTurn() {
  sessionPagesRead++;
  totalPagesRead++;

  if (!hatched && sessionPagesRead >= 1) {
    hatched = true;
  }

  if (sessionPagesRead >= FEED_THRESHOLD && !fedThisSession) {
    fedThisSession = true;
    lastFedBoot = bootCount;
    saveToFile();
    Serial.printf("[%lu] [PET] Fed! totalPages=%u\n", millis(), totalPagesRead);
  }

  // Periodic save every 50 pages to avoid excessive SD writes
  if (sessionPagesRead % 50 == 0) {
    saveToFile();
  }
}

PetState::Mood PetState::computeMood() const {
  if (!hatched) {
    return Mood::EGG;
  }
  if (fedThisSession) {
    return Mood::VERY_HAPPY;
  }
  uint32_t hunger = getHunger();
  if (hunger == 0) {
    return Mood::HAPPY;
  }
  if (hunger == 1) {
    return Mood::NEUTRAL;
  }
  if (hunger == 2) {
    return Mood::SAD;
  }
  return Mood::VERY_SAD;
}

uint32_t PetState::getHunger() const {
  if (bootCount <= lastFedBoot) {
    return 0;
  }
  return bootCount - lastFedBoot;
}
