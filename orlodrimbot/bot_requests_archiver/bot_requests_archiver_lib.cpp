#include "bot_requests_archiver_lib.h"
#include <cstdio>
#include <string>
#include <string_view>
#include "cbl/date.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "mwclient/parser.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/wikiutil/date_parser.h"

using cbl::Date;
using cbl::DateDiff;
using std::string;
using std::string_view;
using wikicode::getTitleLevel;
using wikiutil::DateParser;

namespace {

constexpr const char* REQUESTS_ROOT = "Wikipédia:Bot/Requêtes/";
constexpr const char* REQUESTS_ARCHIVES_ROOT = "Wikipédia:Bot/Requêtes/Archives/";
constexpr const char* BOT_PAGE_HEADER = "<noinclude>{{Wikipédia:Bot/Navig}}</noinclude>";

void splitRequests(const string& code, bool archiveAll, string& currentRequests, string& archivedRequests,
                   int& numCurrent, int& numToArchive) {
  constexpr string_view magicTokenOfLineWithDate = "|→ ici ←]";
  const DateParser& dateParser = DateParser::getByLang("fr");
  // Possible values of state:
  // 0: In the header of the page, before the first request.
  // 1: In a request that will stay on the page, unless we find a line with magicTokenOfLineWithDate later.
  // 2: In a request that will be moved the the archive.
  int state = 0;
  currentRequests.clear();
  archivedRequests.clear();
  numCurrent = 0;
  numToArchive = 0;
  const char* beginningOfSection = code.c_str();

  auto writeCurrentSection = [&](const char* endOfSection) {
    string& target = state == 2 ? archivedRequests : currentRequests;
    target.append(beginningOfSection, endOfSection - beginningOfSection);
    if (state == 1) {
      numCurrent++;
    } else if (state == 2) {
      numToArchive++;
    }
    beginningOfSection = endOfSection;
  };

  for (string_view line : cbl::splitLines(code)) {
    int titleLevel = getTitleLevel(line);
    if (titleLevel > 0 && titleLevel <= 2) {
      writeCurrentSection(line.data());
      state = archiveAll ? 2 : 1;
    } else if (state == 1 && line.find(magicTokenOfLineWithDate) != string_view::npos) {
      Date dateArch = dateParser.extractFirstDate(line, DateParser::AFTER_2000);
      if (!dateArch.isNull() && dateArch < Date::now() + DateDiff::fromDays(1)) {
        state = 2;
      }
    }
  }
  writeCurrentSection(code.c_str() + code.size());
}

}  // namespace

string YearMonth::toString() const {
  constexpr int EXTRA_SNPRINTF_MARGIN = 20;
  char buffer[8 + EXTRA_SNPRINTF_MARGIN];
  snprintf(buffer, sizeof(buffer), "%04i/%02i", m_value / 12, m_value % 12 + 1);
  return buffer;
}

void BotRequestsArchiver::initPage(YearMonth yearMonth) {
  for (const string& title : {REQUESTS_ROOT + yearMonth.toString(), REQUESTS_ARCHIVES_ROOT + yearMonth.toString()}) {
    try {
      CBL_INFO << "Creating '" << title << "'";
      if (!m_dryRun) {
        m_wiki->writePage(title, BOT_PAGE_HEADER, mwc::WriteToken::newForCreation(), "Initialisation");
      }
    } catch (const mwc::PageAlreadyExistsError&) {
      // Keep going.
    } catch (const mwc::WikiError& error) {
      CBL_ERROR << error.what();
    }
  }
}

void BotRequestsArchiver::archiveMonth(YearMonth yearMonth, bool archiveAll, RedirectToArchive canRedirectToArchive) {
  const string titleSuffix = yearMonth.toString();
  const string title = REQUESTS_ROOT + titleSuffix;
  const string archiveTitle = REQUESTS_ARCHIVES_ROOT + titleSuffix;

  int numToArchive = 0;
  string commentBase;
  string archivedRequests;

  CBL_INFO << "Reading '" << title << "'";
  m_wiki->editPage(
      title,
      [&](string& content, string& summary) {
        numToArchive = 0;
        if (content.empty()) {
          // The page does not exist (or it was intentionally blanked).
          return;
        }

        // This code may run multiple times in case of edit conflict, so it is important that splitRequests does not
        // depend on any output parameter being initialized to a specific value.
        string currentRequests;
        int numCurrent = 0;
        splitRequests(content, archiveAll, currentRequests, archivedRequests, numCurrent, numToArchive);

        bool redirectToArchive = false;
        if (numCurrent == 0 && !m_wiki->readRedirect(currentRequests, nullptr, nullptr)) {
          switch (canRedirectToArchive) {
            case RedirectToArchive::NO:
              break;
            case RedirectToArchive::IF_CHANGED:
              redirectToArchive = numToArchive > 0;
              break;
            case RedirectToArchive::YES:
              redirectToArchive = true;
              break;
          }
        }
        if (numToArchive == 0 && !redirectToArchive) {
          CBL_INFO << "No request to archive";
          return;
        }
        commentBase = numToArchive == 1 ? "Archivage d'une requête"
                                        : cbl::concat("Archivage de ", std::to_string(numToArchive), " requêtes");
        if (redirectToArchive) {
          CBL_INFO << "Redirecting '" << title << "' to its archive page";
          currentRequests = cbl::concat("#REDIRECTION [[", archiveTitle, "]]");
          if (numToArchive > 0) {
            summary = cbl::concat(commentBase, " et transformation en redirection vers la page d'archives [[",
                                  archiveTitle, "]]");
          } else {
            summary = cbl::concat("Page redirigée vers [[", archiveTitle, "]]");
          }
        } else {
          summary = cbl::concat(commentBase, " vers [[", archiveTitle, "]]");
        }
        CBL_INFO << "Writing '" << title << "' with comment '" << summary << "'";
        if (!m_dryRun) {
          content = std::move(currentRequests);
        }
      },
      mwc::EDIT_MINOR);

  if (numToArchive > 0) {
    m_wiki->editPage(
        archiveTitle,
        [&](string& content, string& summary) {
          summary = cbl::concat(commentBase, " depuis [[", title, "]]");
          CBL_INFO << "Writing '" << archiveTitle << "' with comment '" << summary << "'";
          if (!m_dryRun) {
            cbl::append(content, content.empty() ? BOT_PAGE_HEADER : "", "\n\n", archivedRequests);
          }
        },
        mwc::EDIT_MINOR);
  }
}

void BotRequestsArchiver::run(bool forceNewMonth) {
  // The pages for a new month are initialized on the last day of the previous month at 23:00 UTC+1 or 23:00 UTC+2, so
  // we need to take the date at least 3 hours in the future.
  Date baseDate = Date::now() + DateDiff::fromHours(4);
  YearMonth baseMonth(baseDate);
  bool newMonth = baseDate.day() == 1 || forceNewMonth;

  if (newMonth) {
    try {
      archiveMonth(baseMonth - 13, /* archiveAll = */ true, /* canRedirectToArchive = */ RedirectToArchive::IF_CHANGED);
    } catch (const mwc::WikiError& error) {
      CBL_ERROR << error.what();
    }
  }

  for (int i = -12; i <= 0; i++) {
    if (i == 0 && newMonth) {
      initPage(baseMonth);
    } else {
      // Never redirect the page of the current month to the archive.
      RedirectToArchive canRedirectToArchive = RedirectToArchive::NO;
      if (i == -1 && newMonth) {
        // On the last day of the month, if no requests are left on the page, redirect it to the archive
        // (even if no other change happens because all requests were already archived).
        canRedirectToArchive = RedirectToArchive::YES;
      } else if (i < 0) {
        // Redirect pages of older months to the archive only if a request is archived (do not fight against
        // a human who would have changed it).
        canRedirectToArchive = RedirectToArchive::IF_CHANGED;
      }
      try {
        archiveMonth(baseMonth + i, /* archiveAll = */ false, canRedirectToArchive);
      } catch (const mwc::WikiError& error) {
        CBL_ERROR << error.what();
      }
    }
  }
}
