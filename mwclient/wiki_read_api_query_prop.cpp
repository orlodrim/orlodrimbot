// IMPLEMENTS: wiki.h
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "cbl/date.h"
#include "cbl/json.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "bot_exclusion.h"
#include "request.h"
#include "wiki.h"
#include "wiki_base.h"
#include "wiki_defs.h"

using cbl::Date;
using std::map;
using std::pair;
using std::string;
using std::unordered_map;
using std::unordered_multimap;
using std::vector;

namespace mwc {

constexpr FlagDef REVISION_PROPS[] = {
    {RP_COMMENT, "comment"},     {RP_CONTENT, "content"},    {RP_CONTENT_MODEL, "contentmodel"},
    {RP_MINOR, "flags"},         {RP_REVID, "ids"},          {RP_PARSEDCOMMENT, "parsedcomment"},
    {RP_SHA1, "sha1"},           {RP_SIZE, "size|slotsize"}, {RP_TAGS, "tags"},
    {RP_TIMESTAMP, "timestamp"}, {RP_USER, "user"},          {RP_USERID, "userid"},
};

static int filterRevisionProps(int properties) {
  properties &= ~RP_TITLE;
  // If no property is requested, use "flags" instead of the default "ids|timestamp|flags|comment|user".
  return properties != 0 ? properties : RP_MINOR;
}

static const json::Value& getSinglePageFromAnswer(const json::Value* answer, bool mustExist = true) {
  const json::Value& query = (*answer)["query"];
  const json::Value& page = query["pages"].object().firstValue();
  if (!page.isObject()) {
    if (query.has("interwiki")) {
      throw InvalidParameterError("Invalid title (interwiki)");
    } else if (query.has("badrevids")) {
      throw PageNotFoundError("Revision does not exist");
    }
    throw UnexpectedAPIResponseError("Unexpected API answer (missing page): " + answer->toJSON());
  } else if (page.has("invalid")) {
    throw InvalidParameterError("Invalid title");
  } else if (page.has("special")) {
    throw InvalidParameterError("Invalid title (special page)");
  } else if (page.has("missing") && mustExist) {
    throw PageNotFoundError("The page does not exist");
  }
  return page;
}

static void convertJSONToRevision(const json::Value& value, bool extractTitle, Revision& rev) {
  const json::Value& slot = value.has("slots") ? value["slots"]["main"] : value;
  if (extractTitle) {
    rev.title = value["title"].str();
  }
  rev.revid = static_cast<revid_t>(value["revid"].numberAsInt64());
  rev.minor_ = value.has("minor");
  rev.timestamp = parseAPITimestamp(value["timestamp"].str());
  rev.user = value["user"].str();
  rev.userid = value["userid"].numberAsInt64();
  rev.size = slot["size"].numberAsInt();
  rev.comment = value["comment"].str();
  rev.parsedComment = value["parsedcomment"].str();
  rev.content = slot["*"].str();
  rev.sha1 = value["sha1"].str();
  rev.contentHidden = slot.has("texthidden") || value.has("sha1hidden");
  const string& contentModel = slot["contentmodel"].str();
  if (contentModel == "wikitext") {
    rev.contentModel = RCM_WIKITEXT;
  } else if (contentModel == "flow-board") {
    rev.contentModel = RCM_FLOW_BOARD;
  } else {
    rev.contentModel = RCM_INVALID;
  }
  rev.tags.clear();
  for (const json::Value& tag : value["tags"].array()) {
    rev.tags.push_back(tag.str());
  }
}

Revision convertPageJSONToRevision(const json::Value& page, int properties) {
  const json::Value& revJSON = page["revisions"][0];
  // NOTE: revJSON should be an object, but if it is empty, MediaWiki returns
  // an empty *array* instead.
  // Query: action=query&prop=revisions&titles=API&rvprop=flags&format=json
  // Result: {"query":{"pages":{"11826":{"pageid":11826,"ns":0,"title":"API",
  //                                     "revisions":[[]]}}}}
  if (revJSON.isNull()) {
    throw UnexpectedAPIResponseError("Unexpected API answer (missing revision): " + page.toJSON());
  }
  Revision revision;
  convertJSONToRevision(revJSON, /* extractTitle = */ false, revision);
  revision.title = (properties & RP_TITLE) ? page["title"].str() : string();
  return revision;
}

using PageCallback = std::function<void(const string& title, const json::Value&)>;

using TitleMap = unordered_map<string, string>;

TitleMap parseTitleMap(const json::Value& value) {
  TitleMap titleMap;
  for (const json::Value& entry : value.array()) {
    const string& from = entry["from"].str();
    const string& to = entry["to"].str();
    if (from.empty() || to.empty() || from == to) {
      throw UnexpectedAPIResponseError("Cannot parse title info in mapping: " + value.toJSON());
    }
    titleMap[from] = to;
  }
  return titleMap;
}

const string* followTitleMapping(const TitleMap& titleMap, const string* title) {
  TitleMap::const_iterator it = titleMap.find(*title);
  return it != titleMap.end() ? &it->second : title;
}

static void readPagesPropertiesOneRequest(WikiBase& wiki, const WikiPropPager& pager, StringRange titlesRange,
                                          const PageCallback& pageCallback) {
  CBL_ASSERT(titlesRange.first < titlesRange.second);
  WikiPropPager pagerCopy = pager;
  pagerCopy.setMethod(WikiRequest::METHOD_POST_NO_SIDE_EFFECT);
  pagerCopy.setParam("titles", cbl::join(titlesRange.first, titlesRange.second, "|"));
  pagerCopy.setLimit(PAGER_ALL);
  pagerCopy.runPager(wiki, [&](const json::Value& answer) {
    const json::Value& query = answer["query"];
    const json::Value& pages = query["pages"];
    if (!pages.isObject()) {
      if (query.has("interwiki")) {
        // All titles are interwikis.
        return 0;
      }
      throw UnexpectedAPIResponseError("Both 'query.pages' and 'query.interwiki' are missing in server answer");
    }

    unordered_multimap<string, string> reverseTitleMapping;
    TitleMap normalizationMap = parseTitleMap(query["normalized"]);
    TitleMap redirectionMap = parseTitleMap(query["redirects"]);
    for (const string* title = titlesRange.first; title != titlesRange.second; title++) {
      const string* normalizedTitle = followTitleMapping(normalizationMap, title);
      const string* fullyResolvedTitle = followTitleMapping(redirectionMap, normalizedTitle);
      reverseTitleMapping.emplace(*fullyResolvedTitle, *title);
    }

    for (const pair<const string, json::Value>& keyAndPage : pages) {
      const json::Value& page = keyAndPage.second;
      const string& title = page["title"].str();
      using TitleIt = unordered_multimap<string, string>::iterator;
      pair<TitleIt, TitleIt> originalTitles = reverseTitleMapping.equal_range(title);
      if (originalTitles.first == originalTitles.second) {
        throw UnexpectedAPIResponseError("Page info is given for a title that was not requested: '" + title + "'");
      }
      if (page.has("invalid")) {
        continue;
      }
      for (TitleIt titleIt = originalTitles.first; titleIt != originalTitles.second; ++titleIt) {
        pageCallback(titleIt->second, page);
      }
    }
    return 0;  // The limit is PAGER_ALL so there is no need to return the exact number of items read.
  });
}

static void readPagesProperties(WikiBase& wiki, const WikiPropPager& pager, const vector<string>& titles,
                                const PageCallback& pageCallback) {
  for (StringRange titlesRange : splitVectorIntoRanges(titles, wiki.apiTitlesLimit())) {
    try {
      readPagesPropertiesOneRequest(wiki, pager, titlesRange, pageCallback);
    } catch (WikiError& error) {
      // TODO: Add a parameter to make the error message more precise.
      error.addContext("Cannot read pages " + quoteAndJoin(titlesRange));
      throw;
    }
  }
}

static PagesStringProperties readPagesStringProperties(WikiBase& wiki, const string& property, const string& limitParam,
                                                       const vector<string>& titles) {
  WikiPropPager pager(property, limitParam);
  PagesStringProperties properties;
  readPagesProperties(wiki, pager, titles, [&property, &properties](const string& title, const json::Value& page) {
    vector<string>& pageProperties = properties[title];
    for (const json::Value& propertyNode : page[property].array()) {
      pageProperties.push_back(propertyNode["title"].str());
    }
  });
  return properties;
}

static unordered_map<string, string> getPagesPageProps(WikiBase& wiki, const vector<string>& titles,
                                                       const string& pageProp) {
  WikiPropPager pager("pageprops", NO_LIMIT_PARAM);
  pager.setParam("ppprop", pageProp);

  unordered_map<string, string> pagesPageProps;
  readPagesProperties(wiki, pager, titles, [&pagesPageProps, &pageProp](const string& title, const json::Value& page) {
    const json::Value& value = page["pageprops"][pageProp];
    if (!value.isNull()) {
      pagesPageProps[title] = value.str();
    }
  });
  return pagesPageProps;
}

static void readRevisionsOneRequest(WikiBase& wiki, int properties, StringRange revidsRange,
                                    const unordered_multimap<revid_t, Revision*>& revisionsByRevid) {
  CBL_ASSERT(revidsRange.first < revidsRange.second);
  WikiRequest request("query");
  request.setMethod(WikiRequest::METHOD_POST_NO_SIDE_EFFECT);
  request.setParam("prop", "revisions");
  request.setParam("rvslots", "main");
  request.setFlagsParam("rvprop", filterRevisionProps(properties | RP_REVID), REVISION_PROPS);
  request.setParam("revids", cbl::join(revidsRange.first, revidsRange.second, "|"));
  json::Value answer = request.run(wiki);

  const json::Value& query = answer["query"];
  const json::Value& pages = query["pages"];
  if (!pages.isObject()) {
    if (query.has("badrevids")) {
      // All revids are bad.
      return;
    }
    throw UnexpectedAPIResponseError("Both 'query.pages' and 'query.badrevids' are missing in server answer");
  }
  for (const pair<const string, json::Value>& keyAndPage : pages) {
    const json::Value& page = keyAndPage.second;
    const string& title = page["title"].str();
    const json::Value& revisionsArray = page["revisions"];
    if (!revisionsArray.isArray()) {
      if (page.has("missing")) {
        // For deleted revids, sysops get the corresponding page with an "missing" attribute and without the revision
        // itself.
        continue;
      }
      throw UnexpectedAPIResponseError("'query.pages.<pageid>.revisions' missing in server answer");
    }
    for (const json::Value& revisionNode : revisionsArray.array()) {
      revid_t revid = revisionNode["revid"].numberAsInt64();
      using RevisionIt = unordered_multimap<revid_t, Revision*>::const_iterator;
      pair<RevisionIt, RevisionIt> revisions = revisionsByRevid.equal_range(revid);
      if (revisions.first == revisions.second) {
        throw UnexpectedAPIResponseError("Revision info is given for a revision id that was not requested: " +
                                         std::to_string(revid));
      }
      for (RevisionIt revisionIt = revisions.first; revisionIt != revisions.second; ++revisionIt) {
        Revision* revision = revisionIt->second;
        convertJSONToRevision(revisionNode, /* extractTitle = */ false, *revision);
        revision->title = (properties & RP_TITLE) ? title : string();
      }
    }
  }
}

Revision Wiki::readPage(const string& title, int properties) {
  WikiRequest request("query");
  request.setParam("prop", "revisions");
  request.setParam("rvslots", "main");
  request.setFlagsParam("rvprop", filterRevisionProps(properties), REVISION_PROPS);
  request.setParam("titles", title);

  try {
    json::Value answer = request.run(*this);
    const json::Value& page = getSinglePageFromAnswer(&answer);
    return convertPageJSONToRevision(page, properties);
  } catch (WikiError& error) {
    error.addContext("Cannot read page '" + title + "'");
    throw;
  }
}

Revision Wiki::readPage(const string& title, int properties, WriteToken* writeToken) {
  int extraProperties = writeToken != nullptr ? (RP_CONTENT | RP_TIMESTAMP) : 0;
  Revision revision = readPage(title, properties | extraProperties);
  if (writeToken != nullptr) {
    bool needsNoBotsBypass = testBotExclusion(revision.content, m_externalUserName, "");
    *writeToken = WriteToken::newForEdit(title, revision.timestamp, needsNoBotsBypass);
    if (!(properties & RP_CONTENT)) {
      revision.content.clear();
    }
    if (!(properties & RP_TIMESTAMP)) {
      revision.timestamp = Date();
    }
  }
  return revision;
}

string Wiki::readPageContentIfExists(const string& title, WriteToken* writeToken) {
  try {
    return readPageContent(title, writeToken);
  } catch (const PageNotFoundError&) {
    if (writeToken != nullptr) {
      *writeToken = WriteToken::newForCreation();
    }
    return string();
  }
}

string Wiki::readPageContent(const string& title, WriteToken* writeToken) {
  return readPage(title, RP_CONTENT, writeToken).content;
}

Revision Wiki::readRevision(revid_t revid, int properties) {
  WikiRequest request("query");
  request.setParam("prop", "revisions");
  request.setParam("rvslots", "main");
  request.setFlagsParam("rvprop", filterRevisionProps(properties), REVISION_PROPS);
  request.setRevidParam("revids", revid);

  try {
    json::Value answer = request.run(*this);
    const json::Value& page = getSinglePageFromAnswer(&answer);
    return convertPageJSONToRevision(page, properties);
  } catch (WikiError& error) {
    error.addContext("Cannot read revision '" + std::to_string(revid) + "'");
    throw;
  }
}

string Wiki::readRevisionContent(revid_t revid) {
  return readRevision(revid, RP_CONTENT).content;
}

void Wiki::readPages(int properties, std::vector<Revision>& revisions, int readPageFlags) {
  WikiPropPager pager("revisions", NO_LIMIT_PARAM);
  pager.setParam("rvslots", "main");
  pager.setFlagsParam("rvprop", filterRevisionProps(properties), REVISION_PROPS);
  if (readPageFlags & READ_RESOLVE_REDIRECTS) {
    pager.setParam("redirects", "1");
  }

  vector<string> titles;
  unordered_multimap<string, Revision*> revisionsByTitle;
  for (Revision& revision : revisions) {
    // A query with no titles gives a strange result, so we filter out empty titles.
    if (!revision.title.empty() && revision.title.find('|') == string::npos) {
      if (revisionsByTitle.find(revision.title) == revisionsByTitle.end()) {
        titles.push_back(revision.title);
      }
      revisionsByTitle.emplace(revision.title, &revision);
    }
    revision.revid = -2;
  }

  PageCallback callback = [&revisionsByTitle, properties](const string& title, const json::Value& page) {
    using RevisionsIt = unordered_multimap<string, Revision*>::iterator;
    pair<RevisionsIt, RevisionsIt> revisions = revisionsByTitle.equal_range(title);
    for (RevisionsIt it = revisions.first; it != revisions.second; ++it) {
      Revision* revision = it->second;
      if (page.has("missing")) {
        revision->revid = -1;
      } else {
        revision->revid = 0;
        convertJSONToRevision(page["revisions"][0], /* extractTitle = */ false, *revision);
      }
      if (properties & RP_TITLE) {
        revision->title = page["title"].str();  // This is the normalized title.
      }
    }
  };
  readPagesProperties(*this, pager, titles, callback);
}

void Wiki::readRevisions(int properties, std::vector<Revision>& revisions) {
  unordered_multimap<revid_t, Revision*> revisionsByRevid;
  vector<string> revids;
  for (Revision& revision : revisions) {
    if (revisionsByRevid.find(revision.revid) == revisionsByRevid.end()) {
      revids.push_back(std::to_string(revision.revid));
    }
    revisionsByRevid.emplace(revision.revid, &revision);
    revision.title = INVALID_TITLE;
  }
  for (StringRange revidsRange : splitVectorIntoRanges(revids, m_apiTitlesLimit)) {
    try {
      readRevisionsOneRequest(*this, properties, revidsRange, revisionsByRevid);
    } catch (WikiError& error) {
      error.addContext("Cannot read revisions " + cbl::join(revidsRange.first, revidsRange.second, ", "));
      throw;
    }
  };
}

bool Wiki::pageExists(const string& title) {
  try {
    readPage(title, RP_TIMESTAMP);
  } catch (const PageNotFoundError&) {
    return false;
  }
  return true;
}

vector<string> Wiki::getPageLinks(const string& title) {
  return getPagesLinks({title})[title];
}

PagesStringProperties Wiki::getPagesLinks(const vector<string>& titles) {
  return readPagesStringProperties(*this, "links", "pllimit", titles);
}

vector<string> Wiki::getPageCategories(const string& title) {
  return getPagesCategories({title})[title];
}

PagesStringProperties Wiki::getPagesCategories(const vector<string>& titles) {
  return readPagesStringProperties(*this, "categories", "cllimit", titles);
}

map<string, vector<pair<string, Date>>> Wiki::getPagesCategoriesWithDate(const vector<string>& titles) {
  WikiPropPager pager("categories", "cllimit");
  pager.setParam("clprop", "timestamp");

  map<string, vector<pair<string, Date>>> pagesCategories;
  readPagesProperties(*this, pager, titles, [&pagesCategories](const string& title, const json::Value& page) {
    vector<pair<string, Date>>& categories = pagesCategories[title];
    for (const json::Value& categoryNode : page["categories"].array()) {
      categories.emplace_back(categoryNode["title"].str(), parseAPITimestamp(categoryNode["timestamp"].str()));
    }
  });
  return pagesCategories;
}

vector<string> Wiki::getPageTemplates(const string& title) {
  return getPagesTemplates({title})[title];
}

PagesStringProperties Wiki::getPagesTemplates(const vector<string>& titles) {
  return readPagesStringProperties(*this, "templates", "tllimit", titles);
}

vector<string> Wiki::getPageImages(const string& title) {
  return getPagesImages({title})[title];
}

PagesStringProperties Wiki::getPagesImages(const vector<string>& titles) {
  return readPagesStringProperties(*this, "images", "imlimit", titles);
}

vector<string> Wiki::getPageLangLinks(const string& title) {
  return getPagesLangLinks({title})[title];
}

PagesStringProperties Wiki::getPagesLangLinks(const vector<string>& titles, const string& lang) {
  WikiPropPager pager("langlinks", "lllimit");
  pager.setParamWithEmptyDefault("lllang", lang);
  PagesStringProperties pagesLangLinks;
  readPagesProperties(*this, pager, titles, [&pagesLangLinks](const string& title, const json::Value& page) {
    vector<string>& langLinks = pagesLangLinks[title];
    for (const json::Value& langLinkNode : page["langlinks"].array()) {
      langLinks.push_back(langLinkNode["lang"].str() + ":" + langLinkNode["*"].str());
    }
  });
  return pagesLangLinks;
}

unordered_map<string, bool> Wiki::getPagesDisambigStatus(const vector<string>& titles) {
  WikiPropPager pager("pageprops", NO_LIMIT_PARAM);
  pager.setParam("ppprop", "disambiguation");
  unordered_map<string, bool> disambigStatus;
  readPagesProperties(*this, pager, titles, [&disambigStatus](const string& title, const json::Value& page) {
    disambigStatus[title] = !page["pageprops"]["disambiguation"].isNull();
  });
  return disambigStatus;
}

unordered_map<string, string> Wiki::getPagesWikibaseItems(const vector<string>& titles) {
  return getPagesPageProps(*this, titles, "wikibase_item");
}

vector<PageProtection> Wiki::getPageProtections(const string& title) {
  return getPagesProtections({title})[title];
}

unordered_map<string, vector<PageProtection>> Wiki::getPagesProtections(const vector<string>& titles) {
  WikiPropPager pager("info", NO_LIMIT_PARAM);
  pager.setParam("inprop", "protection");

  unordered_map<string, vector<PageProtection>> pagesProtections;
  readPagesProperties(*this, pager, titles, [&pagesProtections](const string& title, const json::Value& page) {
    const json::Value& protectionsNode = page["protection"];
    if (!protectionsNode.isArray()) return;
    vector<PageProtection>& pageProtections = pagesProtections[title];

    for (const json::Value& protectionNode : protectionsNode.array()) {
      PageProtection pageProtection;

      const string& type = protectionNode["type"].str();
      if (type == "edit") {
        pageProtection.type = PRT_EDIT;
      } else if (type == "move") {
        pageProtection.type = PRT_MOVE;
      } else if (type == "upload") {
        pageProtection.type = PRT_UPLOAD;
      } else if (type == "create") {
        pageProtection.type = PRT_CREATE;
      } else {
        continue;
      }

      const string& level = protectionNode["level"].str();
      if (level == "autoconfirmed") {
        pageProtection.level = PRL_AUTOCONFIRMED;
      } else if (level == "editextendedsemiprotected") {
        pageProtection.level = PRL_AUTOPATROLLED;
      } else if (level == "sysop") {
        pageProtection.level = PRL_SYSOP;
      } else {
        continue;
      }

      const string& expiry = protectionNode["expiry"].str();
      if (expiry == "infinity") {
        pageProtection.expiry = Date();
      } else {
        pageProtection.expiry = parseAPITimestamp(expiry);
      }

      pageProtections.push_back(pageProtection);
    }
  });
  return pagesProtections;
}

ImageSize Wiki::getImageSize(const string& title) {
  return getImagesSize({title})[title];
}

unordered_map<string, ImageSize> Wiki::getImagesSize(const vector<string>& titles) {
  WikiPropPager pager("imageinfo", NO_LIMIT_PARAM);
  pager.setParam("iiprop", "size");

  unordered_map<string, ImageSize> imagesSize;
  readPagesProperties(*this, pager, titles, [&imagesSize](const string& title, const json::Value& page) {
    const json::Value& firstImageInfo = page["imageinfo"][0];
    if (!firstImageInfo.isNull()) {
      ImageSize& imageSize = imagesSize[title];
      // NOTE: For sound files, the width and height are defined and equal to 0.
      imageSize.width = firstImageInfo["width"].numberAsInt();
      imageSize.height = firstImageInfo["height"].numberAsInt();
    }
  });
  return imagesSize;
}

unordered_map<string, int> Wiki::getCategoriesCount(const vector<string>& titles) {
  WikiPropPager pager("categoryinfo", NO_LIMIT_PARAM);
  unordered_map<string, int> categoriesCount;
  readPagesProperties(*this, pager, titles, [&categoriesCount](const string& title, const json::Value& page) {
    const json::Value& value = page["categoryinfo"]["size"];
    if (!value.isNull()) {
      categoriesCount[title] = value.numberAsInt();
    }
  });
  return categoriesCount;
}

static vector<Revision> getHistoryOrDeletedHistory(WikiBase& wiki, const HistoryParams& params,
                                                   const string& propPrefix, const string& propName,
                                                   const string& propDebugName) {
  if (params.prop == 0) {
    throw std::invalid_argument("'prop' field of HistoryParams must not be zero");
  } else if (propPrefix == "drv") {
    if (params.startId != 0) {
      throw std::invalid_argument("'startId' field of HistoryParams must be null for getDeletedHistory");
    } else if (params.endId != 0) {
      throw std::invalid_argument("'endId' field of HistoryParams must be null for getDeletedHistory");
    }
  }

  WikiPropPager pager(propName, propPrefix + "limit");
  pager.setParam("titles", params.title);
  pager.setParam(propPrefix + "slots", "main");
  pager.setFlagsParam(propPrefix + "prop", filterRevisionProps(params.prop), REVISION_PROPS);
  pager.setParam(propPrefix + "dir", params.direction);
  pager.setParam(propPrefix + "start", params.start);
  pager.setParam(propPrefix + "end", params.end);
  pager.setRevidParam(propPrefix + "startid", params.startId);
  pager.setRevidParam(propPrefix + "endid", params.endId);
  pager.setLimit(params.limit);
  pager.setQueryContinue(params.queryContinue);

  vector<Revision> revisions;
  try {
    pager.runPager(wiki, [&params, &propName, &revisions](const json::Value& answer) {
      const json::Value& pageNode = getSinglePageFromAnswer(&answer, /* mustExist = */ propName != "deletedrevisions");
      const json::Value& revisionsNode = pageNode[propName];
      string titleIfRequested = (params.prop & RP_TITLE) ? pageNode["title"].str() : string();

      int numRevisionsRead = revisionsNode.array().size();
      revisions.reserve(revisions.size() + numRevisionsRead);
      for (const json::Value& revNode : revisionsNode.array()) {
        revisions.emplace_back();
        convertJSONToRevision(revNode, /* extractTitle = */ false, revisions.back());
        revisions.back().title = titleIfRequested;
      }
      return numRevisionsRead;
    });
  } catch (WikiError& error) {
    error.addContext("Cannot read the " + propDebugName + " of '" + params.title + "'");
    throw;
  }

  if (params.nextQueryContinue) {
    *params.nextQueryContinue = pager.queryContinue();
  }
  return revisions;
}

vector<Revision> Wiki::getHistory(const HistoryParams& params) {
  return getHistoryOrDeletedHistory(*this, params, "rv", "revisions", "history");
}

vector<Revision> Wiki::getDeletedHistory(const HistoryParams& params) {
  return getHistoryOrDeletedHistory(*this, params, "drv", "deletedrevisions", "deleted history");
}

}  // namespace mwc
