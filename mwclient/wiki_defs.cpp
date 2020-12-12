#include "wiki_defs.h"
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "cbl/date.h"
#include "cbl/error.h"
#include "cbl/log.h"
#include "cbl/string.h"

using cbl::Date;
using std::string;
using std::string_view;
using std::vector;

namespace mwc {

const string EMPTY_STRING;
constexpr Date NULL_DATE;

const char* WikiError::what() const noexcept {
  return m_messageWithContext.empty() ? Error::what() : m_messageWithContext.c_str();
}

void WikiError::addContext(const std::string& context) {
  if (context.empty()) return;
  m_messageWithContext = context + ": " + what();
}

// TODO: Replace this with a function parseRevid that validates the input.
revid_t revidOfString(const string& s) {
  return atoll(s.c_str());
}

string NamespaceList::toString() const {
  string buffer;
  for (int namespace_ : m_namespaces) {
    if (!buffer.empty()) {
      buffer += '|';
    }
    buffer += std::to_string(namespace_);
  }
  return buffer;
}

void RecentChange::setType(RecentChangeType newType) {
  m_type = newType;
  m_revision.reset();
  m_logEvent.reset();
  switch (newType) {
    case RC_UNDEFINED:
      break;
    case RC_EDIT:
    case RC_NEW:
      m_revision = std::make_unique<Revision>();
      break;
    case RC_LOG:
      m_logEvent = std::make_unique<LogEvent>();
      break;
  }
}

const Revision& RecentChange::revision() const {
  CBL_ASSERT(m_revision != nullptr);
  return *m_revision;
}

Revision& RecentChange::mutableRevision() {
  CBL_ASSERT(m_revision != nullptr);
  return *m_revision;
}

const LogEvent& RecentChange::logEvent() const {
  CBL_ASSERT(m_logEvent != nullptr);
  return *m_logEvent;
}

LogEvent& RecentChange::mutableLogEvent() {
  CBL_ASSERT(m_logEvent != nullptr);
  return *m_logEvent;
}

const string& RecentChange::title() const {
  switch (m_type) {
    case RC_UNDEFINED:
      break;
    case RC_EDIT:
    case RC_NEW:
      return m_revision->title;
    case RC_LOG:
      return m_logEvent->title;
  }
  return EMPTY_STRING;
}

const Date& RecentChange::timestamp() const {
  switch (m_type) {
    case RC_UNDEFINED:
      break;
    case RC_EDIT:
    case RC_NEW:
      return m_revision->timestamp;
    case RC_LOG:
      return m_logEvent->timestamp;
  }
  return NULL_DATE;
}

const string& RecentChange::user() const {
  switch (m_type) {
    case RC_UNDEFINED:
      break;
    case RC_EDIT:
    case RC_NEW:
      return m_revision->user;
    case RC_LOG:
      return m_logEvent->user;
  }
  return EMPTY_STRING;
}

const string& RecentChange::comment() const {
  switch (m_type) {
    case RC_UNDEFINED:
      break;
    case RC_EDIT:
    case RC_NEW:
      return m_revision->comment;
    case RC_LOG:
      return m_logEvent->comment;
  }
  return EMPTY_STRING;
}

WriteToken WriteToken::newForCreation() {
  return WriteToken(CREATE);
}

WriteToken WriteToken::newForEdit(string_view title, const Date& timestamp, bool needsNoBotsBypass) {
  return WriteToken(EDIT, title, timestamp, needsNoBotsBypass);
}

WriteToken WriteToken::newWithoutConflictDetection() {
  return WriteToken(NO_CONFLICT_DETECTION);
}

WriteToken WriteToken::newFromString(string_view serializedWriteToken) {
  vector<string_view> fields = cbl::splitAsVector(serializedWriteToken, '|');
  if (!fields.empty()) {
    string_view typeStr = fields[0];
    if (typeStr == "UNINITIALIZED") {
      return WriteToken();
    } else if (typeStr == "CREATE") {
      return WriteToken(CREATE);
    } else if (typeStr == "EDIT" && fields.size() >= 3) {
      string_view dateStr = fields[1];
      string_view title = fields[2];
      bool needsNoBotsBypass = fields.size() >= 4 && fields[3] == "1";
      return WriteToken(EDIT, title, Date::fromISO8601OrEmpty(dateStr), needsNoBotsBypass);
    } else if (typeStr == "NO_CONFLICT_DETECTION") {
      return WriteToken(NO_CONFLICT_DETECTION);
    }
  }
  throw cbl::ParseError("Invalid serialized WriteToken '" + string(serializedWriteToken) + "'");
}

string WriteToken::toString() const {
  const char* typeStr = "";
  switch (m_type) {
    case UNINITIALIZED:
      typeStr = "UNINITIALIZED";
      break;
    case CREATE:
      typeStr = "CREATE";
      break;
    case EDIT:
      typeStr = "EDIT";
      break;
    case NO_CONFLICT_DETECTION:
      typeStr = "NO_CONFLICT_DETECTION";
      break;
  }
  string serializedWriteToken = typeStr;
  if (m_type == EDIT) {
    serializedWriteToken += '|';
    if (!m_timestamp.isNull()) {
      serializedWriteToken += m_timestamp.toISO8601();
    }
    serializedWriteToken += '|';
    serializedWriteToken += m_title;
    if (m_needsNoBotsBypass) {
      serializedWriteToken += "|1";
    }
  }
  return serializedWriteToken;
}

}  // namespace mwc
