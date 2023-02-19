#ifndef MWC_WIKI_DEFS_H
#define MWC_WIKI_DEFS_H

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "cbl/date.h"
#include "cbl/error.h"

namespace mwc {

class WikiError : public cbl::Error {
public:
  using Error::Error;
  const char* what() const noexcept override;
  void addContext(const std::string& context);

private:
  std::string m_messageWithContext;
};

class LowLevelError : public WikiError {
public:
  enum Type {
    UNSPECIFIED,
    NETWORK,
    HTTP,
    JSON_PARSING,
    READ_ONLY_WIKI,
  };
  LowLevelError(Type type, const std::string& message) : WikiError(message), m_type(type) {}
  Type type() const { return m_type; }

private:
  Type m_type;
};

class APIError : public WikiError {
public:
  APIError(const std::string& code, const std::string& message) : WikiError(message), m_code(code) {}
  const std::string& code() const { return m_code; }
  // Special code for errors reported through another mechanism than an "error" member in the response.
  static constexpr char CODELESS_ERROR[] = "codeless-error";

private:
  std::string m_code;
};

class UnexpectedAPIResponseError : public WikiError {
public:
  using WikiError::WikiError;
};

class InvalidParameterError : public WikiError {
public:
  using WikiError::WikiError;
};

class InvalidStateError : public WikiError {
public:
  using WikiError::WikiError;
};

class PageAlreadyExistsError : public WikiError {
public:
  using WikiError::WikiError;
};

class PageNotFoundError : public WikiError {
public:
  using WikiError::WikiError;
};

class ProtectedPageError : public WikiError {
public:
  using WikiError::WikiError;
};

class EmergencyStopError : public WikiError {
public:
  using WikiError::WikiError;
};

class EditConflictError : public WikiError {
public:
  using WikiError::WikiError;
};

class BotExclusionError : public WikiError {
public:
  using WikiError::WikiError;
};

using revid_t = int64_t;

constexpr revid_t INVALID_REVID = 0;

revid_t revidOfString(const std::string& s);

class NamespaceList {
public:
  NamespaceList() {}
  explicit NamespaceList(int namespace_) : m_namespaces(1, namespace_) {}
  explicit NamespaceList(const std::vector<int>& namespaces) : m_namespaces(namespaces) {}
  bool empty() const { return m_namespaces.empty(); }
  int size() const { return m_namespaces.size(); }
  std::string toString() const;

private:
  std::vector<int> m_namespaces;
};

// For functions that take a limit on the number of results, PAGER_ALL means "all results".
constexpr int PAGER_ALL = -1;

enum EventsDir {
  NEWEST_FIRST,
  OLDEST_FIRST,
};

enum RevProp {
  RP_TITLE = 1,
  RP_REVID = 2,
  RP_MINOR = 4,
  RP_BOT = 8,  // Recent changes only
  RP_TIMESTAMP = 0x10,
  RP_USER = 0x20,
  RP_USERID = 0x40,
  RP_SIZE = 0x80,
  RP_COMMENT = 0x100,
  RP_PARSEDCOMMENT = 0x200,
  RP_CONTENT = 0x400,
  RP_TAGS = 0x800,
  RP_REDIRECT = 0x1000,   // Recent changes only
  RP_PATROLLED = 0x2000,  // Recent changes only
  RP_NEW = 0x4000,        // Recent changes only
  RP_SHA1 = 0x8000,
  RP_CONTENT_MODEL = 0x10000,
};

enum RevContentModel {
  RCM_INVALID,
  RCM_WIKITEXT,
  RCM_FLOW_BOARD,
};

constexpr const char* INVALID_TITLE = "#";

struct Revision {
  std::string title;
  revid_t revid = INVALID_REVID;
  cbl::Date timestamp;
  std::string user;
  int64_t userid = 0;
  long size = 0;
  std::string comment;
  std::string parsedComment;
  std::string content;
  std::string sha1;
  std::vector<std::string> tags;
  RevContentModel contentModel = RCM_INVALID;
  bool minor_ = false;
  bool bot = false;        // Recent changes only.
  bool redirect = false;   // Recent changes only.
  bool patrolled = false;  // Recent changes only.
  bool new_ = false;       // Recent changes only.
  bool contentHidden = false;
};

enum LogEventType {
  LE_UNDEFINED,
  LE_BLOCK,
  LE_PROTECT,
  LE_RIGHTS,
  LE_DELETE,
  LE_UPLOAD,
  LE_MOVE,
  LE_IMPORT,
  LE_PATROL,
  LE_MERGE,
  LE_SUPPRESS,
  LE_ABUSEFILTER,
  LE_NEWUSERS,
  LE_CREATE,
};

class LogEvent {
public:
  LogEventType type() const { return m_type; }
  void setType(LogEventType newType) { m_type = newType; }

  int64_t logid = 0;
  std::string action;
  bool bot = false;
  cbl::Date timestamp;
  std::string title;
  std::string user;
  int64_t userid;
  std::string comment;
  std::string parsedComment;

  struct MoveParams {
    std::string newTitle;
    bool suppressRedirect = false;
  };
  // Returns an empty MoveParams if type() != LE_MOVE.
  const MoveParams& moveParams() const { return m_moveParams; }
  // Requires type() == LE_MOVE.
  MoveParams& mutableMoveParams() { return m_moveParams; }

private:
  LogEventType m_type = LE_UNDEFINED;
  MoveParams m_moveParams;
};

enum RecentChangeType {
  RC_UNDEFINED = 0,
  RC_EDIT = 1,
  RC_NEW = 2,
  RC_LOG = 4,
};

enum RecentChangesShow {
  RCS_MINOR = 1,
  RCS_NOT_MINOR = 2,
  RCS_BOT = 4,
  RCS_NOT_BOT = 8,
  RCS_ANON = 0x10,
  RCS_NOT_ANON = 0x20,
  RCS_REDIRECT = 0x40,
  RCS_NOT_REDIRECT = 0x80,
  RCS_PATROLLED = 0x100,
  RCS_NOT_PATROLLED = 0x200,
};

class RecentChange {
public:
  RecentChangeType type() const { return m_type; }
  void setType(RecentChangeType newType);

  const Revision& revision() const;
  Revision& mutableRevision();
  const LogEvent& logEvent() const;
  LogEvent& mutableLogEvent();
  const std::string& title() const;
  const cbl::Date& timestamp() const;
  const std::string& user() const;
  const std::string& comment() const;

  int64_t rcid = 0;
  revid_t oldRevid = 0;
  long oldSize = 0;

  RecentChange copy() const;

private:
  RecentChangeType m_type = RC_UNDEFINED;
  std::unique_ptr<Revision> m_revision;
  std::unique_ptr<LogEvent> m_logEvent;
};

enum UserInfoProp {
  UIP_NAME = 1,
  UIP_EDIT_COUNT = 2,
  UIP_GROUPS = 4,
};

enum UserGroup {
  UG_AUTOCONFIRMED = 1,
  UG_AUTOPATROLLED = 2,
  UG_SYSOP = 4,
  UG_BOT = 8,
  UG_BUREAUCRAT = 0x10,
  UG_CHECKUSER = 0x20,
  UG_OVERSIGHT = 0x40,
  UG_INTERFACE_ADMIN = 0x80,
};

class UserInfo {
public:
  std::string name;
  int editCount = 0;
  int groups = 0;
};

enum ReadPageFlags {
  READ_RESOLVE_REDIRECTS = 1,
};

enum EditPageFlags {
  EDIT_MINOR = 1,
  EDIT_OMIT_BOT_FLAG = 2,
  EDIT_APPEND = 4,
  EDIT_ALLOW_BLANKING = 8,
  EDIT_BYPASS_NOBOTS = 0x10,
};

enum MovePageFlags {
  MOVE_MOVETALK = 1,
  MOVE_NOREDIRECT = 2,
};

enum PageProtectionType {
  PRT_EDIT = 1,
  PRT_MOVE = 2,
  PRT_UPLOAD = 4,
  PRT_CREATE = 8,
};

enum PageProtectionLevel {
  PRL_NONE = 0,
  PRL_AUTOCONFIRMED = 1,
  PRL_SYSOP = 2,
  PRL_AUTOPATROLLED = 4,
};

class PageProtection {
public:
  PageProtection() {}
  PageProtection(PageProtectionType t, PageProtectionLevel l, cbl::Date e = cbl::Date())
      : type(t), level(l), expiry(e) {}

  PageProtectionType type;
  PageProtectionLevel level;
  cbl::Date expiry;
};

enum FilterRedirMode { FR_ALL, FR_REDIRECTS, FR_NONREDIRECTS };

struct ImageSize {
  int width = 0;
  int height = 0;
};

// Token to pass to Wiki::writePage for edit conflict and {{nobots}} automatic detection.
// Can be obtained from one of the Wiki::readPage functions.
// There is no relation between WriteToken (a client-side concept of this library) and CSRF tokens (required by the
// MediaWiki API to write pages and managed internally by this library).
class WriteToken {
public:
  enum Type {
    UNINITIALIZED = 0,
    CREATE = 1,
    EDIT = 2,
    NO_CONFLICT_DETECTION = 3,
  };

  // Only allows page creation. Fails if the page already exists.
  static WriteToken newForCreation();
  // Allow the edit if there if the diff between the revision at time `timestamp` and the current one does not cause an
  // edit conflict.
  // If needsNoBotsBypass is true, the edit will be rejected unless writePage is called with EDIT_BYPASS_NOBOTS.
  static WriteToken newForEdit(std::string_view title, revid_t revid, bool needsNoBotsBypass);
  // Bypass all checks. This is a bad idea. The name is intentionally long to make it annoying to use.
  static WriteToken newWithoutConflictDetection();
  // Initializes from a string obtained with toString().
  static WriteToken newFromString(std::string_view serializedWriteToken);

  WriteToken() = default;
  Type type() const { return m_type; }
  // Serializes to a string that can be parsed with newFromString(). The format of the string is unspecified.
  std::string toString() const;

  // For internal use by the Wiki class (the exact list of what is stored might change over time).
  const std::string& title() const { return m_title; }
  revid_t revid() const { return m_revid; }
  bool needsNoBotsBypass() const { return m_needsNoBotsBypass; }

private:
  explicit WriteToken(Type type) : m_type(type) {}
  WriteToken(Type type, std::string_view title, revid_t revid, bool needsNoBotsBypass)
      : m_type(type), m_title(title), m_revid(revid), m_needsNoBotsBypass(needsNoBotsBypass) {}

  Type m_type = UNINITIALIZED;
  std::string m_title;
  revid_t m_revid = INVALID_REVID;
  bool m_needsNoBotsBypass = false;
};

}  // namespace mwc

#endif
