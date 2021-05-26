#ifndef DRAFT_MOVED_TO_MAIN_LIB_H
#define DRAFT_MOVED_TO_MAIN_LIB_H

#include <list>
#include <string>
#include "cbl/date.h"
#include "cbl/json.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/live_replication/recent_changes_reader.h"

constexpr const char* LIST_TITLE = "Utilisateur:OrlodrimBot/Créations par déplacement";

class ListOfPublishedDrafts {
public:
  ListOfPublishedDrafts(mwc::Wiki* wiki, live_replication::RecentChangesReader* recentChangesReader,
                        const std::string& stateFile, int daysToKeep);
  void update(bool dryRun);

private:
  struct Article {
    std::string draftTitle;
    std::string firstTitleInMain;
    std::string currentTitle;
    std::string publisher;
    cbl::Date publishDate;
    cbl::Date lastMoveDate;
    bool deleted = false;
  };
  using Articles = std::list<Article>;

  // Computes the list of published drafts since the point stored in state. As a side effect, updates the state.
  Articles getNewlyPublishedDrafts(json::Value& state);
  std::string describeNewArticle(const Article& article);
  std::string generateEditSummary(const Articles& articles);
  void updateBotSection(const Articles& newArticles, bool dryRun);

  mwc::Wiki* m_wiki;
  live_replication::RecentChangesReader* m_recentChangesReader;
  std::string m_stateFile;
  int m_daysToKeep;
};

#endif
