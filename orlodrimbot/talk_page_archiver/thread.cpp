#include "thread.h"
#include <re2/re2.h>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>
#include "cbl/date.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "mwclient/parser.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/wikiutil/date_parser.h"
#include "algorithm.h"
#include "archive_template.h"

using cbl::Date;
using cbl::DateDiff;
using mwc::Revision;
using mwc::RP_CONTENT;
using mwc::RP_TIMESTAMP;
using mwc::Wiki;
using std::string;
using std::string_view;
using std::unordered_set;
using std::vector;
using wikiutil::DateParser;
using wikiutil::SignatureDate;

namespace talk_page_archiver {

HistoryCache::HistoryCache(Wiki* wiki, const string& title) : m_wiki(wiki), m_title(title) {}

bool HistoryCache::searchThreadAtDate(const string& thread, const Date& date) {
  if (m_cache.count(date) == 0) {
    try {
      loadVersion(date);
    } catch (const mwc::WikiError& error) {
      CBL_ERROR << "Could not load the history of '" << m_title << "' at date " << date << ": " << error.what();
      // It is ok to continue. In the worst case, some threads without signature will not be archived.
    }
  }
  string trimmedThread(cbl::trim(thread));
  return m_cache.at(date).count(trimmedThread) != 0;
}

void HistoryCache::loadVersion(const Date& date) {
  // In any case, the cache is initialized (it will remain empty on error).
  unordered_set<string>& threadsText = m_cache[date];

  mwc::HistoryParams histParams;
  histParams.title = m_title;
  histParams.start = date;
  histParams.prop = RP_CONTENT | RP_TIMESTAMP;
  histParams.limit = 1;
  vector<Revision> history = m_wiki->getHistory(histParams);

  vector<Thread> threads;
  if (!history.empty()) {
    CBL_INFO << "First revision of '" << m_title << "' before " << date << ": " << history[0].timestamp;
    threads = parseCodeAsThreads(history[0].content);
  } else {
    CBL_INFO << "No revision of '" << m_title << "' before " << date;
  }

  threadsText.clear();
  for (const Thread& thread : threads) {
    threadsText.insert(string(cbl::trim(thread.text())));
  }
}

void Thread::computeState(Wiki* wiki, const Date& now, const vector<ParameterizedAlgorithm>& algorithms,
                          HistoryCache* historyCache) {
  static const re2::RE2 reNoArchive(R"(<!--\s*[Nn]e\s+pas\s+archiver\s*-->|\{\{\s*[Nn]e\s+pas\s+archiver\s*[|}])");
  if (m_titleLevel != 2) {
    m_state = ThreadState::NEVER_ARCHIVABLE_BECAUSE_OF_TITLE_LEVEL;
    return;
  } else if (containsArchiveTemplate(*wiki, m_text) || RE2::PartialMatch(m_text, reNoArchive)) {
    m_state = ThreadState::NEVER_ARCHIVABLE_BECAUSE_OF_TEXT;
    return;
  }
  m_state = ThreadState::NOT_ARCHIVABLE_YET;
  SignatureDate defaultThreadDate = DateParser::getByLang("fr").extractMaxSignatureDate(m_text);
  for (const ParameterizedAlgorithm& algo : algorithms) {
    Algorithm::RunResult runResult = algo.algorithm->run(*wiki, m_text);
    if (runResult.action == ThreadAction::KEEP) {
      continue;
    }
    Date archiveThreshold = now - DateDiff::fromDays(algo.maxAgeInDays);
    SignatureDate threadDateForAlgorithm = runResult.forcedDate.isNull() ? defaultThreadDate : runResult.forcedDate;
    if (threadDateForAlgorithm.isNull()) {
      if (!historyCache->searchThreadAtDate(m_text, archiveThreshold)) {
        continue;
      }
      threadDateForAlgorithm = {.utcDate = archiveThreshold};
    } else if (threadDateForAlgorithm.utcDate >= archiveThreshold) {
      continue;
    }
    m_date = threadDateForAlgorithm;
    m_algoMaxAgeInDays = algo.maxAgeInDays;
    m_state = runResult.action == ThreadAction::ERASE ? ThreadState::ERASABLE : ThreadState::ARCHIVABLE;
    break;
  }
}

int getTitleLevel(string_view line) {
  string lineWithoutComments = wikicode::stripComments(line);
  string_view normLine = cbl::trim(lineWithoutComments, cbl::TRIM_RIGHT);
  int i = 0, n = normLine.size();
  for (; i < n && normLine[i] == '=' && normLine[n - 1 - i] == '='; i++) {
  }
  return i < n ? i : 0;
}

vector<Thread> parseCodeAsThreads(string_view code) {
  vector<Thread> threads;
  int threadTitleLevel = 0;
  string threadText;
  for (string_view line : cbl::splitLines(code)) {
    int titleLevel = getTitleLevel(line);
    if (titleLevel != 0 && titleLevel <= 2) {
      if (!threadText.empty()) {
        threads.emplace_back(threadTitleLevel, threadText);
        threadText.clear();
      }
      threadTitleLevel = titleLevel;
    }
    cbl::append(threadText, line, "\n");
  }
  if (!threadText.empty()) {
    threads.emplace_back(threadTitleLevel, threadText);
  }
  return threads;
}

}  // namespace talk_page_archiver
