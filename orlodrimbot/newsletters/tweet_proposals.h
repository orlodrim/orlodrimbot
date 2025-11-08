#ifndef TWEET_PROPOSALS_H
#define TWEET_PROPOSALS_H

#include <string>
#include <vector>
#include "cbl/date.h"
#include "mwclient/wiki.h"

namespace newsletters {

class TweetProposals {
public:
  struct Section {
    cbl::Date date;
    std::string code;
  };

  explicit TweetProposals(mwc::Wiki* wiki);
  ~TweetProposals();
  // Throws: mwc::WikiError.
  void load();
  // Throws: mwc::WikiError.
  void writePage(const std::string& comment);
  // Throws: cbl::ParseError.
  void addProposal(const std::string& proposal);
  // Throws: cbl::ParseError.
  void addProposalWithDate(const std::string& proposal, const cbl::Date& date);

private:
  mwc::Wiki* m_wiki = nullptr;
  mwc::WriteToken m_proposalsPageWriteToken;
  std::vector<Section> m_sections;
};

}  // namespace newsletters

#endif
