#include "request.h"
#include <algorithm>
#include <functional>
#include <string>
#include <unordered_set>
#include <vector>
#include "cbl/date.h"
#include "cbl/error.h"
#include "cbl/json.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "wiki_base.h"
#include "wiki_defs.h"

using cbl::Date;
using std::string;
using std::unordered_set;
using std::vector;

namespace mwc {

vector<StringRange> splitVectorIntoRanges(const vector<string>& v, int maxRangeSize) {
  vector<StringRange> ranges;
  int size = v.size();
  for (int rangeBegin = 0; rangeBegin < size; rangeBegin += maxRangeSize) {
    const string* begin = &v[rangeBegin];
    ranges.emplace_back(begin, begin + std::min(maxRangeSize, size - rangeBegin));
  }
  return ranges;
}

string quoteAndJoin(StringRange range) {
  string result;
  for (const string* item = range.first; item < range.second; item++) {
    if (item > range.first) {
      result += ", ";
    }
    result += '\'';
    result += *item;
    result += '\'';
  }
  return result;
}

Date parseAPITimestamp(const string& timestamp) {
  try {
    return Date::fromISO8601OrEmpty(timestamp);
  } catch (const cbl::ParseError&) {
    throw UnexpectedAPIResponseError("Unexpected API response: '" + timestamp + "' is not a valid ISO8601 date");
  }
}

/* == WikiRequest == */

string WikiRequest::convertFlagsToString(int flags, const FlagDef* begin, const FlagDef* end, const char* extraFlags) {
  string result;
  if (extraFlags) {
    result = extraFlags;
  }
  for (const FlagDef* flagDef = begin; flagDef != end; flagDef++) {
    if (flags & flagDef->value) {
      if (!result.empty()) {
        result += '|';
      }
      result += flagDef->name;
      flags &= ~flagDef->value;
    }
  }
  return result;
}

WikiRequest::WikiRequest(const string& action) {
  setParam("action", action);
}

void WikiRequest::setMethod(Method method) {
  m_method = method;
}

void WikiRequest::setParam(const string& param, const string& value) {
  m_fields[param] = value;
}

void WikiRequest::setParam(const string& param, int value) {
  setParam(param, std::to_string(value));
}

void WikiRequest::setRevidParam(const string& param, revid_t value) {
  setOrClearParam(param, std::to_string(value), value != 0);
}

void WikiRequest::setParam(const string& param, const Date& value) {
  setOrClearParam(param, value.toISO8601(), !value.isNull());
}

void WikiRequest::setParam(const string& param, EventsDir direction) {
  setOrClearParam(param, "newer", direction == OLDEST_FIRST);
}

void WikiRequest::setOrClearParam(const string& param, const string& value, bool setCondition) {
  if (setCondition) {
    setParam(param, value);
  } else {
    clearParam(param);
  }
}

void WikiRequest::setParamWithEmptyDefault(const string& param, const string& value) {
  setOrClearParam(param, value, !value.empty());
}

void WikiRequest::clearParam(const string& param) {
  m_fields.erase(param);
}

string WikiRequest::getRequestString() const {
  string request;
  for (const auto& [param, value] : m_fields) {
    if (!request.empty()) request += '&';
    cbl::encodeURIComponentCat(param, request);
    request += '=';
    cbl::encodeURIComponentCat(value, request);
  }
  return request;
}

json::Value WikiRequest::run(WikiBase& wiki) {
  string request = getRequestString();
  switch (m_method) {
    case METHOD_GET:
      return wiki.apiGetRequest(request);
    case METHOD_POST_NO_SIDE_EFFECT:
    case METHOD_POST_IDEMPOTENT:
      return wiki.apiRequest("", request, true);
    case METHOD_POST:
      return wiki.apiRequest("", request, false);
  }
  throw cbl::InternalError("Invalid m_method");
}

/* == WikiWriteRequest == */

WikiWriteRequest::WikiWriteRequest(const string& action, TokenType tokenType)
    : WikiRequest(action), m_tokenType(tokenType) {
  setMethod(METHOD_POST);
}

json::Value WikiWriteRequest::setTokenAndRun(WikiBase& wiki) {
  wiki.waitBeforeEdit();
  int editTokenRenewed = 0;
  while (true) {
    string token = wiki.getToken(m_tokenType);
    setParam("token", token);

    bool emergencyStop = false;
    try {
      emergencyStop = wiki.isEmergencyStopTriggered();
    } catch (WikiError& error) {
      error.addContext("Error while checking emergency stop");
      throw;
    }
    if (emergencyStop) {
      throw EmergencyStopError("Emergency stop");
    }

    try {
      json::Value answer = WikiRequest::run(wiki);
      CBL_INFO << answer;
      return answer;
    } catch (const APIError& error) {
      if (error.code() == "badtoken" && editTokenRenewed < 2) {
        wiki.clearTokenCache();
        if (editTokenRenewed == 0) {
          CBL_WARNING << "Token '" << token << "' rejected, new token needed";
        } else {
          CBL_WARNING << "Second token '" << token << "' rejected, trying to log in again";
          wiki.retryToLogIn();
        }
        editTokenRenewed++;
        // Try again.
      } else {
        throw;
      }
    }
  }
}

/* == WikiPager == */

WikiPager::WikiPager(const string& limitParam) : WikiRequest("query"), m_limitParam(limitParam) {}

void WikiPager::setLimit(int limit) {
  m_limit = limit;
}

const string& WikiPager::queryContinue() const {
  return m_queryContinue;
}

void WikiPager::setQueryContinue(const string& value) {
  m_queryContinue = value;
}

void WikiPager::runPager(WikiBase& wiki, const std::function<int(const json::Value& answer)>& callback) {
  if (!m_queryContinue.empty()) {
    try {
      setContinue(json::parse(m_queryContinue));
    } catch (const cbl::ParseError&) {
      throw InvalidParameterError("Failed to pass the continue parameter as JSON: '" + m_queryContinue + "'");
    }
  }
  int apiLimit = wiki.apiLimit();
  unordered_set<string> m_previousRequests;
  for (int leftToRead = m_limit; leftToRead != 0;) {
    if (m_limitParam != NO_LIMIT_PARAM) {
      setParam(m_limitParam, leftToRead == PAGER_ALL ? apiLimit : std::min(apiLimit, leftToRead));
    }
    string request = getRequestString();
    if (m_previousRequests.count(request) != 0) {
      throw UnexpectedAPIResponseError("Request caused an infinite loop in the pager: " + request);
    }
    m_previousRequests.insert(request);
    // LOG(INFO, "Pager request: " << request);
    json::Value answer = run(wiki);
    // LOG(INFO, "Answer: " + answer.toJSON());
    int numItemsRead = callback(answer);
    if (leftToRead != PAGER_ALL) {
      leftToRead = std::max(leftToRead - numItemsRead, 0);
    }
    if (!answer.has("continue")) {
      m_queryContinue.clear();
      break;
    }
    m_queryContinue = answer["continue"].toJSON();
    setContinue(answer["continue"]);
  }
}

void WikiPager::setContinue(const json::Value& value) {
  for (const auto& [continueName, continueValue] : value) {
    setParam(continueName, continueValue.str());
  }
}

WikiPropPager::WikiPropPager(const string& prop, const string& limitParam) : WikiPager(limitParam) {
  setParam("prop", prop);
}

WikiListPager::WikiListPager(const string& list, const string& limitParam) : WikiPager(limitParam), m_list(list) {
  setParam("list", list);
}

}  // namespace mwc
