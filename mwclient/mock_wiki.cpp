#include "mock_wiki.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include "cbl/date.h"
#include "cbl/json.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "site_info.h"
#include "titles_util.h"
#include "wiki.h"
#include "wiki_defs.h"

using cbl::Date;
using std::string;
using std::unordered_map;
using std::vector;

namespace mwc {
namespace {

void partialRevisionCopy(const Revision& source, int properties, Revision& target) {
  target.title = (properties & RP_TITLE) ? source.title : "";
  target.revid = (properties & RP_REVID) ? source.revid : 0;
  target.minor_ = (properties & RP_MINOR) ? source.minor_ : false;
  target.timestamp = (properties & RP_TIMESTAMP) ? source.timestamp : Date();
  target.user = (properties & RP_USER) ? source.user : "";
  target.size = (properties & RP_SIZE) ? source.size : 0;
  target.comment = (properties & RP_COMMENT) ? source.comment : "";
  target.contentHidden = (properties & (RP_CONTENT | RP_SHA1)) ? source.contentHidden : false;
  if (source.contentHidden) {
    target.content.clear();
    target.sha1.clear();
  } else {
    target.content = (properties & RP_CONTENT) ? source.content : "";
    target.sha1 = (properties & RP_SHA1) ? "sha1:" + source.content : "";
  }
  target.contentModel = (properties & RP_CONTENT_MODEL) ? RCM_WIKITEXT : RCM_INVALID;
}

const PageProtection* getProtectionByType(const vector<PageProtection>& protections, PageProtectionType type) {
  for (const PageProtection& protection : protections) {
    if (protection.type == type) {
      return &protection;
    }
  }
  return nullptr;
}

}  // namespace

MockWiki::MockWiki() : m_nextRevid(1), m_verboseWrite(false) {
  m_wikiURL = "http://invalid/mockwiki";
  setInternalUserName("MockUser");
  m_siteInfo.fromJSONValue(json::parse(R"({
    "aliases": {
      "special": -1, "spécial": -1,
      "": 0,
      "discussion": 1, "talk": 1,
      "utilisateur": 2, "utilisatrice": 2, "user": 2,
      "discussion utilisateur": 3, "discussion utilisatrice": 3, "user talk": 3,
      "wikipédia": 4, "wikipedia": 4, "wp": 4, "project": 4,
      "discussion wikipédia": 5, "wikipedia talk": 5,
      "fichier": 6, "image": 6, "file": 6,
      "discussion fichier": 7, "discussion image": 7, "file talk": 7,
      "mediawiki": 8,
      "discussion mediawiki": 9, "mediawiki talk": 9,
      "modèle": 10, "template": 10,
      "discussion modèle": 11, "template talk": 11,
      "aide": 12, "help": 12,
      "discussion aide": 13, "help talk": 13,
      "catégorie": 14, "category": 14,
      "discussion catégorie": 15, "category talk": 15,
      "portail": 100,
      "discussion portail": 101,
      "projet": 102,
      "discussion projet": 103,
      "module": 828,
      "discussion module": 829,
      "sujet": 2600
    },
    "interwikis": {
      "en": { "lang": "English" },
      "mw": {}
    },
    "namespaces": {
      "Spécial": { "casemode": 1, "number": -1 },
      "": { "casemode": 1, "number": 0 },
      "Discussion": { "casemode": 1, "number": 1 },
      "Utilisateur": { "casemode": 1, "number": 2 },
      "Discussion utilisateur": { "casemode": 1, "number": 3 },
      "Wikipédia": { "casemode": 1, "number": 4 },
      "Discussion Wikipédia": { "casemode": 1, "number": 5 },
      "Fichier": { "casemode": 1, "number": 6 },
      "Discussion fichier": { "casemode": 1, "number": 7 },
      "MediaWiki": { "casemode": 1, "number": 8 },
      "Discussion MediaWiki": { "casemode": 1, "number": 9 },
      "Modèle": { "casemode": 1, "number": 10 },
      "Discussion modèle": { "casemode": 1, "number": 11 },
      "Aide": { "casemode": 1, "number": 12 },
      "Discussion aide": { "casemode": 1, "number": 13 },
      "Catégorie": { "casemode": 1, "number": 14 },
      "Discussion catégorie": { "casemode": 1, "number": 15 },
      "Portail": { "casemode": 1, "number": 100 },
      "Discussion Portail": { "casemode": 1, "number": 101 },
      "Projet": { "casemode": 1, "number": 102 },
      "Discussion Projet": { "casemode": 1, "number": 103 },
      "Module": { "casemode": 1, "number": 828 },
      "Discussion module": { "casemode": 1, "number": 829 },
      "Sujet": { "casemode": 1, "number": 2600 }
    },
    "redirect-aliases": [
      "#redirect",
      "#redirection"
    ],
    "siteinfo_version": 1
  })"));
}

Revision MockWiki::readPage(const std::string& title, int properties) {
  const Page& page = getPage(title);
  if (page.revisions.empty()) {
    throw PageNotFoundError(cbl::concat("Page '", title, "' not found"));
  }
  Revision revision;
  partialRevisionCopy(m_revisions[page.revisions.back()], properties, revision);
  return revision;
}

void MockWiki::readPages(int properties, vector<Revision>& revisions, int flags) {
  for (Revision& revision : revisions) {
    string unnormalizedTitle = revision.title;
    const Page* page = &getPage(unnormalizedTitle);
    string redirectTarget;
    if ((flags & READ_RESOLVE_REDIRECTS) && !page->revisions.empty() &&
        readRedirect(m_revisions[page->revisions.back()].content, &redirectTarget, nullptr)) {
      page = &getPage(redirectTarget);
    }
    if (page->revisions.empty()) {
      revision.revid = -1;
    } else {
      partialRevisionCopy(m_revisions[page->revisions.back()], properties, revision);
      if (!(properties & RP_TITLE)) {
        revision.title = unnormalizedTitle;
      }
      if (!(properties & RP_REVID)) {
        revision.revid = 0;
      }
    }
  }
}

Revision MockWiki::readRevision(revid_t revid, int properties) {
  if (m_revisions.count(revid) == 0) {
    throw PageNotFoundError("revid=" + std::to_string(revid));
  }
  Revision revision;
  partialRevisionCopy(m_revisions[revid], properties, revision);
  return revision;
}

void MockWiki::readRevisions(int properties, vector<Revision>& revisions) {
  for (Revision& revision : revisions) {
    if (m_revisions.count(revision.revid) == 0) {
      revision.title = INVALID_TITLE;
    } else {
      partialRevisionCopy(m_revisions.at(revision.revid), properties | RP_REVID, revision);
    }
  }
}

vector<Revision> MockWiki::getHistory(const HistoryParams& params) {
  CBL_ASSERT(params.startId == 0 || params.direction == NEWEST_FIRST);
  CBL_ASSERT(params.endId == 0);
  // CBL_ASSERT(params.direction == NEWEST_FIRST);
  const Page& page = getPage(params.title);
  if (page.revisions.empty()) {
    throw PageNotFoundError("title=" + params.title);
  }
  if (params.nextQueryContinue) {
    params.nextQueryContinue->clear();
  }
  int numRevisions = page.revisions.size();
  int limit = params.limit == PAGER_ALL ? numRevisions : params.limit;

  int startIndex = numRevisions - 1;
  int endIndex = 0;
  int delta = -1;
  Date maxTimestamp = params.start;
  Date minTimestamp = params.end;
  if (params.direction == OLDEST_FIRST) {
    std::swap(startIndex, endIndex);
    delta = -delta;
    std::swap(maxTimestamp, minTimestamp);
  }
  bool afterStartId = true;
  if (!params.queryContinue.empty()) {
    CBL_ASSERT(params.queryContinue.starts_with("I"));
    startIndex = atoi(params.queryContinue.c_str() + 1);
    CBL_ASSERT(startIndex >= 0 && startIndex < numRevisions);
  } else {
    afterStartId = params.startId == 0;
  }
  vector<Revision> revisions;
  for (int i = startIndex; i != endIndex + delta; i += delta) {
    if (limit <= 0) {
      if (params.nextQueryContinue) {
        *params.nextQueryContinue = "I" + std::to_string(i);
      }
      break;
    }
    const Revision& rev = m_revisions[page.revisions[i]];
    if (!afterStartId) {
      if (rev.revid != params.startId) continue;
      afterStartId = true;
    }
    if (!maxTimestamp.isNull() && rev.timestamp > maxTimestamp) continue;
    if (!minTimestamp.isNull() && rev.timestamp < minTimestamp) continue;
    revisions.emplace_back();
    partialRevisionCopy(rev, params.prop, revisions.back());
    limit--;
  }
  return revisions;
}

unordered_map<string, vector<PageProtection>> MockWiki::getPagesProtections(const vector<string>& titles) {
  unordered_map<string, vector<PageProtection>> pagesProtection;
  for (const string& title : titles) {
    pagesProtection[title] = getPage(title).protections;
  }
  return pagesProtection;
}

vector<string> MockWiki::getTransclusions(const string& title) {
  TitleParts titleParts = parseTitle(title);
  vector<string> pages;
  for (const auto& [pageTitle, page] : m_pages) {
    const vector<revid_t>& revisions = page.revisions;
    if (revisions.empty()) continue;
    const string& content = readRevisionContent(revisions.back());
    int position = 0;
    while (true) {
      int start = content.find("{{", position);
      if (start == -1) break;
      start += 2;
      int end = content.find_first_of("|}", start);
      if (end == -1) break;
      string templateName = content.substr(start, end - start);
      if (templateName == title ||
          (titleParts.namespaceNumber == mwc::NS_TEMPLATE && templateName == titleParts.unprefixedTitle())) {
        pages.push_back(pageTitle);
        break;
      }
      position = end;
    }
  }
  return pages;
}

vector<string> MockWiki::getAllPages(const AllPagesParams& params) {
  string prefix =
      (params.namespace_ == NS_MAIN ? "" : m_siteInfo.namespaces().at(params.namespace_).name + ":") + params.prefix;
  vector<string> pages;
  for (const auto& [pageTitle, page] : m_pages) {
    if (!pageTitle.starts_with(prefix)) continue;
    if (page.revisions.empty()) continue;
    if (params.filterRedir != FR_ALL) {
      bool isRedirect = readRedirect(readRevisionContent(page.revisions.back()), nullptr, nullptr);
      if ((params.filterRedir == FR_REDIRECTS) != isRedirect) continue;
    }
    if (params.protectType != 0) {
      bool match = false;
      for (const PageProtection& protection : page.protections) {
        if ((params.protectType & protection.type) &&
            (params.protectLevel == 0 || (params.protectLevel & protection.level))) {
          match = true;
          break;
        }
      }
      if (!match) continue;
    }
    pages.push_back(pageTitle);
  }
  CBL_ASSERT(params.limit == PAGER_ALL || params.limit > static_cast<int>(pages.size()));
  std::sort(pages.begin(), pages.end());
  return pages;
}

void MockWiki::writePageInternal(const string& title, const string& content, const WriteToken& writeToken,
                                 const string& summary, int flags) {
  Page& page = getMutablePage(title);
  const PageProtection* editProtection = getProtectionByType(page.protections, PRT_EDIT);
  if (editProtection != nullptr && editProtection->level == PRL_SYSOP) {
    throw ProtectedPageError("title=" + title);
  }
  string trimmedContent(cbl::trim(content, cbl::TRIM_RIGHT));
  string oldContent;
  if (!page.revisions.empty()) {
    oldContent = m_revisions[page.revisions.back()].content;
  }
  if (trimmedContent == oldContent) {
    return;
  }
  page.revisions.push_back(m_nextRevid);
  Revision& revision = m_revisions[m_nextRevid];
  revision.title = title;
  revision.revid = m_nextRevid;
  revision.minor_ = flags & EDIT_MINOR;
  revision.timestamp = Date::now();
  revision.user = externalUserName();
  revision.size = content.size();
  revision.comment = summary;
  revision.content = (flags & EDIT_APPEND) ? oldContent + trimmedContent : trimmedContent;
  if (m_verboseWrite) {
    std::cout << "Writing '" << title << "'\n" << revision.content << "\n";
  }
  m_nextRevid++;
}

void MockWiki::setPageProtection(const string& title, const vector<PageProtection>& protections, const string& reason) {
  Page& page = getMutablePage(title);
  int i = 0;
  for (const PageProtection& protection : page.protections) {
    if (getProtectionByType(protections, protection.type) == nullptr) {
      page.protections[i] = protection;
      i++;
    }
  }
  page.protections.resize(i);
  for (const PageProtection& protection : protections) {
    if (protection.level != PRL_NONE) {
      page.protections.push_back(protection);
    }
  }
}

void MockWiki::deletePage(const string& title, const string& reason) {
  if (m_pages.count(title) == 0) {
    throw PageNotFoundError("title=" + title);
  }
  m_pages.erase(title);
}

void MockWiki::resetDatabase() {
  m_pages.clear();
  m_revisions.clear();
  m_nextRevid = 1;
}

void MockWiki::hideRevision(const string& title, int revIndex) {
  const Page& page = getPage(title);
  if (revIndex < 0) {
    revIndex += page.revisions.size();
  }
  CBL_ASSERT(revIndex >= 0 && revIndex < static_cast<int>(page.revisions.size())) << revIndex;
  m_revisions.at(page.revisions[revIndex]).contentHidden = true;
}

void MockWiki::setPageContent(const string& title, const string& content) {
  writePage(title, content, WriteToken::newWithoutConflictDetection(), "", EDIT_ALLOW_BLANKING);
}

void MockWiki::assertPageLastCommentEquals(const string& title, const string& expectedComment) {
  const Page& page = getPage(title);
  CBL_ASSERT(!page.revisions.empty()) << title;
  CBL_ASSERT_EQ(m_revisions.at(page.revisions.back()).comment, expectedComment);
}

json::Value MockWiki::apiRequest(const string& request, const string& data, bool canRetry) {
  if (!request.empty()) {
    CBL_FATAL << "MockWiki::apiRequest called with request = " << request;
  } else {
    CBL_FATAL << "MockWiki::apiRequest called with data = " << data;
  }
}

void MockWiki::sleep(int seconds) {}

const MockWiki::Page& MockWiki::getPage(const string& title) const {
  static const Page EMPTY_PAGE;
  unordered_map<string, Page>::const_iterator it = m_pages.find(normalizeTitle(title));
  return it == m_pages.end() ? EMPTY_PAGE : it->second;
}

MockWiki::Page& MockWiki::getMutablePage(const string& title) {
  return m_pages[normalizeTitle(title)];
}

}  // namespace mwc
