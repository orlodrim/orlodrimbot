#ifndef MOVE_SUBPAGES_LIB_H
#define MOVE_SUBPAGES_LIB_H

#include <string>
#include <vector>
#include "cbl/date.h"
#include "mwclient/wiki.h"

class SubpagesMover {
public:
  SubpagesMover(mwc::Wiki* wiki, const cbl::Date& dateOfLastProcessedMove, bool dryRun)
      : m_wiki(wiki), m_dateOfLastProcessedMove(dateOfLastProcessedMove), m_dryRun(dryRun) {}
  void processAllMoves();
  void processMove(const mwc::LogEvent& move);
  const cbl::Date& dateOfLastProcessedMove() { return m_dateOfLastProcessedMove; }

private:
  std::vector<mwc::LogEvent> readMoveLog(const cbl::Date& dateMin);
  std::vector<std::string> getSubpages(const std::string& title);

  mwc::Wiki* m_wiki = nullptr;
  cbl::Date m_dateOfLastProcessedMove;
  bool m_dryRun = false;
};

#endif
