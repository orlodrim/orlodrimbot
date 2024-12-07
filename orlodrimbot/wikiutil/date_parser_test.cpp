#include "date_parser.h"
#include <string>
#include <string_view>
#include "cbl/date.h"
#include "cbl/log.h"
#include "cbl/unittest.h"

using cbl::Date;
using std::string;
using std::string_view;

namespace wikiutil {

static string debugStringOfSigDate(const SignatureDate& date) {
  string result;
  if (date.isNull()) {
    result = "none";
  } else {
    result = date.utcDate.toISO8601();
    if (date.localTimeDiff.seconds() != 0) {
      result += "|" + std::to_string(date.localTimeDiff.seconds());
    }
  }
  return result;
}

static string debugStringOfDate(const Date& date) {
  return date.isNull() ? "none" : date.toISO8601();
}

class FrenchDateParserTest : public cbl::Test {
public:
  FrenchDateParserTest() : m_dateParser(&DateParser::getByLang("fr")) {}

private:
  void setUp() { Date::setFrozenValueOfNow(Date::fromISO8601("2010-01-01T00:00:00Z")); }

  int getMonthIndexForTest(const char* monthName) const {
    Date date = m_dateParser->parseDate(string("1 ") + monthName + " 2000", 0);
    return !date.isNull() ? date.month() : 0;
  }
  CBL_TEST_CASE(getMonthIndexForTest) {
    CBL_ASSERT_EQ(getMonthIndexForTest("janvier"), 1);
    CBL_ASSERT_EQ(getMonthIndexForTest("février"), 2);
    CBL_ASSERT_EQ(getMonthIndexForTest("mars"), 3);
    CBL_ASSERT_EQ(getMonthIndexForTest("avril"), 4);
    CBL_ASSERT_EQ(getMonthIndexForTest("mai"), 5);
    CBL_ASSERT_EQ(getMonthIndexForTest("juin"), 6);
    CBL_ASSERT_EQ(getMonthIndexForTest("juillet"), 7);
    CBL_ASSERT_EQ(getMonthIndexForTest("août"), 8);
    CBL_ASSERT_EQ(getMonthIndexForTest("septembre"), 9);
    CBL_ASSERT_EQ(getMonthIndexForTest("octobre"), 10);
    CBL_ASSERT_EQ(getMonthIndexForTest("novembre"), 11);
    CBL_ASSERT_EQ(getMonthIndexForTest("décembre"), 12);

    // Case variants.
    CBL_ASSERT_EQ(getMonthIndexForTest("Janvier"), 1);
    CBL_ASSERT_EQ(getMonthIndexForTest("JANVIER"), 1);
    CBL_ASSERT_EQ(getMonthIndexForTest("FÉVRIER"), 2);
    CBL_ASSERT_EQ(getMonthIndexForTest("AOÛT"), 8);

    // Spelling variant for August.
    CBL_ASSERT_EQ(getMonthIndexForTest("aout"), 8);
    CBL_ASSERT_EQ(getMonthIndexForTest("AOUT"), 8);

    // Invalid month
    CBL_ASSERT_EQ(getMonthIndexForTest("octembre"), 0);
    CBL_ASSERT_EQ(getMonthIndexForTest("fevrier"), 0);
  }

  string extractFirstSignatureDateAsStr(string_view text) const {
    return debugStringOfSigDate(m_dateParser->extractFirstSignatureDate(text));
  }
  CBL_TEST_CASE(extractFirstSignatureDate) {
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("1 février 2003 à 4:56 (CET)"), "2003-02-01T03:56:00Z|3600");

    // Time zone
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("1 février 2003 à 4:56 (CEST)"), "2003-02-01T02:56:00Z|7200");
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("1 février 2003 à 4:56"), "2003-02-01T04:56:00Z");

    // Date extraction from a longer piece of text.
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("Some text. 1 février 2010 à 4:56 (CET)\n"  // In the future
                                                 "Some text. 1 février 2003 à 4:56 (CET)\n"
                                                 "Some text. 1 mars 2003 à 4:56 (CET)\n"
                                                 "Some text. 1 janvier 2003 à 4:56 (CET)\n"),
                  "2003-02-01T03:56:00Z|3600");
    // Restore lexer state between attempts.
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("1 février 2003 à 1 mars 2003 à 4:56 (CET)"),
                  "2003-03-01T03:56:00Z|3600");

    // Missing space between components.
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("1février 2003 à 4:56 (CET)"), "none");
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("1 février2003 à 4:56 (CET)"), "none");

    // Year out of range.
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("1 février 20003 à 4:56 (CET)"), "none");
    // Bad month.
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("1 octembre 2003 à 4:56 (CET)"), "none");
    // Bad day.
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("0 février 2003 à 4:56 (CET)"), "none");
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("31 avril 2003 à 4:56 (CET)"), "none");
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("32 janvier 2003 à 4:56 (CET)"), "none");
    // Bad time.
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("1 février 2003 à 23:59 (CET)"), "2003-02-01T22:59:00Z|3600");
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("1 février 2003 à 24:00 (CET)"), "none");
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("1 février 2003 à 4:59 (CET)"), "2003-02-01T03:59:00Z|3600");
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("1 février 2003 à 4:60 (CET)"), "none");

    // 1er is not parsed in this context.
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("1er février 2003 à 4:56 (CET)"), "none");

    // Dates before 2000 are not parsed.
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("1 février 1999 à 4:56 (CET)"), "none");

    // Dates in the future are not parsed.
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("1 février 2010 à 4:56 (CET)"), "none");

    // Should not SEGFAULT.
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr(""), "none");
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("1"), "none");
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("1 février"), "none");
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("1 février 2003"), "none");
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("1 février 2003 à"), "none");
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("1 février 2003 à 4"), "none");
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("1 février 2003 à 4:"), "none");
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("1 février 2003 à 4:56 ("), "2003-02-01T04:56:00Z");
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("1 février 2003 à 4:56 (CET"), "2003-02-01T04:56:00Z");
  }
  CBL_TEST_CASE(extractFirstSignatureDateFebruary29) {
    Date::setFrozenValueOfNow(Date::fromISO8601("2500-01-01T00:00:00Z"));
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("29 février 2000 à 4:56 (CET)"), "2000-02-29T03:56:00Z|3600");
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("29 février 2001 à 4:56 (CET)"), "none");
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("29 février 2004 à 4:56 (CET)"), "2004-02-29T03:56:00Z|3600");
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("29 février 2100 à 4:56 (CET)"), "none");
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("29 février 2101 à 4:56 (CET)"), "none");
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("29 février 2104 à 4:56 (CET)"), "2104-02-29T03:56:00Z|3600");
    CBL_ASSERT_EQ(extractFirstSignatureDateAsStr("29 février 2400 à 4:56 (CET)"), "2400-02-29T03:56:00Z|3600");
  }

  string extractMaxSignatureDateAsStr(string_view text) const {
    return debugStringOfSigDate(m_dateParser->extractMaxSignatureDate(text));
  }
  CBL_TEST_CASE(extractMaxSignatureDate) {
    CBL_ASSERT_EQ(extractMaxSignatureDateAsStr("1 février 2003 à 4:56 (CET)"), "2003-02-01T03:56:00Z|3600");
    // Max date extraction from a longer piece of text.
    CBL_ASSERT_EQ(extractMaxSignatureDateAsStr("Some text. 1 février 2010 à 4:56 (CET)\n"  // In the future
                                               "Some text. 1 février 2003 à 4:56 (CET)\n"
                                               "Some text. 1 mars 2003 à 4:56 (CET)\n"
                                               "Some text. 1 janvier 2003 à 4:56 (CET)\n"),
                  "2003-03-01T03:56:00Z|3600");
  }

  string parseDateAsStr(string_view text, int flags) const {
    return debugStringOfDate(m_dateParser->parseDate(text, flags));
  }
  CBL_TEST_CASE(parseDate) {
    CBL_ASSERT_EQ(parseDateAsStr("1 février 2003", 0), "2003-02-01T00:00:00Z");

    // Variants of 1
    CBL_ASSERT_EQ(parseDateAsStr("1er février 2003", 0), "2003-02-01T00:00:00Z");
    CBL_ASSERT_EQ(parseDateAsStr("1{{er}} février 2003", 0), "2003-02-01T00:00:00Z");
    CBL_ASSERT_EQ(parseDateAsStr("1{{Er}} février 2003", 0), "2003-02-01T00:00:00Z");
    CBL_ASSERT_EQ(parseDateAsStr("{{1er}} février 2003", 0), "2003-02-01T00:00:00Z");
    CBL_ASSERT_EQ(parseDateAsStr("1ER FÉVRIER 2003", 0), "2003-02-01T00:00:00Z");

    // No content allowed before or after the date, except space.
    CBL_ASSERT_EQ(parseDateAsStr(" 1 février 2003", 0), "2003-02-01T00:00:00Z");
    CBL_ASSERT_EQ(parseDateAsStr("1 février 2003 ", 0), "2003-02-01T00:00:00Z");
    CBL_ASSERT_EQ(parseDateAsStr("abc 1 février 2003", 0), "none");
    CBL_ASSERT_EQ(parseDateAsStr("1 février 2003 abc", 0), "none");

    // Dates before 2000 and after the present are allowed by default.
    CBL_ASSERT_EQ(parseDateAsStr("1 février 1", 0), "0001-02-01T00:00:00Z");
    CBL_ASSERT_EQ(parseDateAsStr("1 février 10", 0), "0010-02-01T00:00:00Z");
    CBL_ASSERT_EQ(parseDateAsStr("1 février 100", 0), "0100-02-01T00:00:00Z");
    CBL_ASSERT_EQ(parseDateAsStr("1 février 1000", 0), "1000-02-01T00:00:00Z");
    CBL_ASSERT_EQ(parseDateAsStr("1 février 2010", 0), "2010-02-01T00:00:00Z");
    CBL_ASSERT_EQ(parseDateAsStr("1 février 9999", 0), "9999-02-01T00:00:00Z");
    // Year out of range.
    CBL_ASSERT_EQ(parseDateAsStr("1 février 0", 0), "none");
    CBL_ASSERT_EQ(parseDateAsStr("1 février 10000", 0), "none");
    // Bad month.
    CBL_ASSERT_EQ(parseDateAsStr("1 octembre 2003", 0), "none");
    // Bad day.
    CBL_ASSERT_EQ(parseDateAsStr("0 février 2003", 0), "none");
    CBL_ASSERT_EQ(parseDateAsStr("31 avril 2003", 0), "none");
    CBL_ASSERT_EQ(parseDateAsStr("32 janvier 2003", 0), "none");

    // DD/MM/YY or DD/MM/YYYY
    CBL_ASSERT_EQ(parseDateAsStr("01/02/2003", 0), "none");
    CBL_ASSERT_EQ(parseDateAsStr("01/02/2003", DateParser::ALLOW_NUMERIC_MONTH), "2003-02-01T00:00:00Z");
    CBL_ASSERT_EQ(parseDateAsStr("01/02/1803", DateParser::ALLOW_NUMERIC_MONTH), "1803-02-01T00:00:00Z");
    CBL_ASSERT_EQ(parseDateAsStr("1/2/2003", DateParser::ALLOW_NUMERIC_MONTH), "2003-02-01T00:00:00Z");
    // Two-digit year (parsing depends on the current year, set to 2010 in this test).
    CBL_ASSERT_EQ(parseDateAsStr("01/02/03", DateParser::ALLOW_NUMERIC_MONTH), "2003-02-01T00:00:00Z");
    CBL_ASSERT_EQ(parseDateAsStr("01/02/13", DateParser::ALLOW_NUMERIC_MONTH), "2013-02-01T00:00:00Z");
    CBL_ASSERT_EQ(parseDateAsStr("01/02/23", DateParser::ALLOW_NUMERIC_MONTH), "2023-02-01T00:00:00Z");
    CBL_ASSERT_EQ(parseDateAsStr("01/02/33", DateParser::ALLOW_NUMERIC_MONTH), "1933-02-01T00:00:00Z");
    CBL_ASSERT_EQ(parseDateAsStr("01/02/93", DateParser::ALLOW_NUMERIC_MONTH), "1993-02-01T00:00:00Z");

    // End of day
    CBL_ASSERT_EQ(parseDateAsStr("1 février 2003", DateParser::END_OF_DAY), "2003-02-01T23:59:59Z");
    CBL_ASSERT_EQ(parseDateAsStr("01/02/2003", DateParser::ALLOW_NUMERIC_MONTH | DateParser::END_OF_DAY),
                  "2003-02-01T23:59:59Z");

    // Should not SEGFAULT.
    CBL_ASSERT_EQ(parseDateAsStr("", 0), "none");
    CBL_ASSERT_EQ(parseDateAsStr("1", 0), "none");
    CBL_ASSERT_EQ(parseDateAsStr("1er", 0), "none");
    CBL_ASSERT_EQ(parseDateAsStr("1 février", 0), "none");
  }
  CBL_TEST_CASE(parseDateImplicitYear) {
    Date::setFrozenValueOfNow(Date::fromISO8601("2003-04-01T00:00:00Z"));
    CBL_ASSERT_EQ(parseDateAsStr("1 février", 0), "none");
    CBL_ASSERT_EQ(parseDateAsStr("1 novembre", DateParser::IMPLICIT_YEAR), "2002-11-01T00:00:00Z");
    CBL_ASSERT_EQ(parseDateAsStr("1 décembre", DateParser::IMPLICIT_YEAR), "2002-12-01T00:00:00Z");
    CBL_ASSERT_EQ(parseDateAsStr("1 janvier", DateParser::IMPLICIT_YEAR), "2003-01-01T00:00:00Z");
    CBL_ASSERT_EQ(parseDateAsStr("1 février", DateParser::IMPLICIT_YEAR), "2003-02-01T00:00:00Z");
    CBL_ASSERT_EQ(parseDateAsStr("1 mars", DateParser::IMPLICIT_YEAR), "2003-03-01T00:00:00Z");
    CBL_ASSERT_EQ(parseDateAsStr("1 avril", DateParser::IMPLICIT_YEAR), "2003-04-01T00:00:00Z");
    CBL_ASSERT_EQ(parseDateAsStr("1 mai", DateParser::IMPLICIT_YEAR), "2003-05-01T00:00:00Z");
    CBL_ASSERT_EQ(parseDateAsStr("1 juin", DateParser::IMPLICIT_YEAR), "2003-06-01T00:00:00Z");
    // DD/MM format with an implicit year is not supported.
    CBL_ASSERT_EQ(parseDateAsStr("01/02", DateParser::ALLOW_NUMERIC_MONTH | DateParser::IMPLICIT_YEAR), "none");
  }
  CBL_TEST_CASE(parseDateBeforeNow) {
    Date::setFrozenValueOfNow(Date::fromISO8601("2010-06-01T21:45:00Z"));
    CBL_ASSERT_EQ(parseDateAsStr("1 juin 2010", DateParser::BEFORE_NOW), "2010-06-01T00:00:00Z");
    CBL_ASSERT_EQ(parseDateAsStr("1 juin 2010", DateParser::BEFORE_NOW | DateParser::END_OF_DAY),
                  "2010-06-01T23:59:59Z");
    CBL_ASSERT_EQ(parseDateAsStr("2 juin 2010", DateParser::BEFORE_NOW), "none");
    CBL_ASSERT_EQ(parseDateAsStr("2 juin 2010", DateParser::BEFORE_NOW | DateParser::END_OF_DAY), "none");

    // If the computer time has an error of just 1 second, it may already be June 2 on UTC+2.
    Date::setFrozenValueOfNow(Date::fromISO8601("2010-06-01T21:59:59Z"));
    CBL_ASSERT_EQ(parseDateAsStr("1 juin 2010", DateParser::BEFORE_NOW), "2010-06-01T00:00:00Z");
    CBL_ASSERT_EQ(parseDateAsStr("1 juin 2010", DateParser::BEFORE_NOW | DateParser::END_OF_DAY),
                  "2010-06-01T23:59:59Z");
    CBL_ASSERT_EQ(parseDateAsStr("2 juin 2010", DateParser::BEFORE_NOW), "2010-06-02T00:00:00Z");
    CBL_ASSERT_EQ(parseDateAsStr("2 juin 2010", DateParser::BEFORE_NOW | DateParser::END_OF_DAY),
                  "2010-06-02T23:59:59Z");
  }

  string extractFirstDateStr(string_view text, int flags) const {
    return debugStringOfDate(m_dateParser->extractFirstDate(text, flags));
  }
  CBL_TEST_CASE(extractFirstDate) {
    CBL_ASSERT_EQ(extractFirstDateStr("1 février 2003", 0), "2003-02-01T00:00:00Z");
    CBL_ASSERT_EQ(extractFirstDateStr("1 février 2003 à 4:56 (CET)", 0), "2003-02-01T00:00:00Z");
    constexpr const char* TEXT_WITH_DATES =
        "Some text. 5 janvier\n"
        "Some text. 1 février 2015\n"
        "Some text. 1 février 2003\n"
        "Some text. 1 mars 2003\n";
    CBL_ASSERT_EQ(extractFirstDateStr(TEXT_WITH_DATES, DateParser::BEFORE_NOW), "2003-02-01T00:00:00Z");
    CBL_ASSERT_EQ(extractFirstDateStr(TEXT_WITH_DATES, 0), "2015-02-01T00:00:00Z");
    CBL_ASSERT_EQ(extractFirstDateStr(TEXT_WITH_DATES, DateParser::IMPLICIT_YEAR), "2010-01-05T00:00:00Z");
  }
  CBL_TEST_CASE(extractFirstDateImplicitYear) {
    Date::setFrozenValueOfNow(Date::fromISO8601("2003-04-01T00:00:00Z"));
    // If we accept implicit years but an explicit year is present, the explicit year is always used for parsing, which
    // may lead to rejection if it is not within the accepted range.
    CBL_ASSERT_EQ(extractFirstDateStr("1 février 100", DateParser::IMPLICIT_YEAR | DateParser::AFTER_2000), "none");
    // However, "100a" is parsed as a single token, so it is not interpreted as a possible year.
    CBL_ASSERT_EQ(extractFirstDateStr("1 février 100a", DateParser::IMPLICIT_YEAR | DateParser::AFTER_2000),
                  "2003-02-01T00:00:00Z");
  }

  const DateParser* m_dateParser = nullptr;
};

}  // namespace wikiutil

int main() {
  wikiutil::FrenchDateParserTest().run();
  return 0;
}
