#ifndef MWC_WIKI_BASE_H
#define MWC_WIKI_BASE_H

#include <time.h>
#include <climits>
#include <string>
#include "cbl/json.h"

namespace cbl {
class HTTPClient;
}  // namespace cbl

namespace mwc {

constexpr int BASIC_API_LIMIT = 500;
constexpr int BASIC_API_TITLES_LIMIT = 50;
constexpr int HIGH_API_LIMIT = 5000;
constexpr int HIGH_API_TITLES_LIMIT = 500;
constexpr int DISABLE_MAX_LAG = INT_MAX;

enum TokenType {
  TOK_CSRF,
  TOK_WATCH,
  TOK_LOGIN,
  TOK_MAX,
};

// Base class of Wiki containing the low-level functions to send arbitrary requests as strings to the MediaWiki API.
// This corresponds to the subset of functions that WikiRequest has access to.
// The two main functions, apiRequest and its wrapper apiGetRequest wrapper, are implemented directly in wiki_base.cpp.
// This layer is not fully isolated from the one above (the Wiki class). Some hooks to call higher level functions are
// present as pure virtual functions. This includes everything related to the user session state (for instance, if the
// users unexpectedly gets logged out during a request, the high-level function retryToLogIn is called to try to restore
// the session).
class WikiBase {
public:
  WikiBase() = default;
  WikiBase(const WikiBase&) = delete;
  virtual ~WikiBase() = default;
  WikiBase& operator=(const WikiBase&) = delete;

  int apiLimit() const { return m_apiLimit; }
  int apiTitlesLimit() const { return m_apiTitlesLimit; }
  virtual const std::string& internalUserName() const = 0;
  virtual std::string getToken(TokenType tokenType) = 0;
  virtual void clearTokenCache() = 0;
  virtual void sleep(int seconds);
  // Waits until m_delayBetweenEdits seconds have elapsed since m_lastEdit.
  virtual void waitBeforeEdit();
  virtual bool isEmergencyStopTriggered() = 0;
  // Throws: WikiError and subclasses.
  virtual json::Value apiRequest(const std::string& request, const std::string& data, bool canRetry);
  virtual json::Value apiGetRequest(const std::string& request);
  virtual bool retryToLogIn() = 0;

protected:
  virtual cbl::HTTPClient& httpClient() = 0;

  // Location of api.php and index.php. Example: "https://en.wikipedia.org/w".
  // This must be set to a non-empty value before using apiRequest/apiGetRequest.
  std::string m_wikiURL;
  int m_maxLag = 5;
  int m_delayBetweenEdits = 12;
  int m_apiLimit = BASIC_API_LIMIT;
  int m_apiTitlesLimit = BASIC_API_TITLES_LIMIT;
  time_t m_lastEdit = 0;
};

}  // namespace mwc

#endif
