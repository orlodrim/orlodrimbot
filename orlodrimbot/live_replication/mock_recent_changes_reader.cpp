#include "mock_recent_changes_reader.h"
#include <string>
#include "cbl/date.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "mwclient/wiki.h"
#include "recent_changes_reader.h"

using cbl::Date;
using mwc::LogEvent;
using mwc::RecentChange;
using mwc::Revision;
using std::string;

namespace live_replication {

void MockRecentChangesReader::enumRecentChanges(const RecentChangesOptions& options, const Callback& callback) {
  int changesCount = m_recentChanges.size();
  int firstChange;
  if (options.continueToken && !options.continueToken->empty()) {
    CBL_ASSERT_EQ((*options.continueToken)[0], 'T');
    firstChange = cbl::parseInt(options.continueToken->substr(1));
  } else if (!options.start.isNull()) {
    for (firstChange = 0; firstChange < changesCount && m_recentChanges[firstChange].timestamp() < options.start;
         firstChange++) {}
  } else {
    firstChange = changesCount;
  }
  for (int i = firstChange; i < changesCount; i++) {
    callback(m_recentChanges[i]);
  }
  if (options.continueToken) {
    *options.continueToken = "T" + std::to_string(changesCount);
  }
}

void MockRecentChangesReader::resetMock() {
  m_recentChanges.clear();
}

void MockRecentChangesReader::addRC(const RecentChange& rc) {
  m_recentChanges.push_back(rc.copy());
}

void MockRecentChangesReader::addEdit(const string& timestamp, const string& title, const string& user) {
  m_recentChanges.emplace_back();
  RecentChange& rc = m_recentChanges.back();
  rc.setType(mwc::RC_EDIT);
  Revision& revision = rc.mutableRevision();
  revision.timestamp = Date::fromISO8601(timestamp);
  revision.title = title;
  revision.user = user;
}

void MockRecentChangesReader::addMove(const string& timestamp, const string& title, const string& newTitle,
                                      const string& user) {
  m_recentChanges.emplace_back();
  RecentChange& rc = m_recentChanges.back();
  rc.setType(mwc::RC_LOG);
  LogEvent& logEvent = rc.mutableLogEvent();
  logEvent.setType(mwc::LE_MOVE);
  logEvent.timestamp = Date::fromISO8601(timestamp);
  logEvent.title = title;
  logEvent.user = user;
  logEvent.action = "move";
  logEvent.mutableMoveParams().newTitle = newTitle;
}

void MockRecentChangesReader::addDeletion(const string& timestamp, const string& title, const string& user) {
  m_recentChanges.emplace_back();
  RecentChange& rc = m_recentChanges.back();
  rc.setType(mwc::RC_LOG);
  LogEvent& logEvent = rc.mutableLogEvent();
  logEvent.setType(mwc::LE_DELETE);
  logEvent.timestamp = Date::fromISO8601(timestamp);
  logEvent.title = title;
  logEvent.user = user;
  logEvent.action = "delete";
}

}  // namespace live_replication
