// A class to define unit tests.
// Usage:
//   class MyTest : public cbl::Test {
//     CBL_TEST_CASE(SomeTest) { CBL_ASSERT_EQ(1 + 1, 2); }
//     CBL_TEST_CASE(SomeOtherTest) { CBL_ASSERT_EQ(2 * 2, 4); }
//   };
//   int main() {
//     MyTest().run();  // Run all tests defined with CBL_TEST_CASE.
//   }

#ifndef CBL_UNITTEST_H
#define CBL_UNITTEST_H

#include <functional>
#include <string>
#include <vector>

#define CBL_TEST_CASE(x)                                              \
  int testCaseRunner##x = [this]() {                                  \
    m_testCases.push_back({#x, [this]() { testCaseFunction##x(); }}); \
    return 0;                                                         \
  }();                                                                \
  void testCaseFunction##x()

namespace cbl {

class Test {
public:
  // Runs all tests by default, or just a specific test if testName is non-empty.
  void run(const std::string& testName = std::string());
  virtual ~Test();
  // Executed before each test case.
  virtual void setUp() {}
  // Executed after each test case.
  virtual void tearDown() {}

protected:
  struct TestCase {
    std::string name;
    std::function<void()> f;
  };
  std::vector<TestCase> m_testCases;
};

}  // namespace cbl

#endif
