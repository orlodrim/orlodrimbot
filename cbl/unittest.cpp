#include "unittest.h"
#include <string>
#include "log.h"

using std::string;

namespace cbl {

Test::~Test() {}

void Test::run(const string& testName) {
  int numTestCases = 0;
  for (const TestCase& testCase : m_testCases) {
    if (!testName.empty() && testCase.name != testName) {
      continue;
    }
    setUp();
    testCase.f();
    tearDown();
    numTestCases++;
  }
  CBL_ASSERT(numTestCases > 0);
}

}  // namespace cbl
