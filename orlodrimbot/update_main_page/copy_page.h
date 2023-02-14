#ifndef COPY_PAGE_H
#define COPY_PAGE_H

#include <string>
#include "cbl/error.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/live_replication/recent_changes_reader.h"

class CopyError : public cbl::Error {
public:
  using Error::Error;
};

// Copy the code of sourcePage to targetPage if the sourcePage does not include any recently modified template.
void copyPageIfTemplatesAreUnchanged(mwc::Wiki& wiki, live_replication::RecentChangesReader* recentChangesReader,
                                     const std::string& stateFile, const std::string& sourcePage,
                                     const std::string& targetPage);

#endif
