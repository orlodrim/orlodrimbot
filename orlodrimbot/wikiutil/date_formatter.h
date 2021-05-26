#ifndef WIKIUTIL_DATE_FORMATTER_H
#define WIKIUTIL_DATE_FORMATTER_H

#include <string>
#include <string_view>
#include "cbl/date.h"

namespace wikiutil {

// Converts a cbl::Date to a human-readable string, optionally with wiki markup.
class DateFormatter {
public:
  enum Precision {
    DAY,     // 3 mars 2000
    MINUTE,  // 3 mars 2000 à 04:05
    SECOND,  // 3 mars 2000 à 04:05:06
  };
  enum Format {
    LONG,               // 1 octobre 2000
    LONG_1ST,           // 1er octobre 2000
    LONG_1ST_TEMPLATE,  // {{1er}} octobre 2000
    SHORT,              // 01/10/2000
  };

  // Returns a reference to an internally-owned parser for language `lang`. It remain valid forever.
  // The only existing implementation is for lang = "fr".
  static const DateFormatter& getByLang(std::string_view lang);

  virtual std::string format(const cbl::Date& date, Format format = LONG, Precision precision = DAY) const = 0;
};

}  // namespace wikiutil

#endif
