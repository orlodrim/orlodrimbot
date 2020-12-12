// Class to store date and time in UTC.
#ifndef CBL_DATE_H
#define CBL_DATE_H

#include <time.h>
#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>

namespace cbl {

// Represents a difference between two dates, with the same granularity as Date (1 second).
class DateDiff {
public:
  explicit DateDiff(int64_t seconds) : m_seconds(seconds) {}
  int64_t seconds() const { return m_seconds; }
  bool operator==(const DateDiff& d) const { return m_seconds == d.m_seconds; }
  bool operator!=(const DateDiff& d) const { return m_seconds != d.m_seconds; }
  bool operator<=(const DateDiff& d) const { return m_seconds <= d.m_seconds; }
  bool operator>=(const DateDiff& d) const { return m_seconds >= d.m_seconds; }
  bool operator<(const DateDiff& d) const { return m_seconds < d.m_seconds; }
  bool operator>(const DateDiff& d) const { return m_seconds > d.m_seconds; }
  DateDiff operator+(const DateDiff& diff) const { return DateDiff(m_seconds + diff.m_seconds); }
  DateDiff operator-(const DateDiff& diff) const { return DateDiff(m_seconds - diff.m_seconds); }

private:
  int64_t m_seconds = 0;
};

// Represents a date including the time of the day. The supported year range is 1-9999 and the granularity is 1 second.
// The value of a default-constructed Date object is the null date, which does not represent a valid date.
// All its members are equal to 0 and it is lower than all other dates.
class Date {
public:
  static Date fromTimeT(time_t t) { return Date(t); }
  static Date fromISO8601(std::string_view s);
  static Date fromISO8601OrEmpty(std::string_view s);

  constexpr Date() = default;
  Date(int y, int mo, int d, int h = 0, int mi = 0, int s = 0) { init(y, mo, d, h, mi, s); }

  int year() const { return m_year; }
  int month() const { return m_month; }
  int day() const { return m_day; }  // Day of month.
  int hour() const { return m_hour; }
  int minute() const { return m_minute; }
  int second() const { return m_second; }

  bool operator==(const Date& d) const { return sortKey() == d.sortKey(); }
  bool operator!=(const Date& d) const { return sortKey() != d.sortKey(); }
  bool operator<=(const Date& d) const { return sortKey() <= d.sortKey(); }
  bool operator>=(const Date& d) const { return sortKey() >= d.sortKey(); }
  bool operator<(const Date& d) const { return sortKey() < d.sortKey(); }
  bool operator>(const Date& d) const { return sortKey() > d.sortKey(); }
  // Not supported for null dates.
  Date operator+(const DateDiff& diff) const;
  Date operator-(const DateDiff& diff) const;
  DateDiff operator-(const Date& d) const;

  // Serializes in ISO8601 format, e.g. "2001-02-03T04:05:06Z".
  // The representation of the null date is unspecified but it can be parsed by fromISO8601OrEmpty.
  std::string toISO8601() const;
  // Returns a Date with the same (year, month, day) and the time of the day equal to midnight.
  Date extractDay() const;
  // Not supported for null dates. Depending on sizeof(time_t), so dates may not be converted correctly.
  time_t toTimeT() const;
  bool isNull() const { return sortKey() == 0; }

  // Returns the current date and time.
  // If Date::setFrozenValueOfNow was called, returns the last value passed to it instead.
  static Date now();

  // Freezes the value returned by Date::now() for testing purposes.
  static void setFrozenValueOfNow(const Date& d);

private:
  explicit Date(time_t t);
  explicit Date(std::string_view s);

  void init(int y, int mo, int d, int h, int mi, int s);
  int64_t sortKey() const;

  int16_t m_year = 0;
  int8_t m_month = 0;
  int8_t m_day = 0;
  int8_t m_hour = 0;
  int8_t m_minute = 0;
  int8_t m_second = 0;

  static Date frozenValueOfNow;
};

inline std::ostream& operator<<(std::ostream& os, const Date& w) {
  return os << w.toISO8601();
}

// Overload for args_parser.
void initFromFlagValue(const std::string& rawValue, Date& date);

}  // namespace cbl

#endif
