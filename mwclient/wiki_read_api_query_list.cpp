// IMPLEMENTS: wiki.h
#include <algorithm>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "cbl/json.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "request.h"
#include "titles_util.h"
#include "wiki.h"
#include "wiki_base.h"
#include "wiki_defs.h"

using std::pair;
using std::string;
using std::unordered_multimap;
using std::vector;

namespace mwc {

constexpr FlagDef RECENT_CHANGE_PROPS[] = {
    {RP_TITLE, "title"},         {RP_REVID, "ids"},
    {RP_MINOR, "flags"},         {RP_BOT, "flags"},
    {RP_TIMESTAMP, "timestamp"}, {RP_USER, "user"},
    {RP_USERID, "userid"},       {RP_SIZE, "sizes"},
    {RP_COMMENT, "comment"},     {RP_PARSEDCOMMENT, "parsedcomment"},
    {RP_TAGS, "tags"},           {RP_REDIRECT, "redirect"},
    {RP_PATROLLED, "patrolled"}, {RP_NEW, "flags"},
    {RP_SHA1, "sha1"},
};

constexpr FlagDef RECENT_CHANGE_TYPES[] = {
    {RC_EDIT, "edit"},
    {RC_NEW, "new"},
    {RC_LOG, "log"},
};

constexpr FlagDef RECENT_CHANGE_SHOW[] = {
    {RCS_MINOR, "minor"},
    {RCS_NOT_MINOR, "!minor"},
    {RCS_BOT, "bot"},
    {RCS_NOT_BOT, "!bot"},
    {RCS_ANON, "anon"},
    {RCS_NOT_ANON, "!anon"},
    {RCS_REDIRECT, "redirect"},
    {RCS_NOT_REDIRECT, "!redirect"},
    {RCS_PATROLLED, "patrolled"},
    {RCS_NOT_PATROLLED, "!patrolled"},
};

constexpr FlagDef LOG_PROPS[] = {
    {RP_REVID, "ids"},
    {RP_USER, "user"},
    {RP_USERID, "userid"},
    {RP_TIMESTAMP, "timestamp"},
    {RP_SIZE, "size"},
    {RP_COMMENT, "comment"},
    {RP_PARSEDCOMMENT, "parsedcomment"},
};

constexpr FlagDef CATEGORY_MEMBERS_PROPS[] = {
    {CMP_SORTKEY_PREFIX, "sortkeyprefix"},
    {CMP_TIMESTAMP, "timestamp"},
};

constexpr FlagDef PROTECTION_TYPES[] = {
    {PRT_EDIT, "edit"},
    {PRT_MOVE, "move"},
    {PRT_UPLOAD, "upload"},
};

constexpr FlagDef PROTECTION_LEVELS[] = {
    {PRL_AUTOCONFIRMED, "autoconfirmed"},
    {PRL_AUTOPATROLLED, "editextendedsemiprotected"},
    {PRL_SYSOP, "sysop"},
};

constexpr FlagDef USER_CONTRIBS_PROPS[] = {
    {RP_TITLE, "title"},
    {RP_REVID, "ids"},
    {RP_MINOR, "flags"},
    {RP_TIMESTAMP, "timestamp"},
    {RP_SIZE, "size"},
    {RP_COMMENT, "comment"},
    {RP_PARSEDCOMMENT, "parsedcomment"},
    {RP_TAGS, "tags"},
    {RP_PATROLLED, "patrolled"},
    {RP_NEW, "flags"},
};

constexpr FlagDef USER_CONTRIBS_SHOW[] = {
    {RCS_MINOR, "minor"},
    {RCS_NOT_MINOR, "!minor"},
    {RCS_PATROLLED, "patrolled"},
    {RCS_NOT_PATROLLED, "!patrolled"},
};

constexpr FlagDef USER_INFO_PROPS[] = {
    {UIP_EDIT_COUNT, "editcount"},
    {UIP_GROUPS, "groups"},
};

static const char* getStringOfFilterRedirMode(FilterRedirMode mode) {
  switch (mode) {
    case FR_ALL:
      return "all";
    case FR_REDIRECTS:
      return "redirects";
    case FR_NONREDIRECTS:
      return "nonredirects";
  }
  throw std::invalid_argument("getStringOfFilterRedirMode called with invalid mode " + std::to_string(mode));
}

static const char* getStringOfUserGroup(UserInfoGroups userGroup) {
  switch (userGroup) {
    case UIG_SYSOP:
      return "sysop";
    case UIG_BOT:
      return "bot";
    default:
      break;
  }
  throw std::invalid_argument("getStringOfUserGroup called with unsupported group " + std::to_string(userGroup));
}

static RecentChangeType getRecentChangeTypeFromString(const string& str) {
  if (str == "edit") {
    return RC_EDIT;
  } else if (str == "new") {
    return RC_NEW;
  } else if (str == "log") {
    return RC_LOG;
  }
  return RC_UNDEFINED;
}

static LogEventType getLogEventTypeFromString(const string& str) {
  if (str == "block") {
    return LE_BLOCK;
  } else if (str == "protect") {
    return LE_PROTECT;
  } else if (str == "rights") {
    return LE_RIGHTS;
  } else if (str == "delete") {
    return LE_DELETE;
  } else if (str == "upload") {
    return LE_UPLOAD;
  } else if (str == "move") {
    return LE_MOVE;
  } else if (str == "import") {
    return LE_IMPORT;
  } else if (str == "patrol") {
    return LE_PATROL;
  } else if (str == "merge") {
    return LE_MERGE;
  } else if (str == "suppress") {
    return LE_SUPPRESS;
  } else if (str == "abusefilter") {
    return LE_ABUSEFILTER;
  } else if (str == "newusers") {
    return LE_NEWUSERS;
  }
  return LE_ALL;
}

static const char* getStringOfLogEventType(LogEventType type) {
  switch (type) {
    case LE_ALL:
      return "";
    case LE_BLOCK:
      return "block";
    case LE_PROTECT:
      return "protect";
    case LE_RIGHTS:
      return "rights";
    case LE_DELETE:
      return "delete";
    case LE_UPLOAD:
      return "upload";
    case LE_MOVE:
      return "move";
    case LE_IMPORT:
      return "import";
    case LE_PATROL:
      return "patrol";
    case LE_MERGE:
      return "merge";
    case LE_SUPPRESS:
      return "suppress";
    case LE_ABUSEFILTER:
      return "abusefilter";
    case LE_NEWUSERS:
      return "newusers";
  }
  throw std::invalid_argument("getStringOfLogEventType called with invalid type " + std::to_string(type));
}

static void convertJSONToLogEvent(const json::Value& value, LogEvent& le) {
  const string& logTypeStr = value.has("logtype") ? value["logtype"].str() : value["type"].str();
  le.type = getLogEventTypeFromString(logTypeStr);
  le.action = value.has("logaction") ? value["logaction"].str() : value["action"].str();
  le.title = value["title"].str();
  le.logid = value["logid"].numberAsInt64();
  le.bot = value.has("bot");
  le.timestamp = parseAPITimestamp(value["timestamp"].str());
  le.user = value["user"].str();
  le.userid = value["userid"].numberAsInt64();
  le.comment = value["comment"].str();
  le.parsedComment = value["parsedcomment"].str();
  const json::Value& newTitle = value["params"]["target_title"];
  if (!newTitle.isNull()) {
    le.setNewTitle(newTitle.str());
  } else {
    const json::Value& newTitle2 = value["logparams"]["target_title"];
    if (!newTitle2.isNull()) {
      le.setNewTitle(newTitle2.str());
    } else {
      le.setNewTitle(value["move"]["new_title"].str());
    }
  }
}

static void convertJSONToRecentChange(const json::Value& value, RecentChange& recentChange) {
  RecentChangeType rcType = getRecentChangeTypeFromString(value["type"].str());
  recentChange.setType(rcType);
  recentChange.rcid = value["rcid"].numberAsInt64();
  recentChange.oldRevid = 0;
  recentChange.oldSize = value["oldlen"].numberAsInt();
  switch (rcType) {
    case RC_UNDEFINED:
      break;
    case RC_EDIT:
    case RC_NEW: {
      Revision& rev = recentChange.mutableRevision();
      rev.title = value["title"].str();
      rev.revid = static_cast<revid_t>(value["revid"].numberAsInt64());
      rev.minor_ = value.has("minor");
      rev.new_ = value.has("new");
      rev.bot = value.has("bot");
      rev.timestamp = parseAPITimestamp(value["timestamp"].str());
      rev.user = value["user"].str();
      rev.userid = value["userid"].numberAsInt64();
      rev.size = value["newlen"].numberAsInt();
      rev.comment = value["comment"].str();
      rev.parsedComment = value["parsedcomment"].str();
      rev.sha1 = value["sha1"].str();
      rev.tags.clear();
      for (const json::Value& tag : value["tags"].array()) {
        rev.tags.push_back(tag.str());
      }
      rev.redirect = value.has("redirect");
      rev.patrolled = value.has("patrolled");
      recentChange.oldRevid = value["old_revid"].numberAsInt64();
      break;
    }
    case RC_LOG:
      convertJSONToLogEvent(value, recentChange.mutableLogEvent());
      break;
  }
}

vector<RecentChange> Wiki::getRecentChanges(const RecentChangesParams& params) {
  WikiListPager pager("recentchanges", "rclimit");
  pager.setFlagsParam("rcprop", params.prop, RECENT_CHANGE_PROPS, "loginfo");
  pager.setFlagsParam("rctype", params.type, RECENT_CHANGE_TYPES);
  pager.setFlagsParam("rcshow", params.show, RECENT_CHANGE_SHOW);
  pager.setParamWithEmptyDefault("rcuser", params.user);
  pager.setParamWithEmptyDefault("rctag", params.tag);
  pager.setParamWithEmptyDefault("rcnamespace", params.namespaceList.toString());
  pager.setParam("rcdir", params.direction);
  pager.setParam("rcstart", params.start);
  pager.setParam("rcend", params.end);
  pager.setLimit(params.limit);
  pager.setQueryContinue(params.queryContinue);

  vector<RecentChange> recentChanges;
  try {
    recentChanges = pager.runListPager<RecentChange>(*this, convertJSONToRecentChange);
  } catch (WikiError& error) {
    error.addContext("Cannot enumerate recent changes");
    throw;
  }

  if (params.nextQueryContinue != nullptr) {
    *params.nextQueryContinue = pager.queryContinue();
  }
  return recentChanges;
}

vector<LogEvent> Wiki::getLogEvents(const LogEventsParams& params) {
  WikiListPager pager("logevents", "lelimit");
  pager.setFlagsParam("leprop", params.prop, LOG_PROPS, "type|details|title");
  pager.setParamWithEmptyDefault("letype", getStringOfLogEventType(params.type));
  pager.setParamWithEmptyDefault("leuser", params.user);
  pager.setParamWithEmptyDefault("letitle", params.title);
  pager.setParam("ledir", params.direction);
  pager.setParam("lestart", params.start);
  pager.setParam("leend", params.end);
  pager.setLimit(params.limit);

  try {
    return pager.runListPager<LogEvent>(*this, convertJSONToLogEvent);
  } catch (WikiError& error) {
    error.addContext("Cannot enumerate log events");
    throw;
  }
}

void Wiki::getCategoryMembers(const CategoryMembersParams& params) {
  WikiListPager pager("categorymembers", "cmlimit");
  pager.setParam("cmtitle", params.title);
  pager.setFlagsParam("cmprop", params.prop, CATEGORY_MEMBERS_PROPS, "title");
  pager.setOrClearParam("cmsort", "timestamp", params.sort == CMS_TIMESTAMP);
  pager.setOrClearParam("cmdir", "descending", params.sort == CMS_TIMESTAMP && params.direction == NEWEST_FIRST);
  pager.setParam("cmstart", params.start);
  pager.setParam("cmend", params.end);
  pager.setLimit(params.limit);
  // Parameters for the simultaneous size request, if requested.
  pager.setOrClearParam("titles", params.title, params.sizeEstimate != nullptr);
  pager.setOrClearParam("prop", "categoryinfo", params.sizeEstimate != nullptr);

  if (params.members) {
    params.members->clear();
  }
  if (params.titlesOfMembers) {
    params.titlesOfMembers->clear();
  }
  if (params.sizeEstimate) {
    *params.sizeEstimate = 0;
  }
  bool sizeSet = false;

  try {
    pager.runPager(*this, [&pager, &params, &sizeSet](const json::Value& answer) {
      const json::Value& query = answer["query"];
      if (!sizeSet && params.sizeEstimate) {
        const json::Value& size = query["pages"].object().firstValue()["categoryinfo"]["size"];
        // The size may be null if the category is and has always been empty.
        // In any case, the category size is not reliable and MediaWiki has not always enforced that it is >= 0.
        *params.sizeEstimate = size.isNull() ? 0 : std::max(size.numberAsInt(), 0);
        pager.clearParam("prop");
        pager.clearParam("titles");
        sizeSet = true;
      }

      const json::Value& members = query["categorymembers"];
      if (!members.isArray()) {
        throw UnexpectedAPIResponseError("Unexpected API response: 'query.categorymembers' is not an array");
      }

      int numMembers = members.array().size();
      if (params.members) {
        params.members->reserve(params.members->size() + numMembers);
        for (const json::Value& member : members.array()) {
          params.members->emplace_back();
          CategoryMember& newCategoryMember = params.members->back();
          newCategoryMember.title = member["title"].str();
          newCategoryMember.sortkeyPrefix = member["sortkeyprefix"].str();
          newCategoryMember.timestamp = parseAPITimestamp(member["timestamp"].str());
        }
      }
      if (params.titlesOfMembers) {
        params.titlesOfMembers->reserve(params.titlesOfMembers->size() + numMembers);
        for (const json::Value& member : members.array()) {
          params.titlesOfMembers->push_back(member["title"].str());
        }
      }
      return numMembers;
    });
  } catch (WikiError& error) {
    error.addContext("Cannot enumerate members of '" + params.title + "'");
    throw;
  }
}

vector<string> Wiki::getCategoryMembers(const string& category) {
  vector<string> members;
  CategoryMembersParams params;
  params.title = category;
  params.titlesOfMembers = &members;
  getCategoryMembers(params);
  return members;
}

static void convertJSONToTitle(const json::Value& value, string& title) {
  title = value["title"].str();
}

vector<string> Wiki::getBacklinks(const BacklinksParams& params) {
  WikiListPager pager("backlinks", "bllimit");
  pager.setParam("bltitle", params.title);
  pager.setOrClearParam("blfilterredir", getStringOfFilterRedirMode(params.filterRedir), params.filterRedir != FR_ALL);
  pager.setParamWithEmptyDefault("blnamespace", params.namespaceList.toString());

  try {
    return pager.runListPager<string>(*this, convertJSONToTitle);
  } catch (WikiError& error) {
    error.addContext("Cannot enumerate backlinks of '" + params.title + "'");
    throw;
  }
}

vector<string> Wiki::getBacklinks(const string& title) {
  BacklinksParams params;
  params.title = title;
  return getBacklinks(params);
}

vector<string> Wiki::getRedirects(const string& title) {
  BacklinksParams params;
  params.title = title;
  params.filterRedir = FR_REDIRECTS;
  return getBacklinks(params);
}

vector<string> Wiki::getTransclusions(const TransclusionsParams& params) {
  WikiListPager pager("embeddedin", "eilimit");
  pager.setParam("eititle", params.title);
  pager.setParamWithEmptyDefault("einamespace", params.namespaceList.toString());

  try {
    return pager.runListPager<string>(*this, convertJSONToTitle);
  } catch (WikiError& error) {
    error.addContext("Cannot enumerate transclusions of '" + params.title + "'");
    throw;
  }
}

vector<string> Wiki::getTransclusions(const string& title) {
  TransclusionsParams params;
  params.title = title;
  return getTransclusions(params);
}

vector<string> Wiki::getAllPages(const AllPagesParams& params) {
  WikiListPager pager("allpages", "aplimit");
  pager.setParam("apprefix", params.prefix);
  pager.setOrClearParam("apfilterredir", getStringOfFilterRedirMode(params.filterRedir), params.filterRedir != FR_ALL);
  pager.setFlagsParam("apprtype", params.protectType, PROTECTION_TYPES);
  pager.setFlagsParam("apprlevel", params.protectLevel, PROTECTION_LEVELS);
  pager.setParam("apnamespace", params.namespace_);
  pager.setLimit(params.limit);

  try {
    return pager.runListPager<string>(*this, convertJSONToTitle);
  } catch (WikiError& error) {
    error.addContext("Cannot read the list of pages");
    throw;
  }
}

vector<string> Wiki::getPagesByPrefix(const string& prefix) {
  TitleParts titleParts = parseTitle(prefix);
  if (!titleParts.anchor().empty()) {
    throw InvalidParameterError("The prefix passed to getPagesByPrefix must not contain a '#'");
  }
  AllPagesParams params;
  params.prefix = string(titleParts.unprefixedTitle());
  params.namespace_ = titleParts.namespaceNumber;
  params.limit = PAGER_ALL;
  return getAllPages(params);
}

vector<Revision> Wiki::getUserContribs(const UserContribsParams& params) {
  if (params.userPrefix.empty() == params.user.empty()) {
    throw std::invalid_argument("Exactly one of 'user' or 'userPrefix' must be set in UserContribsParams");
  }

  int properties = params.prop & ~(RP_USER | RP_USERID);
  if (properties == 0) {
    properties = RP_MINOR;
  }

  WikiListPager pager("usercontribs", "uclimit");
  pager.setParamWithEmptyDefault("ucuser", params.user);
  pager.setParamWithEmptyDefault("ucuserprefix", params.userPrefix);
  pager.setFlagsParam("ucprop", properties, USER_CONTRIBS_PROPS);
  pager.setFlagsParam("ucshow", params.show, USER_CONTRIBS_SHOW);
  pager.setParamWithEmptyDefault("uctag", params.tag);
  pager.setParamWithEmptyDefault("ucnamespace", params.namespaceList.toString());
  pager.setParam("ucdir", params.direction);
  pager.setParam("ucstart", params.start);
  pager.setParam("ucend", params.end);
  pager.setLimit(params.limit);
  pager.setQueryContinue(params.queryContinue);

  vector<Revision> userContribs;
  try {
    userContribs = pager.runListPager<Revision>(*this, [](const json::Value& value, Revision& userContrib) {
      userContrib.title = value["title"].str();
      userContrib.revid = static_cast<revid_t>(value["revid"].numberAsInt64());
      userContrib.minor_ = value.has("minor");
      userContrib.new_ = value.has("new");
      userContrib.timestamp = parseAPITimestamp(value["timestamp"].str());
      userContrib.user = value["user"].str();
      userContrib.userid = value["userid"].numberAsInt64();
      userContrib.size = value["size"].numberAsInt();
      userContrib.comment = value["comment"].str();
      userContrib.parsedComment = value["parsedcomment"].str();
      // userContrib.sha1 = value["sha1"].str();  // Not supported.
      // userContrib.contentHidden = value.has("sha1hidden");  // Not supported.
      userContrib.tags.clear();
      for (const json::Value& tag : value["tags"].array()) {
        userContrib.tags.push_back(tag.str());
      }
      // userContrib.redirect = value.has("redirect");  // Not supported.
      userContrib.patrolled = value.has("patrolled");
    });
  } catch (WikiError& error) {
    string user = params.userPrefix.empty() ? "'" + params.user + "'" : "[" + params.userPrefix + "*]";
    error.addContext("Cannot read the list of " + user);
    throw;
  }

  if (params.nextQueryContinue) {
    *params.nextQueryContinue = pager.queryContinue();
  }
  return userContribs;
}

static void getUsersInfoOneRequest(WikiBase& wiki, int properties, StringRange namesRange,
                                   const unordered_multimap<string, UserInfo*>& usersByName) {
  CBL_ASSERT(namesRange.first < namesRange.second);
  WikiRequest request("query");
  request.setMethod(WikiRequest::METHOD_POST_NO_SIDE_EFFECT);
  request.setParam("list", "users");
  request.setFlagsParam("usprop", properties & ~UIP_NAME, USER_INFO_PROPS);
  request.setParam("ususers", cbl::join(namesRange.first, namesRange.second, "|"));
  json::Value answer = request.run(wiki);

  const json::Value& usersNode = answer["query"]["users"];
  if (!usersNode.isArray()) {
    throw UnexpectedAPIResponseError("'query.users' not found in the server answer or is not an array");
  }
  int numUsersInQuery = namesRange.second - namesRange.first;
  int numUsersInAnswer = usersNode.array().size();
  if (numUsersInQuery != numUsersInAnswer) {
    throw UnexpectedAPIResponseError("User count mismatch (" + std::to_string(numUsersInQuery) + " requested, " +
                                     std::to_string(numUsersInAnswer) + " received)");
  }
  for (int i = 0; i < numUsersInQuery; i++) {
    const string& unnormalizedName = *(namesRange.first + i);
    const json::Value& userNode = usersNode[i];
    using UserIt = unordered_multimap<string, UserInfo*>::const_iterator;
    pair<UserIt, UserIt> users = usersByName.equal_range(unnormalizedName);
    for (UserIt userIt = users.first; userIt != users.second; ++userIt) {
      UserInfo* user = userIt->second;
      if (properties & UIP_NAME) {
        user->name = userNode["name"].str();  // Normalized name.
      }
      if (properties & UIP_EDIT_COUNT) {
        user->editCount = userNode["editcount"].numberAsInt();
      }
      if (properties & UIP_GROUPS) {
        const json::Value& groupsNode = userNode["groups"];
        user->groups = 0;
        for (const json::Value& group : groupsNode.array()) {
          const string& groupStr = group.str();
          if (groupStr == "autoconfirmed") {
            user->groups |= UIG_AUTOCONFIRMED;
          } else if (groupStr == "autopatrolled") {
            user->groups |= UIG_AUTOPATROLLED;
          } else if (groupStr == "sysop") {
            user->groups |= UIG_SYSOP;
          } else if (groupStr == "bot") {
            user->groups |= UIG_BOT;
          }
        }
      }
    }
  }
}

void Wiki::getUsersInfo(int properties, vector<UserInfo>& users) {
  unordered_multimap<string, UserInfo*> usersByName;
  vector<string> names;
  for (UserInfo& user : users) {
    if (!user.name.empty() && user.name.find('|') == string::npos) {
      // The API returns normalized user names, but with no matching from unnormalized to normalized names.
      // Thus, we rely on the fact that the response contains users in the same order as in the query, which is true
      // provided that exact duplicates are removed.
      if (usersByName.find(user.name) == usersByName.end()) {
        names.push_back(user.name);
      }
      usersByName.emplace(user.name, &user);
    }
  }
  for (StringRange namesRange : splitVectorIntoRanges(names, m_apiTitlesLimit)) {
    try {
      getUsersInfoOneRequest(*this, properties, namesRange, usersByName);
    } catch (WikiError& error) {
      error.addContext("Cannot read user info for " + quoteAndJoin(namesRange));
      throw;
    }
  };
}

vector<string> Wiki::getUsersInGroup(UserInfoGroups userGroup) {
  string userGroupStr = getStringOfUserGroup(userGroup);
  WikiListPager pager("allusers", "aulimit");
  pager.setParam("augroup", userGroupStr);

  try {
    return pager.runListPager<string>(*this,
                                      [](const json::Value& value, string& name) { name = value["name"].str(); });
  } catch (WikiError& error) {
    error.addContext("Cannot read the list of users in the group " + userGroupStr);
    throw;
  }
}

vector<string> Wiki::searchText(const string& query, int maxResults) {
  WikiListPager pager("search", "srlimit");
  pager.setParam("srprop", "size");
  pager.setParam("srsearch", query);
  // pager.setParamWithEmptyDefault("srnamespace", namespaces.toString());
  pager.setLimit(maxResults);

  try {
    return pager.runListPager<string>(*this, convertJSONToTitle);
  } catch (WikiError& error) {
    error.addContext("Text search failure");
    throw;
  }
}

vector<string> Wiki::getExtURLUsage(const string& url, int maxResults) {
  size_t protocolEnd = url.find("://");
  if (protocolEnd == string::npos) {
    throw InvalidParameterError("Protocol part missing in URL: " + url);
  }

  WikiListPager pager("exturlusage", "eulimit");
  pager.setParam("euprop", "title");
  pager.setParam("euprotocol", url.substr(0, protocolEnd));
  pager.setParam("euquery", url.substr(protocolEnd + 3));
  pager.setLimit(maxResults);

  try {
    return pager.runListPager<string>(*this, convertJSONToTitle);
  } catch (WikiError& error) {
    error.addContext("Cannot enumerate links to '" + url + "'");
    throw;
  }
}

}  // namespace mwc
