// Mock wiki for recent_changes_sync_test.cpp and recent_changes_reader_test.cpp.
#ifndef LIVE_REPLICATION_RECENT_CHANGES_TEST_UTIL_H
#define LIVE_REPLICATION_RECENT_CHANGES_TEST_UTIL_H

#include <string>
#include <utility>
#include <vector>
#include "mwclient/mock_wiki.h"
#include "mwclient/wiki.h"

namespace live_replication {

class RCSyncMockWiki : public mwc::MockWiki {
public:
  std::vector<mwc::RecentChange> getRecentChanges(const mwc::RecentChangesParams& params) override;
  void addRecentChange(mwc::RecentChange&& rc) { m_recentChanges.push_back(std::move(rc)); }

private:
  std::vector<mwc::RecentChange> m_recentChanges;
};

mwc::RecentChange makeRC(int rcid, const std::string& timestamp, const std::string& user, const std::string& title,
                         mwc::RecentChangeType type = mwc::RC_EDIT);

mwc::RecentChange makeLogRC(int rcid, int logid, mwc::LogEventType type, const std::string& action,
                            const std::string& timestamp, const std::string& user, const std::string& title,
                            const std::string& newTitle = "");

}  // namespace live_replication

#endif
