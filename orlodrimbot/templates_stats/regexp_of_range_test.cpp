#include "regexp_of_range.h"
#include <re2/re2.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <string_view>
#include "cbl/log.h"
#include "cbl/string.h"
#include "cbl/unittest.h"

using std::string;
using std::string_view;

class RegExpOfRangeTest : public cbl::Test {
private:
  void assertMatch(string_view min, string_view max, string_view number) {
    re2::RE2 regExp(buildRegExpForRange(min, max));
    CBL_ASSERT(RE2::FullMatch(number, regExp));
  }
  void assertNoMatch(string_view min, string_view max, string_view number) {
    re2::RE2 regExp(buildRegExpForRange(min, max));
    CBL_ASSERT(!RE2::FullMatch(number, regExp));
  }

  CBL_TEST_CASE(ExactRegExp) {
    CBL_ASSERT_EQ(buildRegExpForRange("0", ""), R"([0-9]|[1-9]\d{1,})");
    CBL_ASSERT_EQ(buildRegExpForRange("1", ""), R"([1-9]\d*)");
    CBL_ASSERT_EQ(buildRegExpForRange("2", ""), R"([2-9]|[1-9]\d{1,})");
    CBL_ASSERT_EQ(buildRegExpForRange("9", ""), R"(9|[1-9]\d{1,})");
    CBL_ASSERT_EQ(buildRegExpForRange("10", ""), R"([1-9]\d|[1-9]\d{2,})");
    CBL_ASSERT_EQ(buildRegExpForRange("35", "8214"),
                  R"(3([5-9])|[4-9]\d|[1-9]\d{2,2}|[1-7]\d{3}|8([0-1]\d{2}|2(0\d|1([0-4]))))");
  }

  CBL_TEST_CASE(SpecificNumbers) {
    assertNoMatch("0", "", "00");
    assertMatch("0", "", "9");
    assertMatch("0", "", "10");
    assertMatch("0", "", "100");
    assertMatch("0", "1", "1");
    assertMatch("0", "10", "9");
    assertMatch("0", "10", "10");
    assertMatch("0", "20", "17");
    assertNoMatch("0", "44", "100");
    assertMatch("0", "100", "92");
    assertNoMatch("0", "290", "291");
    assertNoMatch("0", "772", "1000");
    assertNoMatch("1", "", "0");
    assertMatch("1", "", "1");
    assertMatch("1", "", "9");
    assertNoMatch("2", "", "1");
    assertMatch("3", "20", "20");
    assertNoMatch("3", "100", "101");
    assertMatch("5", "100", "10");
    assertNoMatch("10", "", "8");
    assertNoMatch("100", "", "9");
    assertMatch("100", "", "100");
    assertNoMatch("101", "", "96");
    assertNoMatch("102", "", "100");
  }

  /*
  static int getRandExp(int upperBound) {
    double multiplier = upperBound / exp(10.0 - 0.4);
    return static_cast<int>(exp(rand() / (RAND_MAX + 1.0) * 10.0 - 0.4) * multiplier);
  }
  CBL_TEST_CASE(TempTest) {
    int highestNumber = 20000;
    string example;
    for (int i = 0; i < 100000; i++) {
      int min = getRandExp(highestNumber);
      int max = rand() % 3 == 0 ? -1 : getRandExp(highestNumber);
      if (max != -1 && min > max) {
        std::swap(min, max);
      }
      string minStr = std::to_string(min);
      string maxStr = max == -1 ? "" : std::to_string(max);
      string regExpStr = buildRegExpForRange(minStr, maxStr);
      re2::RE2 regExp(regExpStr);
      for (int j = 0; j < 50; j++) {
        int number = getRandExp(highestNumber);
        string numberStr = std::to_string(number);
        bool addZero = rand() % 10 == 0;
        if (addZero) { numberStr = "0" + numberStr; }
        if (!addZero && number >= min && (max == -1 || number <= max)) {
          if (!RE2::FullMatch(numberStr, regExp)) {
            example = cbl::concat("assertMatch(\"", minStr, "\", \"", maxStr, "\", \"", numberStr, "\");");
            highestNumber = std::max(std::max(min, max), number);
          }
        } else {
          if (RE2::FullMatch(numberStr, regExp)) {
            example = cbl::concat("assertNoMatch(\"", minStr, "\", \"", maxStr, "\", \"", numberStr, "\");");
            highestNumber = std::max(std::max(min, max), number);
          }
        }
      }
    }
    if (!example.empty()) {
      std::cout << "Example:\n" << example << "\n";
    }
  }
  */
};

int main() {
  RegExpOfRangeTest().run();
  return 0;
}
