#include "log.h"
#include <cstring>
#include <iostream>
#include <ostream>

namespace cbl_internal_log {

std::ostream& getLogStream(const EndOfLog&) {
  return std::cerr;
}

std::ostream& printLogLinePrefix(LogLevel level, const char* fileName, const EndOfLog&) {
  const char* prefix = "";
  switch (level) {
    case LogLevel::INFO:
      prefix = "[INFO ";
      break;
    case LogLevel::WARNING:
      prefix = "[WARNING ";
      break;
    case LogLevel::ERROR:
      prefix = "[ERROR ";
      break;
    case LogLevel::FATAL:
      prefix = "[FATAL ";
      break;
  }
  const char* baseName = fileName + strlen(fileName);
  for (; baseName > fileName && *(baseName - 1) != '/'; baseName--) {}
  return std::cerr << prefix << baseName << "] ";
}

}  // namespace cbl_internal_log
