#ifndef LIVE_REPLICATION_RECENT_CHANGES_READER_H
#define LIVE_REPLICATION_RECENT_CHANGES_READER_H

#include <functional>
#include <string>
#include <unordered_set>
#include <vector>
#include "cbl/date.h"
#include "cbl/sqlite.h"
#include "mwclient/wiki.h"

namespace live_replication {

struct RecentChangesOptions {
  // Combination of values from RecentChangeType. Special case: 0 means everything.
  int type = 0;
  int properties = mwc::RP_TITLE | mwc::RP_USER | mwc::RP_TIMESTAMP;
  bool includeLogDetails = false;
  // Starts enumerating from this timestamp (to the most recent). If continueToken is set, the enumeration may start
  // later (but not earlier).
  cbl::Date start;
  // Stops the enumeration at this timestamp. If continueToken is set, it is updated so that the enumeration can
  // continue after this date.
  cbl::Date end;
  // Stops the enumeration after returning that number of results. If continueToken is set, it is updated so that the
  // enumeration can continue at the point where it stopped.
  int limit = mwc::PAGER_ALL;
  // Token to continue the enumeration some time after a previous call (optional, input and output parameter).
  std::string* continueToken = nullptr;
};

struct RecentlyUpdatedPagesOptions {
  cbl::Date start;
  cbl::Date end;
  std::string excludedUser;
  std::string* continueToken = nullptr;
};

struct RecentLogEventsOptions {
  mwc::LogEventType logType = mwc::LE_UNDEFINED;  // LE_UNDEFINED means all.
  cbl::Date start;
  cbl::Date end;
  std::string* continueToken = nullptr;
};

// Reads recent changes from the local database written by live_replication.
class RecentChangesReader {
public:
  explicit RecentChangesReader(const std::string& databasePath);
  virtual ~RecentChangesReader() = default;

  using Callback = std::function<void(const mwc::RecentChange&)>;

  // Reads recent changes by increasing rcid and passes them to the callback function.
  // Increasing rcid implies mostly increasing timestamp, but not always (the timestamp might decrease by ~10 seconds).
  //
  // Typical usage for a tool that runs regularly and needs to process all recent changes:
  //   string continueToken = <get value from previous call or empty if it's the first run>;
  //   enumRecentChanges({.continueToken = &continueToken}, callback);
  //   <save the value of continueToken for the next call>
  // On the first run with an empty token, no changes are returned but continueToken is set so that the next call
  // returns changes starting from the time of that first run.
  //
  // To retrieve all recent changes in the last X days:
  //   enumRecentChanges({.start = Date::now() - DateDiff(86400 * X)}, callback);
  virtual void enumRecentChanges(const RecentChangesOptions& options, const Callback& callback);

  // Reads all titles that appear in recent changes from a specified point.
  // Moves, deletions, protections, uploads and imports are taken into account. For moves, both the source and the
  // target are returned.
  virtual std::unordered_set<std::string> getRecentlyUpdatedPages(const RecentlyUpdatedPagesOptions& options);

  // Variant of enumRecentChanges that only reads log events (title, user, timestamp, type, and action).
  virtual std::vector<mwc::LogEvent> getRecentLogEvents(const RecentLogEventsOptions& options);

protected:
  RecentChangesReader() = default;

  sqlite::Database m_database;
};

// Subclass that behaves as RecentChangesReader when created from an empty database (except it does not read any file).
class EmptyRecentChangesReader : public RecentChangesReader {
public:
  void enumRecentChanges(const RecentChangesOptions& options, const Callback& callback) override;
};

}  // namespace live_replication

#endif
