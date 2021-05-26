#ifndef LIVE_REPLICATION_MOCK_RECENT_CHANGES_READER_H
#define LIVE_REPLICATION_MOCK_RECENT_CHANGES_READER_H

#include <string>
#include <vector>
#include "mwclient/wiki_defs.h"
#include "recent_changes_reader.h"

namespace live_replication {

class MockRecentChangesReader : public RecentChangesReader {
public:
  // The current implementation ignores options.properties and always retrieves all properties.
  void enumRecentChanges(const RecentChangesOptions& options, const Callback& callback) override;

  void resetMock();
  void addRC(const mwc::RecentChange& rc);
  void addEdit(const std::string& timestamp, const std::string& title, const std::string& user = std::string());
  void addMove(const std::string& timestamp, const std::string& title, const std::string& newTitle,
               const std::string& user = std::string());
  void addDeletion(const std::string& timestamp, const std::string& title, const std::string& user = std::string());
  mwc::RecentChange& lastRC() { return m_recentChanges.back(); }

private:
  std::vector<mwc::RecentChange> m_recentChanges;
};

}  // namespace live_replication

#endif
