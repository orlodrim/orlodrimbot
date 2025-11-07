#include "date.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <string_view>
#include "error.h"
#include "log.h"

using std::string;
using std::string_view;

namespace cbl {

constexpr string_view ISO8601_PATTERN = "####-##-##T##:##:##Z";
constexpr int EXTRA_SNPRINTF_MARGIN = 20;

Date Date::frozenValueOfNow;

Date::Date(time_t t) {
  tm d = *gmtime(&t);
  init(d.tm_year + 1900, d.tm_mon + 1, d.tm_mday, d.tm_hour, d.tm_min, d.tm_sec);
}

Date::Date(string_view t) {
  char buffer[] = "XXXX XX XX XX XX XX";
  int i = 0;
  for (char c : t) {
    if (c < '0' || c > '9') continue;
    for (; buffer[i] == ' '; i++) {}
    if (!buffer[i]) return;
    buffer[i] = c;
    i++;
  }
  if (buffer[i]) return;
  init(atoi(buffer), atoi(buffer + 5), atoi(buffer + 8), atoi(buffer + 11), atoi(buffer + 14), atoi(buffer + 17));
}

string Date::toISO8601() const {
  char buffer[21 + EXTRA_SNPRINTF_MARGIN];
  snprintf(buffer, sizeof(buffer), "%04i-%02i-%02iT%02i:%02i:%02iZ", static_cast<int>(m_year),
           static_cast<int>(m_month), static_cast<int>(m_day), static_cast<int>(m_hour), static_cast<int>(m_minute),
           static_cast<int>(m_second));
  return buffer;
}

Date Date::extractDay() const {
  return Date(m_year, m_month, m_day);
}

time_t Date::toTimeT() const {
  tm d;
  d.tm_year = year() - 1900;
  d.tm_mon = month() - 1;
  d.tm_mday = day();
  d.tm_hour = hour();
  d.tm_min = minute();
  d.tm_sec = second();
  return timegm(&d);
}

void Date::init(int y, int mo, int d, int h, int mi, int s) {
  if (y >= 0 && y <= 9999 && mo >= 0 && mo <= 12 && d >= 0 && d <= 99 && h >= 0 && h <= 99 && mi >= 0 && mi <= 99 &&
      s >= 0 && s <= 99) {
    m_year = static_cast<int16_t>(y);
    m_month = static_cast<int8_t>(mo);
    m_day = static_cast<int8_t>(d);
    m_hour = static_cast<int8_t>(h);
    m_minute = static_cast<int8_t>(mi);
    m_second = static_cast<int8_t>(s);
  } else {
    m_year = 0;
    m_month = 0;
    m_day = 0;
    m_hour = 0;
    m_minute = 0;
    m_second = 0;
  }
}

int Date::dayOfWeek() const {
  time_t daysSince1970 = toTimeT() / 86400;
  // January 1, 1970 was a Thursday (day 3).
  return static_cast<int>((daysSince1970 + 3) % 7);
}

int64_t Date::sortKey() const {
  return static_cast<int64_t>(m_second) + (static_cast<int64_t>(m_minute) << 8) + (static_cast<int64_t>(m_hour) << 16) +
         (static_cast<int64_t>(m_day) << 24) + (static_cast<int64_t>(m_month) << 32) +
         (static_cast<int64_t>(m_year) << 40);
}

Date Date::operator+(const DateDiff& diff) const {
  return Date(static_cast<time_t>(toTimeT() + diff.seconds()));
}

Date Date::operator-(const DateDiff& diff) const {
  return Date(static_cast<time_t>(toTimeT() - diff.seconds()));
}

void Date::operator+=(const DateDiff& diff) {
  *this = Date(static_cast<time_t>(toTimeT() + diff.seconds()));
}

void Date::operator-=(const DateDiff& diff) {
  *this = Date(static_cast<time_t>(toTimeT() - diff.seconds()));
}

DateDiff Date::operator-(const Date& d) const {
  return DateDiff::fromSeconds(toTimeT() - d.toTimeT());
}

Date Date::now() {
  return frozenValueOfNow.isNull() ? Date(time(nullptr)) : frozenValueOfNow;
}

void Date::setFrozenValueOfNow(const Date& d) {
  CBL_ASSERT(!d.isNull());
  frozenValueOfNow = d;
}

void Date::advanceFrozenClock(const DateDiff& dateDiff) {
  CBL_ASSERT(!frozenValueOfNow.isNull());
  frozenValueOfNow += dateDiff;
}

Date Date::fromISO8601(string_view s) {
  if (s.size() != ISO8601_PATTERN.size()) {
    throw ParseError("Invalid ISO8601 date '" + string(s) + "'");
  }
  const char* patternChar = ISO8601_PATTERN.data();
  for (char c : s) {
    if (!((*patternChar == '#' && c >= '0' && c <= '9') || (*patternChar != '#' && c == *patternChar))) {
      throw ParseError("Invalid ISO8601 date '" + string(s) + "'");
    }
    patternChar++;
  }
  return Date(s);
}

Date Date::fromISO8601OrEmpty(string_view s) {
  return s.empty() ? Date() : Date::fromISO8601(s);
}

void initFromFlagValue(const string& rawValue, Date& date) {
  date = Date::fromISO8601OrEmpty(rawValue);
  if (date.isNull()) {
    throw ParseError("Cannot flag value '" + rawValue + "' as a date");
  }
}

}  // namespace cbl
