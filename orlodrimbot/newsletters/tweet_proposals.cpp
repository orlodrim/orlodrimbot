#include "tweet_proposals.h"
#include <string>
#include <string_view>
#include <utility>
#include "cbl/date.h"
#include "cbl/error.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "mwclient/parser.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/wikiutil/date_parser.h"

using cbl::Date;
using cbl::DateDiff;
using mwc::Wiki;
using std::string;
using std::string_view;
using wikiutil::DateParser;

namespace newsletters {
namespace {

constexpr const char* TWEETS_PAGE = "Wikipédia:Réseaux sociaux/Publications";

void normalizeEndOfCode(string& code) {
  int lineBreaksToAdd = 2;
  for (int whitespacePosition = code.size(); whitespacePosition > 0; whitespacePosition--) {
    char c = code[whitespacePosition - 1];
    if (c == '\n') {
      lineBreaksToAdd--;
    } else if (c != ' ') {
      if (c == '=') {
        // Only one line break is required after a title.
        lineBreaksToAdd--;
      }
      break;
    }
  }
  if (lineBreaksToAdd > 0) {
    code += string(lineBreaksToAdd, '\n');
  }
}

}  // namespace

TweetProposals::TweetProposals(Wiki* wiki) : m_wiki(wiki) {}

TweetProposals::~TweetProposals() {}

void TweetProposals::load() {
  string code = m_wiki->readPageContent(TWEETS_PAGE, &m_proposalsPageWriteToken);

  m_sections.clear();
  m_sections.emplace_back();
  Section* currentSection = &m_sections[0];

  const DateParser& dateParser = DateParser::getByLang("fr");
  for (string_view line : cbl::splitLines(code)) {
    if (wikicode::getTitleLevel(line) != 0) {
      if (!currentSection->code.empty()) {
        m_sections.emplace_back();
        currentSection = &m_sections.back();
      }
      currentSection->date = dateParser.extractFirstDate(wikicode::getTitleContent(line),
                                                         DateParser::AFTER_2000 | DateParser::IMPLICIT_YEAR);
    }
    currentSection->code += line;
    currentSection->code += '\n';
  }
}

void TweetProposals::writePage(const string& comment) {
  if (m_proposalsPageWriteToken.type() == mwc::WriteToken::UNINITIALIZED) {
    throw cbl::InvalidStateError(string("Cannot write '") + TWEETS_PAGE + "' before reading it");
  }
  string code;
  for (const Section& section : m_sections) {
    code += section.code;
  }
  m_wiki->writePage(TWEETS_PAGE, code, m_proposalsPageWriteToken, comment);
}

void TweetProposals::addProposal(const string& proposal) {
  Date tomorrow = (Date::now() + DateDiff::fromDays(1)).extractDay();
  addProposalWithDate(proposal, tomorrow);
}

void TweetProposals::addProposalWithDate(const string& proposal, const Date& date) {
  for (Section& section : m_sections) {
    if (section.date == date) {
      size_t insertionPoint = section.code.find('\n');
      CBL_ASSERT(insertionPoint != string::npos);
      insertionPoint++;
      string newCode = section.code.substr(0, insertionPoint) + proposal;
      normalizeEndOfCode(newCode);
      for (; insertionPoint < section.code.size() && section.code[insertionPoint] == '\n'; insertionPoint++) {}
      newCode += section.code.substr(insertionPoint);
      section.code = std::move(newCode);
      return;
    }
  }
  throw cbl::ParseError("No section found for date " + date.toISO8601() + " on the list of tweet proposals");
}

}  // namespace newsletters
