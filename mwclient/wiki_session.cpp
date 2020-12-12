// IMPLEMENTS: wiki.h
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include "cbl/date.h"
#include "cbl/error.h"
#include "cbl/file.h"
#include "cbl/http_client.h"
#include "cbl/json.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "request.h"
#include "site_info.h"
#include "wiki.h"
#include "wiki_base.h"
#include "wiki_defs.h"

using cbl::Date;
using std::string;
using std::string_view;

namespace mwc {

static string readLineFromStdin(const string& prompt) {
  std::cout << prompt;
  string line;
  std::getline(std::cin, line);
  return line;
}

// == Session ==

void Wiki::clearSession() {
  m_apiLimit = BASIC_API_LIMIT;
  m_apiTitlesLimit = BASIC_API_TITLES_LIMIT;
  m_wikiURL.clear();
  setInternalUserName("");
  m_password.clear();
  m_httpClient->clearCookies();
  m_sessionFile.clear();
  clearTokenCache();
}

string Wiki::sessionToString() const {
  string cookies = m_httpClient->cookies();
  string siteInfoStr = m_siteInfo.toJSONValue().toJSON();

  string buffer;
  buffer = "url=" + m_wikiURL + "\n";
  buffer += "user=" + m_internalUserName + "\n";
  buffer += "session=" + cookies + "\n";
  buffer += "siteinfo=" + siteInfoStr + "\n";
  return buffer;
}

void Wiki::sessionToFile(const string& fileName) const {
  try {
    cbl::writeFileAtomically(fileName, sessionToString());
  } catch (const cbl::SystemError& error) {
    CBL_ERROR << "Error while saving the session: " << error.what();
  }
}

void Wiki::sessionFromString(const string& buffer) {
  clearSession();

  for (string_view line : cbl::splitLines(buffer)) {
    size_t equal = line.find('=');
    if (equal == string_view::npos) {
      throw cbl::ParseError("Invalid line '" + string(line) + "' in the input of Wiki::sessionFromString");
    }
    string_view param = line.substr(0, equal);
    string_view value = line.substr(equal + 1);
    if (param == "url") {
      m_wikiURL = value;
    } else if (param == "user") {
      setInternalUserName(value);
      if (m_internalUserName.empty()) {
        m_apiLimit = BASIC_API_LIMIT;
        m_apiTitlesLimit = BASIC_API_TITLES_LIMIT;
      } else {
        m_apiLimit = HIGH_API_LIMIT;
        m_apiTitlesLimit = HIGH_API_TITLES_LIMIT;
      }
    } else if (param == "session") {
      m_httpClient->setCookies(value);
    } else if (param == "siteinfo") {
      m_siteInfo.fromJSONValue(json::parse(value));  // May throw cbl::ParseError.
    }
  }
  if (m_wikiURL.empty()) {
    throw cbl::ParseError("Missing url in the input of Wiki::sessionFromString");
  }
}

void Wiki::sessionFromFile(const string& fileName) {
  string buffer = cbl::readFile(fileName);
  sessionFromString(buffer);
  m_sessionFile = fileName;
}

void Wiki::loadSiteInfo() {
  WikiRequest request("query");
  request.setParam("meta", "siteinfo");
  request.setParam("siprop", "namespaces|namespacealiases|interwikimap|magicwords");
  json::Value answer = request.run(*this);
  try {
    m_siteInfo.fromAPIResponse(answer["query"]);
  } catch (const cbl::ParseError& error) {
    throw UnexpectedAPIResponseError("Unexpected response from meta=siteinfo: " + string(error.what()));
  }
}

void Wiki::loginInternal(string userName, const string& password, bool clientLogin) {
  string cookies;
  try {
    string oldInternalUserName = m_internalUserName;
    cbl::RunOnDestroy cleanup([this, oldInternalUserName]() {
      m_internalUserName = oldInternalUserName;
      m_httpClient->setRemoteCookiesEnabled(false);
    });

    m_httpClient->clearCookies();
    m_httpClient->setRemoteCookiesEnabled(true);
    // Do not send an "assert=user" parameter.
    m_internalUserName.clear();

    CBL_INFO << "Logging in as " << userName;
    string token = getToken(TOK_LOGIN);
    if (clientLogin) {
      string loginResult;
      {
        WikiRequest request("clientlogin");
        request.setMethod(WikiRequest::METHOD_POST_NO_SIDE_EFFECT);
        request.setParam("username", userName.substr(0, userName.rfind('@')));
        request.setParam("password", !password.empty() ? password : readLineFromStdin("Password: "));
        request.setParam("logintoken", token);
        request.setParam("loginreturnurl", "https://127.0.0.1/unused");
        json::Value answer = request.run(*this);
        CBL_INFO << "Login response: " << answer;
        loginResult = answer["clientlogin"]["status"].str();
      }

      if (loginResult == "UI") {
        WikiRequest request("clientlogin");
        request.setMethod(WikiRequest::METHOD_POST_NO_SIDE_EFFECT);
        request.setParam("logincontinue", "1");
        request.setParam("logintoken", token);
        request.setParam("OATHToken", readLineFromStdin("One-time token: "));
        json::Value answer = request.run(*this);
        CBL_INFO << "Login response: " << answer;
        loginResult = answer["clientlogin"]["status"].str();
      }

      if (loginResult != "PASS") {
        throw APIError(APIError::CODELESS_ERROR, "Client login failed with code '" + loginResult + "'");
      }
    } else {
      WikiRequest request("login");
      request.setMethod(WikiRequest::METHOD_POST_NO_SIDE_EFFECT);
      request.setParam("lgname", userName);
      request.setParam("lgpassword", password);
      request.setParam("lgtoken", token);
      json::Value answer = request.run(*this);
      const string& loginResult = answer["login"]["result"].str();
      if (loginResult != "Success") {
        throw APIError(APIError::CODELESS_ERROR, "Server returned unexpected code '" + loginResult + "'");
      }
    }

    cookies = m_httpClient->getRemoteCookies();
    // m_httpClient->setRemoteCookiesEnabled(false) is called by RunOnDestroy.
  } catch (WikiError& error) {
    error.addContext("Cannot log in as '" + userName + "'");
    throw;
  }
  m_httpClient->setCookies(cookies);
}

void Wiki::logIn(const LoginParams& loginParams, const string& sessionFile) {
  if (loginParams.url.empty()) {
    throw std::invalid_argument("'url' field of loginParams must not be empty");
  }

  bool fullyInitialized = false;
  cbl::RunOnDestroy cleanupOnError([this, &fullyInitialized]() {
    if (!fullyInitialized) {
      clearSession();
    }
  });

  if (!loginParams.userAgent.empty()) {
    m_httpClient->setUserAgent(loginParams.userAgent);
  }
  if (loginParams.delayBeforeRequests != -1 && !m_delayBeforeRequestsOverridden) {
    m_httpClient->setDelayBeforeRequests(loginParams.delayBeforeRequests);
  }
  if (loginParams.delayBetweenEdits != -1 && !m_delayBetweenEditsOverridden) {
    m_delayBetweenEdits = loginParams.delayBetweenEdits;
  }
  m_maxLag = loginParams.maxLag;
  m_lastEdit = Date::now().toTimeT();

  string url = loginParams.url;
  if (cbl::endsWith(url, "/")) {
    url.pop_back();
  }

  if (!sessionFile.empty()) {
    try {
      sessionFromFile(sessionFile);  // May throw a ParseError.
      if (m_wikiURL != url) {
        throw cbl::ParseError("URL is different from the one defined in login parameters");
      } else if (m_internalUserName != loginParams.userName) {
        throw cbl::ParseError("User is different from the one defined in login parameters");
      }
      m_password = loginParams.password;
      fullyInitialized = true;
      return;
    } catch (const cbl::FileNotFoundError&) {
      // The file does not exist yet, just keep going.
    } catch (const cbl::SystemError& error) {
      CBL_ERROR << error.what();
    } catch (const cbl::ParseError& error) {
      CBL_ERROR << "Ignoring the existing session file '" + sessionFile + "' because of the following error: "
                << error.what();
    }
  }

  clearSession();

  m_wikiURL = std::move(url);

  if (!loginParams.userName.empty()) {
    loginInternal(loginParams.userName, loginParams.password, loginParams.clientLogin);
    setInternalUserName(loginParams.userName);
    if (!loginParams.clientLogin) {
      // The password may be used later by retryToLogIn(), but this only works when clientLogin is false.
      m_password = loginParams.password;
    }
    // TODO: Check if this is a bot/sysop account or not.
    m_apiLimit = HIGH_API_LIMIT;
    m_apiTitlesLimit = HIGH_API_TITLES_LIMIT;
  }

  if (loginParams.readSiteInfo) {
    loadSiteInfo();
  } else {
    m_siteInfo = SiteInfo();
  }

  if (!sessionFile.empty()) {
    m_sessionFile = sessionFile;
    sessionToFile(sessionFile);
  }

  fullyInitialized = true;
}

void Wiki::logIn(const string& url, const string& userName, const string& password, const string& sessionFile) {
  LoginParams loginParams;
  loginParams.url = url;
  loginParams.userName = userName;
  loginParams.password = password;
  logIn(loginParams, sessionFile);
}

bool Wiki::retryToLogIn() {
  if (m_internalUserName.empty() || m_password.empty()) {
    return false;
  }

  CBL_WARNING << "Disconnected, trying to log in again";
  try {
    loginInternal(m_internalUserName, m_password, /* clientLogin = */ false);
  } catch (const WikiError& error) {
    CBL_WARNING << error.what();
    return false;
  }

  if (!m_sessionFile.empty()) {
    sessionToFile(m_sessionFile);
  }
  return true;
}

void Wiki::setInternalUserName(string_view userName) {
  m_internalUserName = string(userName);
  size_t mainAccountLength = m_internalUserName.find('@');
  if (mainAccountLength == string::npos) {
    m_externalUserName = m_internalUserName;
  } else {
    m_externalUserName = m_internalUserName.substr(0, mainAccountLength);
  }
}

}  // namespace mwc
