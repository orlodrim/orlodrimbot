// Copies recent changes from a wiki to a local sqlite database.
#ifndef LIVE_REPLICATION_RECENT_CHANGES_SYNC_H
#define LIVE_REPLICATION_RECENT_CHANGES_SYNC_H

#include <string>
#include <vector>
#include "cbl/date.h"
#include "cbl/sqlite.h"
#include "mwclient/wiki.h"

namespace live_replication {

class RecentChangesSync {
public:
  // Opens databasePath. The database is created if it does not exist.
  explicit RecentChangesSync(const std::string& databasePath);

  // Reads recent changes from the wiki and writes them to the database.
  void updateDatabaseFromWiki(mwc::Wiki& wiki);

  // Setting to a value higher than 0 can help to prevent the warning
  // "Ignoring change with rcid smaller than latest change from the previous update".
  void setSecondsToIgnore(int value) { m_secondsToIgnore = value; }

private:
  void writeRecentChanges(const std::vector<mwc::RecentChange>& recentChanges, const cbl::Date& requestDate);

  sqlite::Database m_database;
  int m_secondsToIgnore = 0;

  // The test reads directly from the database.
  friend class RecentChangesSyncTest;
};

}  // namespace live_replication

#endif
