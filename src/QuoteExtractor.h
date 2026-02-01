#pragma once
#include <SdFat.h>

#include <cstdint>
#include <string>
#include <vector>

struct CachedQuote {
  std::string text;
  std::string bookTitle;
};

class QuoteExtractor {
 public:
  // Extract a quote from a cached section file by reading a random page's text blocks.
  // Returns false if no suitable quote found.
  static bool extractQuoteFromSection(const std::string& sectionFilePath, const std::string& bookTitle,
                                      CachedQuote& outQuote);

  // Save/load the rotating quote cache (max 5 quotes)
  static bool saveQuotes(const std::vector<CachedQuote>& quotes);
  static bool loadQuotes(std::vector<CachedQuote>& quotes);
  static bool loadRandomQuote(CachedQuote& quote);

  static constexpr uint8_t MAX_QUOTES = 5;

 private:
  static constexpr uint8_t QUOTE_FILE_VERSION = 1;
  // Extract words from a single serialized TextBlock in the file.
  // Advances the file position past the block.
  static std::string readTextBlockWords(FsFile& file);
};
