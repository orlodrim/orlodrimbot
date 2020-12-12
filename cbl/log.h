// A library to log to std::cerr with some context (file and line).
// Usage:
//   CBL_INFO << "Something is happening";
//   CBL_WARNING << "Something strange is happening";
//   CBL_ERROR << "Something wrong is happening";
//   CBL_FATAL << "Something wrong happened and the process will end now";
//
// The CBL_ASSERT macro is similar to assert but allows extra logging.
//   CBL_ASSERT(container.find(key) != container.end()) << "key=" << key;
//
// CBL_ASSERT_EQ is a specialization for equality tests that prints values in case of failure.
//   CBL_ASSERT_EQ(container[key], "expected value") << "key=" << key;

#ifndef CBL_LOG_H
#define CBL_LOG_H

#include <cstdlib>
#include <iostream>
#include <ostream>

// Defines a namespace outside of cbl so that resolution for operator<<(ostream&, Date) in cbl does not hide overloads
// in the global namespace.
namespace cbl_internal_log {

enum class LogLevel {
  INFO,
  WARNING,
  ERROR,
  FATAL,
};

struct EndOfLog {};

struct EndOfNonFatalLog : public EndOfLog {
  ~EndOfNonFatalLog() { std::cerr << '\n'; }
};

struct EndOfFatalLog : public EndOfLog {
  [[noreturn]] ~EndOfFatalLog() {
    std::cerr << '\n';
    exit(1);
  }
};

std::ostream& getLogStream(const EndOfLog&);
std::ostream& printLogLinePrefix(LogLevel level, const char* fileName, const EndOfLog&);

template <class X, class Y>
bool assertEqShouldFail(const X& x, const Y& y, const char* assertionText, const char* fileName) {
  if (!(x == y)) {
    printLogLinePrefix(LogLevel::FATAL, fileName, EndOfLog())
        << "Assertion " << assertionText << " failed (" << x << " != " << y << ") ";
    return true;
  }
  return false;
}

}  // namespace cbl_internal_log

#define CBL_INTERNAL_STRINGIFY1(x) #x
#define CBL_INTERNAL_STRINGIFY2(x) CBL_INTERNAL_STRINGIFY1(x)
#define CBL_HERE __FILE__ ":" CBL_INTERNAL_STRINGIFY2(__LINE__)
#define CBL_INTERNAL_LOG(level, endClass) \
  ::cbl_internal_log::printLogLinePrefix(::cbl_internal_log::LogLevel::level, CBL_HERE, ::cbl_internal_log::endClass())

#define CBL_INFO CBL_INTERNAL_LOG(INFO, EndOfNonFatalLog)
#define CBL_WARNING CBL_INTERNAL_LOG(WARNING, EndOfNonFatalLog)
#define CBL_ERROR CBL_INTERNAL_LOG(ERROR, EndOfNonFatalLog)

#define CBL_FATAL CBL_INTERNAL_LOG(FATAL, EndOfFatalLog)

#define CBL_ASSERT(condition) \
  if (!(condition)) CBL_FATAL << "Assertion " #condition " failed "

#define CBL_ASSERT_EQ(x, y)                                                 \
  if (::cbl_internal_log::assertEqShouldFail(x, y, #x " == " #y, CBL_HERE)) \
  ::cbl_internal_log::getLogStream(::cbl_internal_log::EndOfFatalLog())

#endif
