#ifndef CHECK_STATUS_LIB_H
#define CHECK_STATUS_LIB_H

#include <string>
#include "mwclient/wiki.h"

void updateListOfStatusInconsistencies(mwc::Wiki& wiki, const std::string& listPage);

#endif
