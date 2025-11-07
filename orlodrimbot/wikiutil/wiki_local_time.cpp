#include "wiki_local_time.h"
#include "cbl/date.h"
#include <chrono>

using cbl::Date;
using cbl::DateDiff;

namespace wikiutil {

Date getFrWikiLocalTime(Date utcDate) {
  // TODO: Switch to std::chrono after upgrading g++ to version that fully supports it.
  bool summerTime = false;
  if (utcDate.month() == 3) {
    Date lastDayOfMarch(utcDate.year(), 3, 31);
    int firstDayOfSummerTime = 31 - (lastDayOfMarch.dayOfWeek() + 1) % 7;
    summerTime = utcDate.day() > firstDayOfSummerTime || (utcDate.day() == firstDayOfSummerTime && utcDate.hour() >= 1);
  } else if (utcDate.month() == 10) {
    Date lastDayOfOctober(utcDate.year(), 10, 31);
    int firstDayOfWinterTime = 31 - (lastDayOfOctober.dayOfWeek() + 1) % 7;
    summerTime = utcDate.day() < firstDayOfWinterTime || (utcDate.day() == firstDayOfWinterTime && utcDate.hour() == 0);
  } else if (utcDate.month() >= 4 && utcDate.month() <= 9) {
    summerTime = true;
  }
  return utcDate + DateDiff::fromHours(summerTime ? 2 : 1);
}

}  // namespace wikiutil
