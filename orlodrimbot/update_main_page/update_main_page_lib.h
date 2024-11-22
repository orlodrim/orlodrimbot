#ifndef UPDATE_MAIN_PAGE_LIB_H
#define UPDATE_MAIN_PAGE_LIB_H

#include "cbl/json.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/live_replication/recent_changes_reader.h"
#include "template_expansion_cache.h"

void updateMainPage(mwc::Wiki& wiki, json::Value& state, live_replication::RecentChangesReader& recentChangesReader,
                    TemplateExpansionCache& templateExpansionCache);

#endif
