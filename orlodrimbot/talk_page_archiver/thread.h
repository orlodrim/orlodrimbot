#ifndef TALK_PAGE_ARCHIVER_THREAD_H
#define TALK_PAGE_ARCHIVER_THREAD_H

#include <map>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>
#include "cbl/date.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/wikiutil/date_parser.h"
#include "algorithm.h"

namespace talk_page_archiver {

// Cache of old revisions of a page.
// Used to check the age of threads whose content does not contain a signature.
class HistoryCache {
public:
  HistoryCache(mwc::Wiki* wiki, const std::string& title);
  bool searchThreadAtDate(const std::string& thread, const cbl::Date& date);

private:
  // Throws: WikiError.
  void loadVersion(const cbl::Date& date);

  mwc::Wiki* m_wiki;
  std::string m_title;
  std::map<cbl::Date, std::unordered_set<std::string>> m_cache;
};

enum class ThreadState {
  // The thread is either the header of the page or a section with title level 1 ("= Section =").
  // It will never be archived.
  NEVER_ARCHIVABLE_BECAUSE_OF_TITLE_LEVEL,
  // The thread contains a template that blocks archiving ({{Ne pas archiver}}).
  // Unlike NEVER_ARCHIVABLE_BECAUSE_OF_TITLE_LEVEL, it counts when computing how many threads are left on the page.
  NEVER_ARCHIVABLE_BECAUSE_OF_TEXT,
  // The thread is not old enough to be archived.
  NOT_ARCHIVABLE_YET,
  // An algorithm decided that the thread should be archived.
  ARCHIVABLE,
  // An algorithm decided that the thread should be erased.
  ERASABLE,

  // The two last states are only set in a second pass, once we know which threads are ARCHIVABLE/ERASABLE.
  // Due to the lower bound on the number of threads left on the page, some threads may stay in state
  // ARCHIVABLE/ERASABLE instead of switching to ARCHIVED/ERASED.
  ARCHIVED,  // The thread is archived.
  ERASED,    // The thread is erased.
};

// Section in a talk page.
struct Thread {
public:
  Thread(int titleLevel, const std::string& text) : m_titleLevel(titleLevel), m_text(text) {}
  void computeState(mwc::Wiki* wiki, const cbl::Date& now, const std::vector<ParameterizedAlgorithm>& algorithms,
                    HistoryCache* historyCache);

  // Level of the section ('= Section =' => 1, '== Section ==' => 2, page header => 0).
  // The archiver only works on sections of level 2. Section of level >= 3 are not considered as separate sections.
  int titleLevel() const { return m_titleLevel; }
  // Text of the thread, including the title.
  const std::string& text() const { return m_text; }
  // Date of the last change in this thread.
  const wikiutil::SignatureDate& date() const { return m_date; }
  // Delay of the algorithm that caused the thread to be archived. Used to generate the edit summary.
  int algoMaxAgeInDays() const { return m_algoMaxAgeInDays; }

  // What the archiver will do or has done with the thread.
  ThreadState state() const { return m_state; }
  // When we know the thread count, we can decide for which ARCHIVABLE and ERASABLE threads can become ARCHIVED/ERASED.
  void setState(ThreadState value) { m_state = value; }

private:
  int m_titleLevel = 0;
  std::string m_text;
  wikiutil::SignatureDate m_date;
  int m_algoMaxAgeInDays = 0;
  ThreadState m_state = ThreadState::NEVER_ARCHIVABLE_BECAUSE_OF_TITLE_LEVEL;
};

// Splits the wikicode of a page into a vector of Threads.
std::vector<Thread> parseCodeAsThreads(std::string_view code);

}  // namespace talk_page_archiver

#endif
