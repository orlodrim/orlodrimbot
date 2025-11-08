#include "random.h"
#include <cstdlib>
#include <ctime>

namespace cbl {

static int randWithInit() {
  [[maybe_unused]] static int unusedZero = []() {
    srand(time(nullptr));
    return 0;
  }();
  return rand();
}

double randomDouble(double upperBound) {
  return (randWithInit() / (RAND_MAX + 1.0)) * upperBound;
}

int randomInt(int upperBound) {
  return static_cast<int>(randomDouble(upperBound));
}

}  // namespace cbl
