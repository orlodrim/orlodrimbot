#ifndef WIKIUTIL_WIKI_LOCAL_TIME_H
#define WIKIUTIL_WIKI_LOCAL_TIME_H

#include "cbl/date.h"

namespace wikiutil {

cbl::Date getFrWikiLocalTime(cbl::Date utcDate);

}  // namespace wikiutil

#endif
