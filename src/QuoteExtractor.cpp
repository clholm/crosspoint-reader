#include "QuoteExtractor.h"

#include <HardwareSerial.h>
#include <HalStorage.h>
#include <Serialization.h>

namespace {
constexpr char QUOTE_FILE[] = "/.crosspoint/pet_quotes.bin";

// Section file header size (must match Section.cpp HEADER_SIZE)
constexpr uint32_t SECTION_HEADER_SIZE = sizeof(uint8_t) + sizeof(int) + sizeof(float) + sizeof(bool) + sizeof(uint8_t) +
                                         sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(bool) +
                                         sizeof(uint32_t);
}  // namespace

std::string QuoteExtractor::readTextBlockWords(FsFile& file) {
  uint16_t wordCount;
  serialization::readPod(file, wordCount);

  if (wordCount > 10000 || wordCount == 0) {
    return "";
  }

  // Read all words and concatenate
  std::string result;
  for (uint16_t i = 0; i < wordCount; i++) {
    std::string word;
    serialization::readString(file, word);
    if (i > 0) {
      result += ' ';
    }
    result += word;
  }

  // Skip xpos data (uint16_t per word)
  file.seekCur(wordCount * sizeof(uint16_t));
  // Skip style data (uint8_t per word)
  file.seekCur(wordCount * sizeof(uint8_t));
  // Skip block style (uint8_t)
  file.seekCur(sizeof(uint8_t));

  return result;
}

bool QuoteExtractor::extractQuoteFromSection(const std::string& sectionFilePath, const std::string& bookTitle,
                                             CachedQuote& outQuote) {
  FsFile file;
  if (!Storage.openFileForRead("QEX", sectionFilePath, file)) {
    return false;
  }

  // Read header to get page count and LUT offset
  file.seek(SECTION_HEADER_SIZE - sizeof(uint32_t) - sizeof(uint16_t));
  uint16_t pageCount;
  serialization::readPod(file, pageCount);
  uint32_t lutOffset;
  serialization::readPod(file, lutOffset);

  if (pageCount == 0 || lutOffset == 0) {
    file.close();
    return false;
  }

  // Pick a random page (use millis() as a simple seed)
  const uint16_t pageIndex = millis() % pageCount;

  // Seek to LUT entry for chosen page
  file.seek(lutOffset + sizeof(uint32_t) * pageIndex);
  uint32_t pagePos;
  serialization::readPod(file, pagePos);
  file.seek(pagePos);

  // Read page: element count
  uint16_t elementCount;
  serialization::readPod(file, elementCount);

  std::string bestQuote;
  for (uint16_t i = 0; i < elementCount; i++) {
    uint8_t tag;
    serialization::readPod(file, tag);

    if (tag != 1) {  // TAG_PageLine
      break;
    }

    // Skip xPos, yPos (int16_t each)
    file.seekCur(sizeof(int16_t) * 2);

    std::string lineText = readTextBlockWords(file);
    if (lineText.empty()) {
      continue;
    }

    // Accumulate lines into a paragraph-like string
    if (!bestQuote.empty()) {
      bestQuote += ' ';
    }
    bestQuote += lineText;
  }

  file.close();

  // Check quote quality: between 20 and 300 characters
  if (bestQuote.length() < 20 || bestQuote.length() > 300) {
    // Try to trim to a reasonable length if too long
    if (bestQuote.length() > 300) {
      // Find a sentence-ending punctuation near the limit
      size_t cutoff = bestQuote.rfind('.', 280);
      if (cutoff == std::string::npos || cutoff < 20) {
        cutoff = bestQuote.rfind(' ', 280);
      }
      if (cutoff != std::string::npos && cutoff >= 20) {
        bestQuote = bestQuote.substr(0, cutoff + 1);
      } else {
        return false;
      }
    } else {
      return false;
    }
  }

  outQuote.text = bestQuote;
  outQuote.bookTitle = bookTitle;
  Serial.printf("[%lu] [QEX] Extracted quote (%d chars) from %s\n", millis(), bestQuote.length(), bookTitle.c_str());
  return true;
}

bool QuoteExtractor::saveQuotes(const std::vector<CachedQuote>& quotes) {
  FsFile file;
  if (!Storage.openFileForWrite("QEX", QUOTE_FILE, file)) {
    return false;
  }

  const uint8_t version = QUOTE_FILE_VERSION;
  serialization::writePod(file, version);
  const uint8_t count = static_cast<uint8_t>(quotes.size());
  serialization::writePod(file, count);
  for (const auto& q : quotes) {
    serialization::writeString(file, q.text);
    serialization::writeString(file, q.bookTitle);
  }
  file.close();
  return true;
}

bool QuoteExtractor::loadQuotes(std::vector<CachedQuote>& quotes) {
  FsFile file;
  if (!Storage.openFileForRead("QEX", QUOTE_FILE, file)) {
    return false;
  }

  uint8_t version;
  serialization::readPod(file, version);
  if (version > QUOTE_FILE_VERSION) {
    file.close();
    return false;
  }

  uint8_t count;
  serialization::readPod(file, count);
  quotes.clear();
  for (uint8_t i = 0; i < count; i++) {
    CachedQuote q;
    serialization::readString(file, q.text);
    serialization::readString(file, q.bookTitle);
    quotes.push_back(std::move(q));
  }
  file.close();
  return true;
}

bool QuoteExtractor::loadRandomQuote(CachedQuote& quote) {
  std::vector<CachedQuote> quotes;
  if (!loadQuotes(quotes) || quotes.empty()) {
    return false;
  }
  const size_t index = millis() % quotes.size();
  quote = quotes[index];
  return true;
}
