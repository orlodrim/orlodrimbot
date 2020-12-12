#ifndef MWC_REQUEST_H
#define MWC_REQUEST_H

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "cbl/date.h"
#include "cbl/json.h"
#include "wiki_base.h"
#include "wiki_defs.h"

namespace mwc {

struct FlagDef {
  int value;
  const char* name;
};

using StringRange = std::pair<const std::string*, const std::string*>;

// Splits v into ranges of size <= maxRangeSize.
std::vector<StringRange> splitVectorIntoRanges(const std::vector<std::string>& v, int maxRangeSize);

// Helper function for building error messages.
std::string quoteAndJoin(StringRange range);

cbl::Date parseAPITimestamp(const std::string& timestamp);

// Base class for all requests. Can be used directly for a basic request that can be executed in a single call and does
// not require a token.
class WikiRequest {
public:
  enum Method {
    METHOD_GET,
    // POST is used only to avoid very long URLs. The request will not cause any change.
    METHOD_POST_NO_SIDE_EFFECT,
    // The request will cause a change on the wiki, but it is safe to retry if something goes wrong, even if we cannot
    // be sure whether the previous attempt worked (e.g. in case of network error).
    METHOD_POST_IDEMPOTENT,
    // The request will cause a change on the wiki and retrying it is not safe (example: appending some text to a page).
    METHOD_POST,
  };
  explicit WikiRequest(const std::string& action);
  virtual ~WikiRequest() {}
  void setMethod(Method method);
  void setParam(const std::string& param, const std::string& value);
  void setParam(const std::string& param, int value);
  void setParam(const std::string& param, revid_t value) = delete;
  // Sets `param` to `revid` if `revid` is non-zero.
  void setRevidParam(const std::string& param, revid_t revid);
  void setParam(const std::string& param, const cbl::Date& value);
  void setParam(const std::string& param, EventsDir direction);
  // Sets `param` to `value` if the specified condition is true.
  void setOrClearParam(const std::string& param, const std::string& value, bool setCondition);
  // Sets `param` to `value` if `value` is not empty.
  void setParamWithEmptyDefault(const std::string& param, const std::string& value);
  // Sets a parameter represented by a string of the form flag1|flag2|...|flagN in MediaWiki API and as a bitwise
  // combination of flags in this library.
  template <int size>
  void setFlagsParam(const std::string& param, int flags, const FlagDef (&flagDefs)[size],
                     const char* extraFlags = nullptr) {
    setParamWithEmptyDefault(param, convertFlagsToString(flags, flagDefs, flagDefs + size, extraFlags));
  }
  void clearParam(const std::string& param);
  // Returns all parameters encoded as a query string. This is normally called by run(), but also exposed for debugging
  // purposes.
  std::string getRequestString() const;
  // Runs the request and returns its result.
  // Throws: APIError, LowLevelError.
  json::Value run(WikiBase& wiki);

private:
  static std::string convertFlagsToString(int flags, const FlagDef* begin, const FlagDef* end, const char* extraFlags);

  std::map<std::string, std::string> m_fields;
  Method m_method = METHOD_GET;
};

// Request that requires a token.
class WikiWriteRequest : public WikiRequest {
public:
  WikiWriteRequest(const std::string& action, TokenType tokenType);
  // Runs the query after fetching a token. In some cases, a cached token may be used.
  json::Value setTokenAndRun(WikiBase& wiki);

private:
  TokenType m_tokenType;
};

constexpr char NO_LIMIT_PARAM[] = "";

// Subclass for requests that may need to be repeated until all results are read.
// Should not be used directly. Use WikiPropPager or WikiListPager instead.
class WikiPager : public WikiRequest {
public:
  // If there is no explicit limit parameter, limitParam should be set to NO_LIMIT_PARAM.
  explicit WikiPager(const std::string& limitParam);
  void setLimit(int limit);
  // Opaque string to get the next results of the same request, in case a finite limite was set.
  const std::string& queryContinue() const;
  void setQueryContinue(const std::string& value);
  void runPager(WikiBase& wiki, const std::function<int(const json::Value& answer)>& callback);

private:
  void setContinue(const json::Value& value);

  std::string m_limitParam;
  int m_limit = PAGER_ALL;
  std::string m_queryContinue;
};

// Subclass for requests of the form action=query&prop=...
class WikiPropPager : public WikiPager {
public:
  WikiPropPager(const std::string& prop, const std::string& limitParam);
};

// Subclass for requests of the form action=query&list=...
class WikiListPager : public WikiPager {
public:
  WikiListPager(const std::string& list, const std::string& limitParam);

  template <class T, class Callback>
  std::vector<T> runListPager(WikiBase& wiki, Callback callback) {
    std::vector<T> results;
    runPager(wiki, [&](const json::Value& answer) {
      const json::Value& resultsNode = answer["query"][m_list];
      if (!resultsNode.isArray()) {
        throw UnexpectedAPIResponseError("Unexpected API response: 'query." + m_list + "' is not an array");
      }
      int oldSize = results.size();
      int numResults = resultsNode.array().size();
      results.resize(oldSize + numResults);
      for (int i = 0; i < numResults; i++) {
        callback(resultsNode[i], results[oldSize + i]);
      }
      return numResults;
    });
    return results;
  }

private:
  std::string m_list;
};

}  // namespace mwc

#endif
