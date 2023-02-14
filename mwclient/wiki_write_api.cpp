// IMPLEMENTS: wiki.h
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>
#include "cbl/date.h"
#include "cbl/json.h"
#include "cbl/log.h"
#include "request.h"
#include "wiki.h"
#include "wiki_base.h"
#include "wiki_defs.h"

using cbl::Date;
using std::string;
using std::vector;

namespace mwc {

static const char* getStringOfTokenType(TokenType type) {
  switch (type) {
    case TOK_CSRF:
      return "csrf";
    case TOK_WATCH:
      return "watch";
    case TOK_LOGIN:
      return "login";
    case TOK_MAX:
      break;  // invalid
  }
  throw std::invalid_argument("getStringOfTokenType called with invalid type " + std::to_string(type));
}

static const char* getStringOfPageProtectionType(PageProtectionType type) {
  switch (type) {
    case PRT_EDIT:
      return "edit";
    case PRT_MOVE:
      return "move";
    case PRT_UPLOAD:
      return "upload";
    case PRT_CREATE:
      return "create";
  }
  throw std::invalid_argument("getStringOfPageProtectionType called with invalid type " + std::to_string(type));
}

static const char* getStringOfPageProtectionLevel(PageProtectionLevel level) {
  switch (level) {
    case PRL_NONE:
      return "all";
    case PRL_AUTOCONFIRMED:
      return "autoconfirmed";
    case PRL_SYSOP:
      return "sysop";
    case PRL_AUTOPATROLLED:
      return "editextendedsemiprotected";
  }
  throw std::invalid_argument("getStringOfPageProtectionLevel called with invalid level " + std::to_string(level));
};

string Wiki::getTokenUncached(TokenType tokenType) {
  WikiRequest request("query");
  string tokenTypeStr = getStringOfTokenType(tokenType);
  string tokenName = tokenTypeStr + "token";
  request.setParam("meta", "tokens");
  request.setParam("type", tokenTypeStr);

  try {
    json::Value answer = request.run(*this);
    const json::Value& tokenValue = answer["query"]["tokens"][tokenName];
    if (tokenValue.isNull()) {
      throw UnexpectedAPIResponseError("'tokens." + tokenName + "' is missing in API response");
    }
    string token = tokenValue.str();
    constexpr size_t MIN_TOKEN_SIZE = 4;
    if (!m_internalUserName.empty() && token.size() < MIN_TOKEN_SIZE) {
      throw UnexpectedAPIResponseError("Token '" + token + "' is too short for a logged-in user");
    }
    return token;
  } catch (WikiError& error) {
    error.addContext("Cannot retrieve " + tokenName);
    throw;
  }
}

void Wiki::writePage(const string& title, const string& content, const WriteToken& writeToken, const string& summary,
                     int flags) {
  Date baseTimestamp;
  bool createOnly = false;
  switch (writeToken.type()) {
    case WriteToken::UNINITIALIZED:
      throw std::invalid_argument("Uninitialized writeToken passed to Wiki::writePage");
    case WriteToken::CREATE:
      createOnly = true;
      break;
    case WriteToken::EDIT:
      if (writeToken.title() != title) {
        throw std::invalid_argument("Cannot write page '" + title + "' with a WriteToken created for page '" +
                                    writeToken.title() + "'");
      }
      if (writeToken.needsNoBotsBypass() && !(flags & EDIT_BYPASS_NOBOTS)) {
        throw BotExclusionError("Cannot write page '" + title + "' because it contains a bot exclusion template");
      }
      baseTimestamp = writeToken.timestamp();
      break;
    case WriteToken::NO_CONFLICT_DETECTION:
      break;
  }
  if (!(flags & (EDIT_APPEND | EDIT_ALLOW_BLANKING)) & content.empty()) {
    throw InvalidParameterError("Empty content passed to Wiki::writePage while trying to write '" + title +
                                "'. If this is not a bug, pass EDIT_ALLOW_BLANKING in flags.");
  }

  WikiWriteRequest request("edit", TOK_CSRF);
  request.setMethod((flags & EDIT_APPEND) ? WikiRequest::METHOD_POST : WikiRequest::METHOD_POST_IDEMPOTENT);
  request.setParam("title", title);
  request.setParam("summary", summary);
  request.setOrClearParam("text", content, !(flags & EDIT_APPEND));
  request.setOrClearParam("appendtext", content, flags & EDIT_APPEND);
  request.setParam("watchlist", "nochange");
  request.setOrClearParam("minor", "1", flags & EDIT_MINOR);
  request.setOrClearParam("bot", "1", !(flags & EDIT_OMIT_BOT_FLAG));
  request.setOrClearParam("createonly", "1", createOnly);
  request.setParam("basetimestamp", baseTimestamp);

  try {
    json::Value answer = request.setTokenAndRun(*this);
    const string& editResult = answer["edit"]["result"].str();
    if (editResult != "Success") {
      throw APIError(APIError::CODELESS_ERROR, "Server returned unexpected code '" + editResult + "'");
    }
  } catch (WikiError& error) {
    error.addContext("Cannot write page '" + title + "'");
    throw;
  }
}

void Wiki::appendToPage(const string& title, const string& content, const string& summary, int flags) {
  writePage(title, content, WriteToken::newWithoutConflictDetection(), summary, flags | EDIT_APPEND);
}

void Wiki::editPage(const string& title, const std::function<void(string&)>& transformContent, const string& summary,
                    int flags) {
  bool editSuccessful = false;
  int attemptsLeft = 2;
  while (!editSuccessful) {
    WriteToken writeToken;
    string oldContent = readPageContentIfExists(title, &writeToken);
    string newContent = oldContent;
    transformContent(newContent);
    if (oldContent == newContent) {
      break;
    }
    try {
      writePage(title, newContent, writeToken, summary, flags);
      editSuccessful = true;
    } catch (const EditConflictError&) {
      if (attemptsLeft <= 1) {
        throw;
      }
      attemptsLeft--;
      CBL_WARNING << "Edit conflict detected on page '" << title << "', retrying (" << attemptsLeft
                  << " attempts left)";
    }
  }
}

void Wiki::movePage(const string& oldTitle, const string& newTitle, const string& summary, int flags) {
  WikiWriteRequest request("move", TOK_CSRF);
  request.setParam("from", oldTitle);
  request.setParam("to", newTitle);
  request.setParam("reason", summary);
  request.setParam("watchlist", "nochange");
  request.setOrClearParam("movetalk", "", flags & MOVE_MOVETALK);
  request.setOrClearParam("noredirect", "", flags & MOVE_NOREDIRECT);

  try {
    request.setTokenAndRun(*this);
  } catch (WikiError& error) {
    error.addContext("Cannot move page '" + oldTitle + "' to '" + newTitle + "'");
    throw;
  }
}

void Wiki::setPageProtection(const string& title, const vector<PageProtection>& protections, const string& reason) {
  string protectionsValue;
  string expiryValue;
  for (const PageProtection& protection : protections) {
    if (!protectionsValue.empty()) {
      protectionsValue += '|';
      expiryValue += '|';
    }
    protectionsValue += getStringOfPageProtectionType(protection.type);
    protectionsValue += '=';
    protectionsValue += getStringOfPageProtectionLevel(protection.level);
    if (protection.expiry.isNull()) {
      expiryValue += "infinite";
    } else {
      expiryValue += protection.expiry.toISO8601();
    }
  }

  WikiWriteRequest request("protect", TOK_CSRF);
  request.setParam("title", title);
  request.setParam("reason", reason);
  request.setParam("watchlist", "nochange");
  request.setParam("protections", protectionsValue);
  request.setParam("expiry", expiryValue);

  try {
    request.setTokenAndRun(*this);
  } catch (WikiError& error) {
    error.addContext("Cannot protect page '" + title + "'");
    throw;
  }
}

void Wiki::deletePage(const string& title, const string& reason) {
  WikiWriteRequest request("delete", TOK_CSRF);
  request.setParam("title", title);
  request.setParam("reason", reason);
  request.setParam("watchlist", "nochange");

  try {
    request.setTokenAndRun(*this);
  } catch (WikiError& error) {
    error.addContext("Cannot delete page '" + title + "'");
    throw;
  }
}

void Wiki::purgePage(const string& title) {
  WikiRequest request("purge");
  request.setMethod(WikiRequest::METHOD_POST_IDEMPOTENT);
  request.setParam("titles", title);

  try {
    json::Value answer = request.run(*this);
    const json::Value& purgeResult = answer["purge"][0];
    if (!purgeResult.has("purged")) {
      if (purgeResult.has("missing")) {
        throw PageNotFoundError("The page does not exist");
      } else if (purgeResult.has("iw")) {
        throw InvalidParameterError("Invalid title (interwiki)");
      }
      throw UnexpectedAPIResponseError("No 'purged' member in purge result " + purgeResult.toJSON());
    }
  } catch (WikiError& error) {
    error.addContext("Cannot purge page '" + title + "'");
    throw;
  }
}

void Wiki::emailUser(const string& user, const string& subject, const string& text, bool ccme) {
  WikiWriteRequest request("emailuser", TOK_CSRF);
  request.setParam("target", user);
  request.setParam("subject", subject);
  request.setParam("text", text);
  request.setOrClearParam("ccme", "1", ccme);

  try {
    request.setTokenAndRun(*this);
  } catch (WikiError& error) {
    error.addContext("Cannot send e-mail to '" + user + "'");
    throw;
  }
}

void Wiki::flowNewTopic(const string& title, const string& topic, const string& content, int flags) {
  WikiWriteRequest request("flow", TOK_CSRF);
  request.setParam("submodule", "new-topic");
  request.setParam("page", title);
  request.setParam("nttopic", topic);
  request.setParam("ntcontent", content);

  try {
    request.setTokenAndRun(*this);
  } catch (WikiError& error) {
    error.addContext("Cannot create flow topic on '" + title + "'");
    throw;
  }
}

string Wiki::getToken(TokenType tokenType) {
  if (!(tokenType >= 0 && tokenType < TOK_MAX)) {
    throw std::invalid_argument("getToken called with invalid tokenType " + std::to_string(tokenType));
  }
  string localBuffer;
  // Cache tokens of all types except TOK_LOGIN.
  string& token = tokenType == TOK_LOGIN ? localBuffer : m_tokenCache[tokenType];
  if (token.empty()) {
    token = getTokenUncached(tokenType);
  }
  return token;
}

void Wiki::clearTokenCache() {
  for (int i = 0; i < TOK_MAX; i++) {
    m_tokenCache[i].clear();
  }
}

bool Wiki::isEmergencyStopTriggered() {
  return m_emergencyStopTest ? m_emergencyStopTest() : false;
}

void Wiki::setEmergencyStopTest(const EmergencyStopTest& emergencyStopTest) {
  m_emergencyStopTest = emergencyStopTest;
}

void Wiki::enableDefaultEmergencyStopTest() {
  Date initializationDate = Date::now() - cbl::DateDiff::fromMinutes(1);
  m_emergencyStopTest = [this, initializationDate]() {
    if (m_externalUserName.empty()) {
      throw InvalidStateError("Emergency stop works only for logged in users");
    }
    string stopPage = "User talk:" + m_externalUserName;
    return readPage(stopPage, RP_TIMESTAMP).timestamp >= initializationDate;
  };
}

void Wiki::clearEmergencyStopTest() {
  m_emergencyStopTest = []() { return false; };
}

}  // namespace mwc
