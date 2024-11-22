#include "archiver.h"
#include <re2/re2.h>
#include <algorithm>
#include <climits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "cbl/date.h"
#include "cbl/error.h"
#include "cbl/file.h"
#include "cbl/log.h"
#include "cbl/path.h"
#include "cbl/string.h"
#include "mwclient/parser.h"
#include "mwclient/titles_util.h"
#include "mwclient/util/templates_by_name.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/wikiutil/date_formatter.h"
#include "archive_template.h"
#include "frwiki_algorithms.h"
#include "thread.h"
#include "thread_util.h"

using cbl::Date;
using cbl::DateDiff;
using mwc::revid_t;
using mwc::Revision;
using mwc::Wiki;
using std::map;
using std::optional;
using std::set;
using std::string;
using std::string_view;
using std::unique_ptr;
using std::vector;
using wikiutil::DateFormatter;

namespace talk_page_archiver {
namespace {

class ArchiverError : public cbl::Error {
public:
  using Error::Error;
};

enum ArchiveOrder {
  OLDEST_SECTION_FIRST,
  NEWEST_SECTION_FIRST,
};

int computePageSize(Wiki* wiki, const string& title) {
  CBL_INFO << "Checking size of '" << title << "'";
  try {
    return wiki->readPage(title, mwc::RP_SIZE).size;
  } catch (const mwc::PageNotFoundError&) {
    return 0;
  }
}

string replaceCounter(const string& format, int counter) {
  return cbl::replace(format, "%(counter)d", std::to_string(counter));
}

string padWithZeros(int number, int zeros) {
  const string numberAsString = std::to_string(number);
  return string(std::max(zeros - (int) numberAsString.size(), 0), '0') + numberAsString;
}

set<revid_t> loadStableRevids(const string& path) {
  set<revid_t> revids;
  try {
    string content = cbl::readFile(path);
    for (string_view line : cbl::splitLines(content)) {
      revids.insert(cbl::parseInt64(cbl::legacyStringConv(line)));
    }
    return revids;
  } catch (const cbl::FileNotFoundError& error) {
    CBL_WARNING << "Cannot load stable revision ids: " << error.what();
  } catch (const cbl::SystemError& error) {
    CBL_ERROR << "Cannot load stable revision ids: " << error.what();
  } catch (const cbl::ParseError& error) {
    CBL_ERROR << "Failed to parse '" << path << "': " << error.what();
  }
  return {};
}

void saveStableRevids(const string& path, const set<revid_t>& revids) {
  string content;
  for (revid_t revid : revids) {
    content += std::to_string(revid);
    content += '\n';
  }
  try {
    cbl::writeFile(path, content);
  } catch (const cbl::SystemError& error) {
    CBL_ERROR << "Failed to save stable revision ids: " << error.what();
  }
}

void filterStablePages(Wiki& wiki, const vector<string>& pages, const set<revid_t>& oldStableRevids,
                       vector<string>& pagesToUpdate, set<revid_t>& stableRevids) {
  vector<Revision> revisions(pages.size());
  for (size_t i = 0; i < pages.size(); i++) {
    revisions[i].title = pages[i];
  }
  wiki.readPages(mwc::RP_REVID, revisions);
  pagesToUpdate.clear();
  stableRevids.clear();
  for (const Revision& revision : revisions) {
    if (oldStableRevids.count(revision.revid) != 0) {
      CBL_INFO << "Skipping stable page '" << revision.title << "'";
      stableRevids.insert(revision.revid);
    } else {
      pagesToUpdate.push_back(revision.title);
    }
  }
}

void tryToUpdateDatesInHeader(Wiki& wiki, string& content, const Date& oldestAddedThread,
                              const Date& newestAddedThread) {
  size_t endOfHeader = content.find("\n=");
  if (endOfHeader == string::npos) {
    endOfHeader = content.size();
  }
  wikicode::List parsedCode = wikicode::parse(string_view(content).substr(0, endOfHeader));
  for (wikicode::Template& template_ : wikicode::getTemplatesByName(wiki, parsedCode, "Archive de discussion")) {
    wikicode::ParsedFields parsedFields = template_.getParsedFields();
    int startIndex = parsedFields.indexOf("Début");
    int endIndex = parsedFields.indexOf("Fin");
    const DateFormatter& dateFormatter = DateFormatter::getByLang("fr");
    bool hasStart = startIndex != wikicode::FIND_PARAM_NONE;
    if (!hasStart && !oldestAddedThread.isNull()) {
      template_.addField("Début=" + dateFormatter.format(oldestAddedThread, DateFormatter::LONG_1ST_TEMPLATE));
      hasStart = true;
    }
    string endValue = dateFormatter.format(newestAddedThread, DateFormatter::LONG_1ST_TEMPLATE);
    if (endIndex != wikicode::FIND_PARAM_NONE) {
      template_.setFieldValue(endIndex, endValue);
    } else if (hasStart) {
      template_.addField("Fin=" + endValue);
    }
    content = parsedCode.toString() + content.substr(endOfHeader);
    break;
  }
}

class PageToArchive {
public:
  void load(Wiki* wiki, const string& code);
  const vector<Thread*>& reorderedThreads() { return m_reorderedThreads; }
  bool hasTrackingTemplate() const { return m_categoryTrackingTemplate != nullptr; }
  string generateCode() const;

private:
  string m_header;
  string m_footer;
  vector<Thread> m_threads;
  vector<Thread*> m_reorderedThreads;
  wikicode::TemplatePtr m_categoryTrackingTemplate;
};

void PageToArchive::load(Wiki* wiki, const string& code) {
  string codeInTemplate;
  if (extractTrackingTemplate(wiki, code, m_categoryTrackingTemplate, codeInTemplate, m_header, m_footer)) {
    m_threads = parseCodeAsThreads(codeInTemplate);
    // TODO: Prevent the first thread with a title from being archived.
  } else {
    m_header.clear();
    m_footer.clear();
    m_threads = parseCodeAsThreads(code);
    m_categoryTrackingTemplate.reset(nullptr);
  }
  for (Thread& thread : m_threads) {
    m_reorderedThreads.push_back(&thread);
  }
  if (m_categoryTrackingTemplate) {
    std::reverse(m_reorderedThreads.begin(), m_reorderedThreads.end());
  }
}

string PageToArchive::generateCode() const {
  string newCodeInTemplate;
  Date newMinDate;

  for (const Thread& thread : m_threads) {
    if (thread.state() != ThreadState::ARCHIVED && thread.state() != ThreadState::ERASED) {
      if (m_categoryTrackingTemplate != nullptr) {
        const Date dateInTitle = computeDateInTitle(thread.text(), /* maxForMissingFields = */ false);
        if (!dateInTitle.isNull()) {
          if (newMinDate.isNull() || newMinDate > dateInTitle) {
            newMinDate = dateInTitle;
          }
        }
      }
      newCodeInTemplate += thread.text();
    } else if (m_categoryTrackingTemplate != nullptr) {
      Date dateInTitle = computeDateInTitle(thread.text(), /* maxForMissingFields = */ true);
      if (!dateInTitle.isNull()) {
        dateInTitle = dateInTitle + DateDiff::fromDays(1);
        if (newMinDate.isNull() || newMinDate < dateInTitle) {
          newMinDate = dateInTitle;
        }
      }
    }
  }
  string newCode;
  newCode += m_header;
  if (m_categoryTrackingTemplate != nullptr) {
    if (newMinDate.isNull()) {
      CBL_WARNING << "Cannot extract any date from the remaining threads to update \"date min\" field in category "
                  << "tracking template";
    } else {
      int dateField = m_categoryTrackingTemplate->getParsedFields().indexOf("date min");
      string newMinDateStr = DateFormatter::getByLang("fr").format(newMinDate);
      if (dateField == wikicode::FIND_PARAM_NONE) {
        m_categoryTrackingTemplate->addField("date min = " + newMinDateStr);
      } else {
        m_categoryTrackingTemplate->setFieldValue(dateField, newMinDateStr);
      }
    }
    m_categoryTrackingTemplate->addToBuffer(newCode);
  }
  newCode += newCodeInTemplate;
  newCode += m_footer;
  return newCode;
}

class ArchivePage {
public:
  ArchivePage(const string& title, ArchiveOrder order) : m_title(title), m_order(order) {}
  void load(Wiki* wiki);
  void addThread(const Thread& thread, const string& archiveHeader, bool insertDatesInHeader);
  void update(Wiki* wiki, const string& sourcePage, bool dryRun) const;

  const string& title() const { return m_title; }
  int size() const { return m_size; }
  int numThreads() const { return m_numThreads; }

private:
  const string m_title;
  ArchiveOrder m_order = OLDEST_SECTION_FIRST;
  int m_size = -1;
  string m_newHeader;
  vector<string> m_newThreads;
  int m_numThreads = 0;

  bool m_justCreated = false;
  Date m_oldestAddedThread;
  Date m_newestAddedThread;
};

void ArchivePage::load(Wiki* wiki) {
  m_size = computePageSize(wiki, m_title);
  if (m_size >= 1900000) {
    CBL_ERROR << "Very large archive page '" << m_title << "'";
  } else if (m_size >= 1000000) {
    CBL_WARNING << "Large archive page '" << m_title << "'";
  }
}

void ArchivePage::addThread(const Thread& thread, const string& archiveHeader, bool insertDatesInHeader) {
  if (m_size == 0) {
    m_newHeader = archiveHeader;
    m_size += archiveHeader.size();
    m_justCreated = true;
  }
  if (m_newThreads.empty() && m_order == OLDEST_SECTION_FIRST) {
    m_size += 2;  // For the "\n\n" before the first new thread.
  }
  m_newThreads.push_back(thread.text());
  m_size += thread.text().size();
  const Date& threadDate = thread.date().localDate();
  if (insertDatesInHeader) {
    if (m_justCreated && (m_oldestAddedThread.isNull() || m_oldestAddedThread > threadDate)) {
      m_oldestAddedThread = threadDate;
    }
    m_newestAddedThread = std::max(m_newestAddedThread, threadDate);
  }
  m_numThreads++;
}

void ArchivePage::update(Wiki* wiki, const string& sourcePage, bool dryRun) const {
  string sectionCount =
      m_numThreads > 1 ? cbl::concat("de ", std::to_string(m_numThreads), " sections") : "d'une section";
  string editSummary = cbl::concat("Archivage ", sectionCount, " provenant de [[", sourcePage, "]]");
  if (dryRun) {
    CBL_INFO << "[DRY RUN] Writing '" << m_title << "' with comment '" << editSummary << "'";
    return;
  }
  wiki->editPage(
      m_title,
      [&](string& content, string& summary) {
        summary = editSummary;
        if (content.empty()) {
          content = m_newHeader;
        }
        if (!m_newestAddedThread.isNull()) {
          tryToUpdateDatesInHeader(*wiki, content, m_oldestAddedThread, m_newestAddedThread);
        }
        switch (m_order) {
          case OLDEST_SECTION_FIRST: {
            content += "\n\n";
            for (const string& thread : m_newThreads) {
              content += thread;
            }
            break;
          }
          case NEWEST_SECTION_FIRST: {
            size_t insertionPoint;
            if (content.empty() || content.starts_with("=")) {
              insertionPoint = 0;
            } else {
              size_t firstSection = content.find("\n=");
              if (firstSection == string::npos) {
                content += "\n\n";
                insertionPoint = content.size();
              } else {
                insertionPoint = firstSection + 1;
              }
            }
            string newContent;
            newContent.append(content, 0, insertionPoint);
            for (auto threadIt = m_newThreads.rbegin(); threadIt != m_newThreads.rend(); ++threadIt) {
              newContent += *threadIt;
            }
            newContent.append(content, insertionPoint, content.size() - insertionPoint);
            content = std::move(newContent);
            break;
          }
        }
      },
      mwc::EDIT_MINOR | mwc::EDIT_BYPASS_NOBOTS);
}

class ArchivePagesBuffer {
public:
  ArchivePagesBuffer(Wiki* wiki, const string& pattern, int counter, ArchiveOrder archiveOrder);
  void addThread(const Thread& thread, const ArchiveParams& params);
  int counter() const { return m_counter; }
  bool useCounter() const { return m_useCounter; }
  const vector<const ArchivePage*>& usedArchivePages() const { return m_usedArchivePages; }

private:
  ArchivePage* loadArchivePage(const string& title);
  ArchivePage* loadArchivePageByIndex(int index);
  void goToLastArchive();
  void initializeCounter();

  Wiki* m_wiki;
  string m_pattern;
  int m_counter = -1;
  ArchiveOrder m_archiveOrder;
  bool m_counterInitialized = false;
  bool m_useCounter = false;
  map<string, unique_ptr<ArchivePage>> m_archivePages;
  set<string> m_usedArchivePagesSet;
  vector<const ArchivePage*> m_usedArchivePages;
};

ArchivePagesBuffer::ArchivePagesBuffer(Wiki* wiki, const string& pattern, int counter, ArchiveOrder archiveOrder)
    : m_wiki(wiki), m_pattern(pattern), m_counter(counter), m_archiveOrder(archiveOrder) {
  m_useCounter = pattern.find("%(counter)d") != string::npos;
}

void ArchivePagesBuffer::addThread(const Thread& thread, const ArchiveParams& params) {
  ArchivePage* archivePage = nullptr;
  if (m_useCounter) {
    initializeCounter();
    const int maxSize = params.maxarchivesize() > 0 ? params.maxarchivesize() * 1000 : 500 * 1000;
    for (;; m_counter++) {
      archivePage = loadArchivePageByIndex(m_counter);
      if (archivePage->size() < maxSize) break;
    }
  } else {
    // Compute the archive page based on the local time because it is the less surprising behavior. For instance, if the
    // last message contains "1 janvier 2010 à 00:04 (CET)", the UTC date is 2009-12-31T23:04:00Z but the thread should
    // be archived to /2010, not /2009.
    Date localDate = thread.date().localDate();
    int month = localDate.month();
    string archiveTitle = m_pattern;
    cbl::replaceInPlace(archiveTitle, "%(year)d", std::to_string(localDate.year()));
    cbl::replaceInPlace(archiveTitle, "%(month)d", std::to_string(month));
    cbl::replaceInPlace(archiveTitle, "%(month)02d", padWithZeros(month, 2));
    cbl::replaceInPlace(archiveTitle, "%(monthname)s", DateFormatter::getByLang("fr").getMonthName(month));
    cbl::replaceInPlace(archiveTitle, "%(quarter)d", std::to_string((month - 1) / 3 + 1));
    if (archiveTitle.find("%(monthnameshort)s") != string::npos) {
      throw ArchiverError("'%(monthnameshort)s' is not supported");
    }
    archivePage = loadArchivePage(archiveTitle);
  }
  if (m_usedArchivePagesSet.count(archivePage->title()) == 0) {
    m_usedArchivePagesSet.insert(archivePage->title());
    m_usedArchivePages.push_back(archivePage);
  }
  archivePage->addThread(thread, params.archiveheader(), params.hasAutoArchiveHeader() && m_useCounter);
}

ArchivePage* ArchivePagesBuffer::loadArchivePage(const string& title) {
  unique_ptr<ArchivePage>& archivePageUniquePtr = m_archivePages[title];
  if (!archivePageUniquePtr) {
    archivePageUniquePtr = std::make_unique<ArchivePage>(title, m_archiveOrder);
    archivePageUniquePtr->load(m_wiki);
  }
  return archivePageUniquePtr.get();
}

ArchivePage* ArchivePagesBuffer::loadArchivePageByIndex(int index) {
  string title = replaceCounter(m_pattern, index);
  if (m_pattern == title) {
    throw ArchiverError(
        "Internal error: ArchivePagesBuffer::loadArchivePageByIndex called with a pattern that does "
        "not depend on a counter");
  }
  return loadArchivePage(title);
}

void ArchivePagesBuffer::goToLastArchive() {
  int searchMin = 1, searchMax = INT_MAX;
  CBL_INFO << "Computing the last archive for '" << m_pattern << "'";
  while (searchMin < searchMax) {
    int index;
    if (searchMax == INT_MAX) {
      index = searchMin * 2;
    } else {
      index = (searchMin + searchMax + 1) / 2;  // searchMin < index <= searchMax
    }
    ArchivePage* archivePage = loadArchivePageByIndex(index);
    if (archivePage->size() == 0) {
      searchMax = index - 1;
    } else {
      searchMin = index;
    }
  }
  m_counter = searchMin;
  CBL_INFO << "Last archive: " << m_counter;
}

void ArchivePagesBuffer::initializeCounter() {
  if (m_counterInitialized || !m_useCounter) {
    return;
  }
  if (m_counter < 1) {
    // If the counter is not defined yet (or not valid), directly goes to the last non-empty archive.
    // It is indeed possible that some manual archiving has been done already to pages following the same pattern.
    CBL_INFO << "Counter is undefined";
    goToLastArchive();
  } else if (m_counter > 1) {
    ArchivePage* archivePage = loadArchivePageByIndex(m_counter);
    if (m_counter > 1 && archivePage->size() == 0) {
      ArchivePage* previousArchivePage = loadArchivePageByIndex(m_counter - 1);
      if (previousArchivePage->size() == 0) {
        // There was probably a copy-and-paste of the template from a different page. Let us fix this.
        CBL_INFO << "The counter is past the last existing archive";
        goToLastArchive();
      }
    }
  }
  m_counterInitialized = true;
}

string generateEditSummary(const vector<Thread*>& threads, const vector<const ArchivePage*>& usedArchivePages) {
  int numThreadsArchived = 0;
  int numThreadsErased = 0;
  int ageLowerBound = INT_MAX;
  int ageUpperBound = 0;
  for (const Thread* thread : threads) {
    if (thread->state() == ThreadState::ARCHIVED) {
      numThreadsArchived++;
    } else if (thread->state() == ThreadState::ERASED) {
      numThreadsErased++;
    }
    if (thread->state() == ThreadState::ARCHIVED || thread->state() == ThreadState::ERASED) {
      ageLowerBound = std::min(ageLowerBound, thread->algoMaxAgeInDays());
      ageUpperBound = std::max(ageUpperBound, thread->algoMaxAgeInDays());
    }
  }
  const int numThreadsArchivedOrErased = numThreadsArchived + numThreadsErased;

  string sourcePart;
  if (numThreadsArchivedOrErased > 1) {
    cbl::append(sourcePart, "de ", std::to_string(numThreadsArchivedOrErased), " sections");
  } else {
    sourcePart += "d'une section";
  }
  if (ageUpperBound > 0) {
    sourcePart += (numThreadsArchivedOrErased > 1 ? " non modifiées depuis " : " non modifiée depuis ");
    if (ageLowerBound < ageUpperBound) {
      cbl::append(sourcePart, std::to_string(ageLowerBound), " à ");
    }
    sourcePart += std::to_string(ageUpperBound);
    sourcePart += (ageUpperBound > 1 ? " jours" : " jour");
  }

  string targetPart;
  if (!usedArchivePages.empty()) {
    cbl::append(targetPart, "vers [[", usedArchivePages[0]->title(), "]]");
    if (usedArchivePages.size() == 2) {
      cbl::append(targetPart, " et [[", usedArchivePages[1]->title(), "]]");
    } else if (usedArchivePages.size() > 2) {
      cbl::append(targetPart, " et ", std::to_string(usedArchivePages.size() - 1), " autres pages");
    }
  }

  string editSummary;
  if (numThreadsErased == 0) {
    editSummary = cbl::concat("Archivage ", sourcePart, " ", targetPart);
  } else if (numThreadsArchived == 0) {
    editSummary = "Effacement " + sourcePart;
  } else {
    editSummary = cbl::concat("Effacement ou archivage ", targetPart, " ", sourcePart);
  }
  return editSummary;
}

}  // namespace

Archiver::Archiver(Wiki* wiki, const string& dataDir, const string& keyPrefixFile, bool dryRun)
    : m_wiki(wiki), m_dataDir(dataDir), m_dryRun(dryRun), m_algorithms(getFrwikiAlgorithms()) {
  if (!keyPrefixFile.empty()) {
    m_keyPrefix = string(cbl::trim(cbl::readFile(keyPrefixFile)));
  }
}

void Archiver::checkArchiveName(const string& title, const string& archive, const string& rawArchive,
                                const string& key) {
  mwc::TitleParts titleParts = m_wiki->parseTitle(title);
  mwc::TitleParts archiveParts = m_wiki->parseTitle(archive);

  switch (titleParts.namespaceNumber) {
    case mwc::NS_MAIN:
    case mwc::NS_FILE:
    case mwc::NS_TEMPLATE:
    case mwc::NS_HELP:
    case mwc::NS_CATEGORY:
      throw ArchiverError(cbl::concat("Page '", title, "' is in a namespace where archiving is disabled"));
    default:
      break;
  }

  string_view unprefixedTitle = titleParts.unprefixedTitle();
  string_view archiveUnprefixedTitle = archiveParts.unprefixedTitle();
  bool isSubPage = titleParts.namespaceNumber == archiveParts.namespaceNumber &&
                   archiveUnprefixedTitle.starts_with(cbl::concat(unprefixedTitle, "/"));
  if (!isSubPage) {
    throw ArchiverError(cbl::concat("The archive page '", archive, "' is not a subpage of '", title, "'"));
  }
  static const re2::RE2 reArchiveTitle("/.*[Aa]rchiv");
  if (RE2::PartialMatch(title, reArchiveTitle)) {
    throw ArchiverError(
        cbl::concat("Page '", title, "' cannot be archived because its name indicates that it is an archive"));
  }
}

void Archiver::updateCounterInCode(string& wcode, int newValue) {
  wikicode::List parsedCode = wikicode::parse(wcode);
  wikicode::Template* archiveTemplate = findArchiveTemplate(*m_wiki, parsedCode);
  if (archiveTemplate == nullptr) {
    CBL_ERROR << "Cannot update counter after archiving because the template was not found";
    return;
  }

  int counterIndex = archiveTemplate->getParsedFields().indexOf("counter");
  if (counterIndex != wikicode::FIND_PARAM_NONE) {
    archiveTemplate->setFieldValue(counterIndex, std::to_string(newValue));
  } else {
    bool singleLine = archiveTemplate->toString().find('\n') == string::npos;
    archiveTemplate->addField(cbl::concat("counter=", std::to_string(newValue), singleLine ? "" : "\n"));
  }

  wcode = parsedCode.toString();
}

void Archiver::archivePageWithCode(const string& title, const ArchiveParams& params, const mwc::WriteToken& writeToken,
                                   const string& wcode, bool& inStableState) {
  checkArchiveName(title, params.archive(), params.rawArchive(), params.key());

  PageToArchive pageToArchive;
  pageToArchive.load(m_wiki, wcode);
  if (pageToArchive.hasTrackingTemplate()) {
    if (params.algorithms().size() != 1 || params.algorithms()[0].algorithm->name() != "oldtitle") {
      throw ArchiverError("Archiving a page with tracking template with an algorithm different from 'oldtitle'");
    }
  }

  unique_ptr<HistoryCache> historyCache;
  // HistoryCache is not needed for category tracking templates. If it were needed, the parsing logic for those
  // templates should be added to HistoryCache (this is currently not implemented).
  if (!pageToArchive.hasTrackingTemplate()) {
    historyCache = std::make_unique<HistoryCache>(m_wiki, title);
  }
  const Date now = Date::now();
  for (Thread* thread : pageToArchive.reorderedThreads()) {
    thread->computeState(m_wiki, now, params.algorithms(), historyCache.get());
  }

  int numThreadsToArchiveOrDelete = 0;
  int numThreadsLeft = 0;
  for (const Thread* thread : pageToArchive.reorderedThreads()) {
    if (thread->state() == ThreadState::ARCHIVABLE || thread->state() == ThreadState::ERASABLE) {
      numThreadsToArchiveOrDelete++;
    }
    if (thread->state() != ThreadState::NEVER_ARCHIVABLE_BECAUSE_OF_TITLE_LEVEL) {
      numThreadsLeft++;
    }
  }

  int minThreadsLeft = params.minthreadsleft();
  if (minThreadsLeft == ARCHIVE_PARAM_NOT_SET) {
    minThreadsLeft = pageToArchive.hasTrackingTemplate() ? 1 : DEF_MIN_THREADS_LEFT;
  }

  int minThreadsToArchive = params.minthreadstoarchive();
  if (minThreadsToArchive == ARCHIVE_PARAM_NOT_SET) {
    minThreadsToArchive = pageToArchive.hasTrackingTemplate() ? 1 : DEF_MIN_THREADS_TO_ARCHIVE;
  }
  minThreadsToArchive = std::max(minThreadsToArchive, 1);

  inStableState = numThreadsLeft < minThreadsLeft + minThreadsToArchive;
  if (inStableState) {
    CBL_INFO << "Only " << numThreadsLeft << " < " << minThreadsLeft << " + " << minThreadsToArchive
             << " threads on the page";
    return;
  } else if (numThreadsToArchiveOrDelete == 0) {
    CBL_INFO << "No thread to archive";
    return;
  } else if (numThreadsToArchiveOrDelete < minThreadsToArchive) {
    CBL_INFO << "Only " << numThreadsToArchiveOrDelete << " < " << minThreadsToArchive << " threads to archive";
    return;
  }

  const ArchiveOrder archiveOrder = pageToArchive.hasTrackingTemplate() ? NEWEST_SECTION_FIRST : OLDEST_SECTION_FIRST;
  ArchivePagesBuffer archivePagesBuffer(m_wiki, params.archive(), params.counter(), archiveOrder);
  bool changeDone = false;
  for (Thread* thread : pageToArchive.reorderedThreads()) {
    if (numThreadsLeft <= minThreadsLeft) break;
    if (thread->state() == ThreadState::ARCHIVABLE) {
      archivePagesBuffer.addThread(*thread, params);
      thread->setState(ThreadState::ARCHIVED);
    } else if (thread->state() == ThreadState::ERASABLE) {
      thread->setState(ThreadState::ERASED);
    }
    if (thread->state() == ThreadState::ARCHIVED || thread->state() == ThreadState::ERASED) {
      numThreadsLeft--;
      changeDone = true;
    }
  }
  string newCode = pageToArchive.generateCode();
  if (!changeDone) {
    throw ArchiverError(
        "Internal error: expected to find at least one thread to archive or delete, but none was found");
  }

  const vector<const ArchivePage*>& usedArchivePages = archivePagesBuffer.usedArchivePages();
  for (const ArchivePage* archivePage : usedArchivePages) {
    // This is the worst place to fail: we don't know if that write actually failed, and other archive pages may already
    // have been written, so the next attempt may archive the same content again.
    archivePage->update(m_wiki, title, m_dryRun);
  }

  string editSummary = generateEditSummary(pageToArchive.reorderedThreads(), usedArchivePages);
  if (archivePagesBuffer.useCounter() && archivePagesBuffer.counter() != -1) {
    updateCounterInCode(newCode, archivePagesBuffer.counter());
  }
  if (m_dryRun) {
    CBL_INFO << "[DRY RUN] Writing '" << title << "' with comment '" << editSummary << "'";
  } else {
    // Bypasses {{nobots}} because the bot is called by the presence of a template on the page.
    m_wiki->writePage(title, newCode, writeToken, editSummary, mwc::EDIT_MINOR | mwc::EDIT_BYPASS_NOBOTS);
  }

  // Pattern where a short subpage is archived to its parent page containing the full list.
  // Example: https://fr.wikipedia.org/w/index.php?title=Projet:Football/Articles_r%C3%A9cents&oldid=166990970
  if (title.starts_with(params.archive() + "/")) {
    CBL_INFO << "Purging '" << params.archive() << "'";
    if (!m_dryRun) {
      try {
        m_wiki->purgePage(params.archive());
      } catch (const mwc::WikiError& error) {
        CBL_WARNING << error.what();
      }
    }
  }
}

void Archiver::archivePage(const string& title) {
  CBL_INFO << "Archiving '" << title << "'";
  mwc::WriteToken writeToken;
  Revision revision = m_wiki->readPage(title, mwc::RP_CONTENT | mwc::RP_REVID, &writeToken);
  wikicode::List parsedCode = wikicode::parse(revision.content);
  optional<ArchiveParams> params;
  try {
    params.emplace(*m_wiki, m_algorithms, title, parsedCode);
  } catch (const ParamsInitializationError& error) {
    throw ArchiverError(error.what());
  }
  bool inStableState = false;
  archivePageWithCode(title, *params, writeToken, revision.content, inStableState);
  if (inStableState) {
    m_stableRevids.insert(revision.revid);
  }
}

void Archiver::archivePages(const vector<string>& pages) {
  for (const string& page : pages) {
    try {
      archivePage(page);
    } catch (const mwc::WikiError& error) {
      CBL_ERROR << "Failed to archive '" << page << "': " << error.what();
    } catch (const ArchiverError& error) {
      CBL_ERROR << "Failed to archive '" << page << "': " << error.what();
    }
  }
}

void Archiver::archiveAll() {
  string revidsFile = cbl::joinPaths(m_dataDir, "stable_revids.txt");
  set<revid_t> oldStableRevids = loadStableRevids(revidsFile);

  CBL_INFO << "Reading transclusions of {{" << ARCHIVE_TEMPLATE_NAME << "}}";
  vector<string> pages = m_wiki->getTransclusions(cbl::concat("Template:", ARCHIVE_TEMPLATE_NAME));
  vector<string> pagesToUpdate;
  filterStablePages(*m_wiki, pages, oldStableRevids, pagesToUpdate, m_stableRevids);
  archivePages(pagesToUpdate);

  if (!m_dryRun) {
    saveStableRevids(revidsFile, m_stableRevids);
  }
}

bool extractTrackingTemplate(Wiki* wiki, const string& code, unique_ptr<wikicode::Template>& trackingTemplate,
                             string& codeInTemplate, string& header, string& footer) {
  const string TRACKING_TEMPLATE_PLACEHOLDER = "<~~~SUIVI-CATEGORIE-MARQUEUR-ARCHIVAGE~~~>";
  wikicode::List parsedCode = wikicode::parse(code);
  for (wikicode::List& list : parsedCode.getLists()) {
    int trackingTemplateIndex = -1;
    for (int listIndex = 0; listIndex < list.size(); listIndex++) {
      if (list[listIndex].type() == wikicode::NT_TEMPLATE) {
        wikicode::Template& template_ = list[listIndex].asTemplate();
        string templateName = wiki->normalizeTitle(template_.name(), mwc::NS_TEMPLATE);
        if (templateName == "Utilisateur:OrlodrimBot/Suivi catégorie") {
          if (trackingTemplateIndex != -1) break;
          wikicode::ParsedFields parsedFields = template_.getParsedFields();
          // TODO: Check the value of this parameter.
          // string type = parsedFields.getWithDefault("type", "ajout");
          string formatSections = parsedFields["format sections"];
          if (!formatSections.empty() && formatSections != "-") {
            trackingTemplateIndex = listIndex;
            trackingTemplate = template_.copy();
            codeInTemplate.clear();
            list.setItem(listIndex, TRACKING_TEMPLATE_PLACEHOLDER);
          }
        } else if (templateName == "Utilisateur:OrlodrimBot/Suivi catégorie/fin") {
          if (trackingTemplateIndex != -1) {
            for (int i = trackingTemplateIndex + 1; i < listIndex; i++) {
              list[i].addToBuffer(codeInTemplate);
              list.setItem(i, "");
            }
            break;
          }
        }
      }
    }
    if (trackingTemplateIndex != -1) {
      string codeWithPlaceholder = parsedCode.toString();
      const size_t templateStart = codeWithPlaceholder.find(TRACKING_TEMPLATE_PLACEHOLDER);
      CBL_ASSERT(templateStart != string::npos);
      header = codeWithPlaceholder.substr(0, templateStart);
      footer = codeWithPlaceholder.substr(templateStart + TRACKING_TEMPLATE_PLACEHOLDER.size());
      return true;
    }
  }
  return false;
}

}  // namespace talk_page_archiver
