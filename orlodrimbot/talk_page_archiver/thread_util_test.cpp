#include "thread_util.h"
#include "cbl/date.h"
#include "cbl/log.h"
#include "cbl/unittest.h"

using cbl::Date;

namespace talk_page_archiver {
namespace {

class ThreadUtilTest : public cbl::Test {
private:
  CBL_TEST_CASE(extractThreadTitle) {
    CBL_ASSERT_EQ(extractThreadTitle("== Some section =="), "== Some section ==");
    CBL_ASSERT_EQ(extractThreadTitle("== Some section ==\nSome content"), "== Some section ==");
  }
  CBL_TEST_CASE(computeDateInTitle) {
    CBL_ASSERT_EQ(computeDateInTitle("== 2 mars 2000 ==", false), Date::fromISO8601("2000-03-02T00:00:00Z"));
    CBL_ASSERT_EQ(computeDateInTitle("== 2 mars 2000 ==", true), Date::fromISO8601("2000-03-02T00:00:00Z"));
    CBL_ASSERT_EQ(computeDateInTitle("== mars 2000 ==", false), Date::fromISO8601("2000-03-01T00:00:00Z"));
    CBL_ASSERT_EQ(computeDateInTitle("== mars 2000 ==", true), Date::fromISO8601("2000-03-31T00:00:00Z"));
    CBL_ASSERT_EQ(computeDateInTitle("== 2000 ==", false), Date::fromISO8601("2000-01-01T00:00:00Z"));
    CBL_ASSERT_EQ(computeDateInTitle("== 2000 ==", true), Date::fromISO8601("2000-12-31T00:00:00Z"));

    Date::setFrozenValueOfNow(Date::fromISO8601("2005-01-01T00:00:00Z"));
    CBL_ASSERT_EQ(computeDateInTitle("== 2 novembre ==", false), Date::fromISO8601("2004-11-02T00:00:00Z"));
    CBL_ASSERT_EQ(computeDateInTitle("== 2 mars ==", false), Date::fromISO8601("2005-03-02T00:00:00Z"));

    // There is some support for extracting the date when the title contains something else, but this only works with
    // the "day month year" format.
    CBL_ASSERT_EQ(computeDateInTitle("== prefix, 2 mars 2000, suffix ==", false),
                  Date::fromISO8601("2000-03-02T00:00:00Z"));

    CBL_ASSERT_EQ(computeDateInTitle("== 2 mars 2000 ==\n1 mars 2000\n3 mars 2000", false),
                  Date::fromISO8601("2000-03-02T00:00:00Z"));

    CBL_ASSERT_EQ(computeDateInTitle("== No date here ==", false), Date());
    CBL_ASSERT_EQ(computeDateInTitle("== 32 mars 2000 ==", false), Date());
  }
};

}  // namespace
}  // namespace talk_page_archiver

int main() {
  talk_page_archiver::ThreadUtilTest().run();
  return 0;
}
