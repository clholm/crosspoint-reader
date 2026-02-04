#if defined(CROSSPOINT_EMULATED) && CROSSPOINT_EMULATED == 1

#include <SDCardManager.h>

SDCardManager SDCardManager::instance;

SDCardManager::SDCardManager() : sd() {}

bool SDCardManager::begin() {
  initialized = true;
  Serial.printf("[%lu] [SD] SD card detected (emulated stub)\n", millis());
  return true;
}

bool SDCardManager::ready() const {
  return initialized;
}

std::vector<String> SDCardManager::listFiles(const char* path, const int maxFiles) {
  return {};
}

String SDCardManager::readFile(const char* path) {
  return {""};
}

bool SDCardManager::readFileToStream(const char* path, Print& out, const size_t chunkSize) {
  return false;
}

size_t SDCardManager::readFileToBuffer(const char* path, char* buffer, const size_t bufferSize, const size_t maxBytes) {
  if (buffer && bufferSize > 0) buffer[0] = '\0';
  return 0;
}

bool SDCardManager::writeFile(const char* path, const String& content) {
  Serial.printf("[EMU] writeFile stub: %s (%u bytes)\n", path, content.length());
  return false;
}

bool SDCardManager::ensureDirectoryExists(const char* path) {
  return false;
}

bool SDCardManager::openFileForRead(const char* moduleName, const char* path, FsFile& file) {
  return false;
}

bool SDCardManager::openFileForRead(const char* moduleName, const std::string& path, FsFile& file) {
  return false;
}

bool SDCardManager::openFileForRead(const char* moduleName, const String& path, FsFile& file) {
  return false;
}

bool SDCardManager::openFileForWrite(const char* moduleName, const char* path, FsFile& file) {
  return false;
}

bool SDCardManager::openFileForWrite(const char* moduleName, const std::string& path, FsFile& file) {
  return false;
}

bool SDCardManager::openFileForWrite(const char* moduleName, const String& path, FsFile& file) {
  return false;
}

bool SDCardManager::removeDir(const char* path) {
  return false;
}

#endif  // CROSSPOINT_EMULATED
