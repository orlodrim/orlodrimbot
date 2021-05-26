#include "date_formatter.h"
#include "cbl/date.h"
#include "cbl/log.h"
#include "cbl/unittest.h"

using cbl::Date;

namespace wikiutil {

class DateFormatterTest : public cbl::Test {
private:
  CBL_TEST_CASE(format) {
    const DateFormatter& dateFormatter = DateFormatter::getByLang("fr");

    CBL_ASSERT_EQ(dateFormatter.format(Date()), "");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("2005-01-01T00:00:00Z")), "1 janvier 2005");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("2005-01-01T23:59:59Z")), "1 janvier 2005");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("2005-01-02T00:00:00Z")), "2 janvier 2005");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("2005-01-01T00:00:00Z"), DateFormatter::LONG_1ST),
                  "1er janvier 2005");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("2005-01-01T00:00:00Z"), DateFormatter::LONG_1ST_TEMPLATE),
                  "{{1er}} janvier 2005");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("2005-01-31T00:00:00Z")), "31 janvier 2005");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("2005-01-31T00:00:00Z"), DateFormatter::LONG_1ST),
                  "31 janvier 2005");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("2005-01-31T00:00:00Z"), DateFormatter::LONG_1ST_TEMPLATE),
                  "31 janvier 2005");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("2000-02-29T00:00:00Z")), "29 février 2000");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("0800-01-01T00:00:00Z")), "1 janvier 800");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("0080-01-01T00:00:00Z")), "1 janvier 80");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("2005-01-01T23:59:59Z")), "1 janvier 2005");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("1999-01-01T00:00:00Z"), DateFormatter::SHORT), "01/01/99");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("2005-01-01T00:00:00Z"), DateFormatter::SHORT), "01/01/05");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("2005-01-11T00:00:00Z"), DateFormatter::SHORT), "11/01/05");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("2005-01-12T23:59:59Z"), DateFormatter::SHORT), "12/01/05");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("2005-12-20T23:59:59Z"), DateFormatter::SHORT), "20/12/05");

    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("2010-01-01T00:00:00Z")), "1 janvier 2010");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("2010-02-01T00:00:00Z")), "1 février 2010");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("2010-03-01T00:00:00Z")), "1 mars 2010");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("2010-04-01T00:00:00Z")), "1 avril 2010");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("2010-05-01T00:00:00Z")), "1 mai 2010");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("2010-06-01T00:00:00Z")), "1 juin 2010");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("2010-07-01T00:00:00Z")), "1 juillet 2010");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("2010-08-01T00:00:00Z")), "1 août 2010");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("2010-09-01T00:00:00Z")), "1 septembre 2010");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("2010-10-01T00:00:00Z")), "1 octobre 2010");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("2010-11-01T00:00:00Z")), "1 novembre 2010");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("2010-12-01T00:00:00Z")), "1 décembre 2010");

    CBL_ASSERT_EQ(
        dateFormatter.format(Date::fromISO8601("2010-12-01T20:40:32Z"), DateFormatter::LONG_1ST, DateFormatter::SECOND),
        "1er décembre 2010 à 20:40:32");
    CBL_ASSERT_EQ(dateFormatter.format(Date::fromISO8601("2010-12-01T20:40:32Z"), DateFormatter::LONG_1ST_TEMPLATE,
                                       DateFormatter::MINUTE),
                  "{{1er}} décembre 2010 à 20:40");
    CBL_ASSERT_EQ(
        dateFormatter.format(Date::fromISO8601("2010-12-01T20:40:32Z"), DateFormatter::SHORT, DateFormatter::MINUTE),
        "01/12/10 à 20:40");
    CBL_ASSERT_EQ(
        dateFormatter.format(Date::fromISO8601("0080-01-01T00:00:00Z"), DateFormatter::LONG, DateFormatter::MINUTE),
        "1 janvier 80 à 00:00");
  }
};

}  // namespace wikiutil

int main() {
  wikiutil::DateFormatterTest().run();
  return 0;
}
