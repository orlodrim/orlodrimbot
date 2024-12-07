#ifndef WIKIUTIL_DATE_PARSER_H
#define WIKIUTIL_DATE_PARSER_H

#include <string_view>
#include "cbl/date.h"

namespace wikiutil {

struct SignatureDate {
  cbl::Date utcDate;
  // Local date as expressed in the text = utcDate + localTimeDiff.
  cbl::DateDiff localTimeDiff;

  bool isNull() const { return utcDate.isNull(); }
  cbl::Date localDate() const { return utcDate + localTimeDiff; }
  bool operator<(const SignatureDate& other) const { return utcDate < other.utcDate; }
};

// Parser for human-readable dates occurring in wikicode, possibly using wiki-specific syntax.
class DateParser {
public:
  enum ParseFlags {
    // When parsing a date without time, set the time to 23:59:59 instead of 00:00:00.
    END_OF_DAY = 1,
    // Only accept dates with year >= 2000.
    AFTER_2000 = 2,
    // Reject dates in the future (with a tolerance of a few hours).
    BEFORE_NOW = 4,
    // Allow dates without a year and guess it based on the current time.
    IMPLICIT_YEAR = 8,
    // Allow dates such as "01/02/2000".
    ALLOW_NUMERIC_MONTH = 0x10,
  };

  // Returns a reference to an internally-owned parser for language `lang`. It remain valid forever.
  // For now, the only supported language is "fr" (French).
  static const DateParser& getByLang(std::string_view lang);

  // Searches for a date (with time) as it normally appears in wiki signatures, and returns the first one.
  // Dates in the future are ignored.
  virtual SignatureDate extractFirstSignatureDate(std::string_view text) const = 0;
  // Searches for a date (with time) as it normally appears in wiki signatures, and returns the highest one.
  // Dates in the future are ignored.
  virtual SignatureDate extractMaxSignatureDate(std::string_view text) const = 0;
  // Try to parse text as a date (without time).
  // May support wiki-specific syntax such as "{{1er}}" for the first day of the month in French.
  // The typical usage is to parse dates in the value of template parameters.
  virtual cbl::Date parseDate(std::string_view text, int flags) const = 0;
  virtual cbl::Date extractFirstDate(std::string_view text, int flags) const = 0;
};

}  // namespace wikiutil

#endif
