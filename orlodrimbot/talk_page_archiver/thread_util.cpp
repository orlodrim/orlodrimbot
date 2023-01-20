#include "thread_util.h"
#include <string>
#include <string_view>
#include "cbl/date.h"
#include "cbl/string.h"
#include "mwclient/parser.h"
#include "orlodrimbot/wikiutil/date_parser.h"

using cbl::Date;
using cbl::DateDiff;
using std::string;
using std::string_view;
using wikiutil::DateParser;

namespace talk_page_archiver {
namespace {

Date nextMonth(const Date& date) {
  int year = date.year();
  int month = date.month();
  month++;
  if (month > 12) {
    month = 1;
    year++;
  }
  return Date(year, month, 1);
}

}  // namespace

string_view extractThreadTitle(string_view text) {
  size_t eol = text.find('\n');
  return eol != string::npos ? text.substr(0, eol) : text;
}

Date computeDateInTitle(string_view text, bool maxForMissingFields) {
  const DateParser& dateParser = DateParser::getByLang("fr");
  string threadTitle = wikicode::getTitleContent(extractThreadTitle(text));
  Date dateTitle = dateParser.extractFirstDate(threadTitle, DateParser::AFTER_2000 | DateParser::IMPLICIT_YEAR);
  if (dateTitle.isNull()) {
    dateTitle = dateParser.extractFirstDate("1 " + threadTitle, DateParser::AFTER_2000);
    if (!dateTitle.isNull() & maxForMissingFields) {
      dateTitle = nextMonth(dateTitle) - DateDiff::fromDays(1);
    }
  }
  if (dateTitle.isNull()) {
    int year = cbl::parseIntInRange(threadTitle, 2000, 9999, 0);
    if (year != 0) {
      dateTitle = maxForMissingFields ? Date(year, 12, 31) : Date(year, 1, 1);
    }
  }
  return dateTitle;
}

}  // namespace talk_page_archiver
