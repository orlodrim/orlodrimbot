#ifndef CBL_RANDOM_H
#define CBL_RANDOM_H

namespace cbl {

// Generates a pseudo-random integer in [0, upperBound).
int randomInt(int upperBound);

// Generates a pseudo-random double in [0, upperBound).
double randomDouble(double upperBound);

}  // namespace cbl

#endif
