#ifndef TALK_PAGE_ARCHIVER_THREAD_UTIL_H
#define TALK_PAGE_ARCHIVER_THREAD_UTIL_H

#include <string_view>
#include "cbl/date.h"

namespace talk_page_archiver {

// Returns the first line of a thread (including the '=' signs).
std::string_view extractThreadTitle(std::string_view text);

// Tries to extract a date from the title of a thread (text is the full content of that thread).
// Supported formats:
// - "day month year" ("5 janvier 2000")
// - "month year" ("janvier 2000"; returns the first or the last day of that month depending on maxForMissingFields)
// - "year" ("2000"; returns the first or the last day of that year depending on maxForMissingFields)
// - "day month" ("5 janvier"; makes a guess on the year based on the current date)
// If not date can be extracted, returns a null Date.
cbl::Date computeDateInTitle(std::string_view text, bool maxForMissingFields);

}  // namespace talk_page_archiver

#endif
