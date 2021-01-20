#ifndef BOT_REQUESTS_ARCHIVER_LIB_H
#define BOT_REQUESTS_ARCHIVER_LIB_H

#include <string>
#include "cbl/date.h"
#include "mwclient/wiki.h"

class YearMonth {
public:
  explicit YearMonth(const cbl::Date& date) : m_value(date.year() * 12 + date.month() - 1) {}
  YearMonth operator-(int months) const { return YearMonth(m_value - months); };
  YearMonth operator+(int months) const { return YearMonth(m_value + months); };
  std::string toString() const;

private:
  explicit YearMonth(int value) : m_value(value) {}
  int m_value;
};

class BotRequestsArchiver {
public:
  BotRequestsArchiver(mwc::Wiki& wiki, bool dryRun) : m_wiki(&wiki), m_dryRun(dryRun) {}
  void run(bool forceNewMonth);

private:
  enum class RedirectToArchive {
    NO,
    YES,
    IF_CHANGED,
  };
  void initPage(YearMonth yearMonth);
  void archiveMonth(YearMonth yearMonth, bool archiveAll, RedirectToArchive canRedirectToArchive);

  mwc::Wiki* m_wiki;
  bool m_dryRun = false;
};

#endif
