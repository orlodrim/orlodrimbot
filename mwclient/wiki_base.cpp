#include "wiki_base.h"
#include <time.h>
#include <unistd.h>
#include <algorithm>
#include <functional>
#include <string>
#include "cbl/date.h"
#include "cbl/error.h"
#include "cbl/http_client.h"
#include "cbl/json.h"
#include "cbl/log.h"
#include "wiki_defs.h"

using cbl::Date;
using std::string;

namespace mwc {

static bool isProtectedPageError(const string& errorCode) {
  return errorCode == "protectedpage" || errorCode == "protectednamespace-interface" ||
         errorCode == "protectednamespace" || errorCode == "customcssjsprotected" || errorCode == "cascadeprotected";
}

void WikiBase::sleep(int seconds) {
  ::sleep(seconds);
}

void WikiBase::waitBeforeEdit() {
  time_t now = Date::now().toTimeT();
  m_lastEdit = std::min(m_lastEdit, now);  // Do not assume that the clock always moves forward.
  time_t minWriteTime = m_lastEdit + m_delayBetweenEdits;
  if (now < minWriteTime) {
    sleep(minWriteTime - now);
    now = minWriteTime;
  }
  m_lastEdit = now;
}

json::Value WikiBase::apiRequest(const string& request, const string& data, bool canRetry) {
  if (m_wikiURL.empty()) {
    throw InvalidStateError("Not connected to a wiki");
  }
  string url = m_wikiURL + "/api.php?format=json";
  if (!request.empty()) {
    url += '&';
    url += request;
  }
  if (m_maxLag != DISABLE_MAX_LAG) {
    url += "&maxlag=";
    url += std::to_string(m_maxLag);
  }
  if (!internalUserName().empty()) {
    url += "&assert=user";
  }

  constexpr int MAX_ATTEMPTS = 5;
  int remainingAttempts = canRetry ? MAX_ATTEMPTS : 1;
  int retryTime = 30;
  bool logInRetried = false;
  bool postRequest = !data.empty();

  std::function<json::Value()> doOneAttempt = [&]() {
    string rawAnswer;
    try {
      if (postRequest) {
        rawAnswer = httpClient().post(url, data);
      } else {
        rawAnswer = httpClient().get(url);
      }
    } catch (const cbl::NetworkError& error) {
      throw LowLevelError(LowLevelError::NETWORK, error.what());
    } catch (const cbl::HTTPServerError& error) {
      throw LowLevelError(LowLevelError::HTTP, error.what());
    }
    json::Value answer;
    try {
      answer = json::parse(rawAnswer);
    } catch (const cbl::ParseError& error) {
      throw LowLevelError(LowLevelError::JSON_PARSING, string("Cannot parse JSON: ") + error.what());
    }
    if (answer.has("error")) {
      const json::Value& error = answer["error"];
      const string& errorCode = error["code"].str();
      string description;
      if (error.has("info")) {
        description = error["info"].str();
        if (!errorCode.empty()) {
          description += " (API error code: '" + errorCode + "')";
        }
      } else if (!errorCode.empty()) {
        description = errorCode;
      } else {
        description = "Unknown API error";
      }
      if (errorCode == "maxlag") {
        remainingAttempts++;
        throw LowLevelError(LowLevelError::UNSPECIFIED, "Server lagged");
      } else if (errorCode == "assertuserfailed") {
        if (!logInRetried && retryToLogIn()) {
          remainingAttempts++;
          retryTime = 0;
          logInRetried = true;
          throw LowLevelError(LowLevelError::UNSPECIFIED, description);
        }
      } else if (errorCode == "readonly") {
        throw LowLevelError(LowLevelError::READ_ONLY_WIKI, "Wiki in read-only mode");
      } else if (errorCode == "editconflict") {
        throw EditConflictError("Edit conflict");
      } else if (errorCode == "articleexists") {
        throw PageAlreadyExistsError("The page already exists");
      } else if (errorCode == "missingtitle") {
        throw PageNotFoundError("The page does not exist");
      } else if (errorCode == "invalidtitle") {
        throw InvalidParameterError("Invalid title");
      } else if (isProtectedPageError(errorCode)) {
        throw ProtectedPageError("Protected page");
      }
      throw APIError(errorCode, description);
    }
    if (answer.has("warnings")) {
      CBL_WARNING << answer["warnings"];
    }
    return answer;
  };

  while (true) {
    try {
      return doOneAttempt();
    } catch (const LowLevelError& error) {
      remainingAttempts--;
      if (remainingAttempts <= 0) {
        throw;
      }
      if (retryTime > 0) {
        CBL_WARNING << error.what() << " (will try again in " << retryTime << " seconds)";
        sleep(retryTime);
      }
      retryTime = std::min(retryTime + 30, 600);
    }
  }
}

json::Value WikiBase::apiGetRequest(const std::string& request) {
  return apiRequest(request, "", true);
}

}  // namespace mwc
