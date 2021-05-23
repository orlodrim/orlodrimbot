#include "recent_changes_test_util.h"
#include <algorithm>
#include <string>
#include <vector>
#include "cbl/date.h"
#include "cbl/log.h"
#include "mwclient/wiki.h"

using cbl::Date;
using mwc::LogEvent;
using mwc::LogEventType;
using mwc::RecentChange;
using mwc::RecentChangesParams;
using mwc::RecentChangeType;
using mwc::Revision;
using std::string;
using std::vector;

namespace live_replication {

vector<RecentChange> RCSyncMockWiki::getRecentChanges(const RecentChangesParams& params) {
  constexpr int requiredProps = mwc::RP_TITLE | mwc::RP_REVID | mwc::RP_USER | mwc::RP_TIMESTAMP;
  CBL_ASSERT_EQ((params.prop & requiredProps), requiredProps);

  int changesCount = m_recentChanges.size();
  int startIndex = 0, step = 1;
  Date minTimestamp = params.start, maxTimestamp = params.end;
  if (params.direction == mwc::NEWEST_FIRST) {
    startIndex = changesCount - 1;
    step = -1;
    std::swap(minTimestamp, maxTimestamp);
  }
  int limit = params.limit;
  vector<RecentChange> results;
  for (int i = startIndex; i >= 0 && i < changesCount && limit != 0; i += step) {
    const RecentChange& rc = m_recentChanges[i];
    if (rc.timestamp() >= minTimestamp && (maxTimestamp.isNull() || rc.timestamp() <= maxTimestamp)) {
      results.push_back(rc.copy());
    }
    if (limit != mwc::PAGER_ALL) {
      limit--;
    }
  }
  return results;
}

RecentChange makeRC(int rcid, const string& timestamp, const string& user, const string& title, RecentChangeType type) {
  CBL_ASSERT(type == mwc::RC_EDIT || type == mwc::RC_NEW);
  RecentChange rc;
  rc.rcid = rcid;
  rc.setType(type);
  Revision& revision = rc.mutableRevision();
  revision.timestamp = Date::fromISO8601(timestamp);
  revision.user = user;
  revision.title = title;
  return rc;
}

RecentChange makeLogRC(int rcid, int logid, LogEventType type, const string& action, const string& timestamp,
                       const string& user, const string& title, const string& newTitle) {
  RecentChange rc;
  rc.rcid = rcid;
  rc.setType(mwc::RC_LOG);
  LogEvent& logEvent = rc.mutableLogEvent();
  logEvent.type = type;
  logEvent.action = action;
  logEvent.timestamp = Date::fromISO8601(timestamp);
  logEvent.user = user;
  logEvent.title = title;
  logEvent.logid = logid;
  logEvent.setNewTitle(newTitle);
  return rc;
}

}  // namespace live_replication
