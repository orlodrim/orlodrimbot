#ifndef MWC_REPLAY_WIKI_H
#define MWC_REPLAY_WIKI_H

#include <string>
#include "mwclient/wiki.h"

namespace mwc {

// Wiki subclass that can either record answers to queries in a json file or replay previously recorded answers for
// testing purposes.
class ReplayWiki : public Wiki {
public:
  enum AccountType {
    USER,
    SYSOP,
  };
  explicit ReplayWiki(const std::string& dataPath, AccountType accountType = USER);
  void startTestCase(const std::string& name);
  void endTestCase();
};

// Helper that calls ReplayWiki::startTestCase in the constructor and and ReplayWiki::endTestCase in the destructor.
class TestCaseRecord {
public:
  TestCaseRecord(ReplayWiki& wiki, const std::string& name);
  ~TestCaseRecord();

private:
  ReplayWiki& m_wiki;
};

}  // namespace mwc

#endif
