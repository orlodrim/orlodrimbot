#ifndef TEMPLATES_STATS_PARSE_TEMPLATES_LIB_H
#define TEMPLATES_STATS_PARSE_TEMPLATES_LIB_H

#include <istream>
#include <string>
#include "mwclient/wiki.h"

// inputStream must be seekable because it is read twice.
void parseTemplatesFromDump(mwc::Wiki& wiki, std::istream& inputStream, const std::string& paramsPath,
                            const std::string& namesPath, const std::string& templateDataPath);

#endif
