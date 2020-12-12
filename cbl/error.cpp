#include "error.h"
#include <cstring>
#include <string>

using std::string;

namespace cbl {

string getCErrorString(int errorNumber) {
  char buffer[0x100];
  buffer[0] = '\0';
  return strerror_r(errorNumber, buffer, sizeof(buffer));
}

}  // namespace cbl
