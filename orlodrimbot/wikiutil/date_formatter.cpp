#include "date_formatter.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include "cbl/date.h"

using cbl::Date;
using std::string;
using std::string_view;

namespace wikiutil {
namespace {

constexpr int EXTRA_SNPRINTF_MARGIN = 20;

const char* const FR_MONTHS[12] = {
    "janvier", "février", "mars",      "avril",   "mai",      "juin",
    "juillet", "août",    "septembre", "octobre", "novembre", "décembre",
};

class FrenchDateFormatter : public DateFormatter {
public:
  string format(const Date& date, Format format, Precision precision) const override;
  string getMonthName(int month) const override;
};

string FrenchDateFormatter::format(const Date& date, Format format, Precision precision) const {
  string result;
  if (!date.isNull()) {
    char dateBuffer[23 + EXTRA_SNPRINTF_MARGIN];  // strlen("{{1er}} décembre 2000") = 22
    if (format == SHORT) {
      snprintf(dateBuffer, sizeof(dateBuffer), "%02i/%02i/%02i", date.day(), date.month(), date.year() % 100);
    } else {
      const char* dayPrefix = "";
      const char* daySuffix = "";
      if (date.day() == 1) {
        if (format == LONG_1ST) {
          daySuffix = "er";
        } else if (format == LONG_1ST_TEMPLATE) {
          dayPrefix = "{{";
          daySuffix = "er}}";
        }
      }
      int month = std::min(std::max(date.month(), 1), 12);
      snprintf(dateBuffer, sizeof(dateBuffer), "%s%i%s %s %i", dayPrefix, date.day(), daySuffix, FR_MONTHS[month - 1],
               date.year());
    }

    char timeBuffer[9 + EXTRA_SNPRINTF_MARGIN];  // strlen(" à 00:00:00") = 12
    switch (precision) {
      case DAY:
        timeBuffer[0] = '\0';
        break;
      case MINUTE:
        snprintf(timeBuffer, sizeof(timeBuffer), " à %02i:%02i", date.hour(), date.minute());
        break;
      case SECOND:
        snprintf(timeBuffer, sizeof(timeBuffer), " à %02i:%02i:%02i", date.hour(), date.minute(), date.second());
        break;
    }

    result.reserve(strlen(dateBuffer) + strlen(timeBuffer));
    result += dateBuffer;
    result += timeBuffer;
  }
  return result;
}

string FrenchDateFormatter::getMonthName(int month) const {
  if (month < 1 || month > 12) {
    throw std::invalid_argument("Invalid month: " + std::to_string(month));
  }
  return FR_MONTHS[month - 1];
};

}  // namespace

const DateFormatter& DateFormatter::getByLang(string_view lang) {
  if (lang == "fr") {
    static const FrenchDateFormatter frenchDateFormatter;
    return frenchDateFormatter;
  }
  throw std::invalid_argument("Unsupported lang passed to DateFormatter::getByLang");
}

}  // namespace wikiutil
