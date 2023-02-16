#ifndef MWC_WIKI_H
#define MWC_WIKI_H

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include "cbl/date.h"
#include "site_info.h"
#include "titles_util.h"
#include "wiki_base.h"
#include "wiki_defs.h"

namespace cbl {
class HTTPClient;
}  // namespace cbl

namespace mwc {

using PagesStringProperties = std::unordered_map<std::string, std::vector<std::string>>;

struct LoginParams {
  // Location of api.php and index.php. Example: "https://en.wikipedia.org/w".
  std::string url;
  // User name, usually an account with a '@' created with Special:BotPasswords.
  // May be empty to use the wiki without being logged in.
  std::string userName;
  // User password, usually the one generated by Special:BotPasswords.
  // Can be omitted if a session file already exists, but in that case, the bot will not be able to recover if the
  // session is lost.
  std::string password;
  // Whether to use the API for UI login. This allows direct login for accounts using two-factor authentication.
  // When enabled, the login function may become interactive (it asks for a one-time token on the command line).
  bool clientLogin = false;

  // On Wikimedia wikis, should follow the guidelines from https://meta.wikimedia.org/wiki/User-Agent_policy.
  std::string userAgent;
  // Number of seconds to wait before every HTTP request. This can be 0 on Wikimedia wikis, as long as requests are done
  // sequentially (see https://www.mediawiki.org/wiki/API:Etiquette).
  int delayBeforeRequests = 0;
  // Number of seconds to wait between edits. Since this has a user-visible impact, each wiki may have different rules.
  // For instance, https://en.wikipedia.org/wiki/Wikipedia:Bot_policy suggests 10 seconds between edits for non-urgent
  // tasks.
  int delayBetweenEdits = 12;
  // If the replication lag is higher than `maxLag` seconds, MediaWiki is allowed to reject the request.
  // https://www.mediawiki.org/wiki/Manual:Maxlag_parameter suggests to use 5 seconds for Wikimedia wikis.
  int maxLag = 5;
  // Whether to read namespaces and the interwiki map from the wiki. This is required for the correctness of string
  // operations on titles. Otherwise, uses a stub SiteInfo with only the main namespace in CM_CASE_SENSITIVE mode.
  bool readSiteInfo = true;
};

struct HistoryParams {
  std::string title;
  int prop = 0;  // From RevProp, must be set to a non-zero value.
  EventsDir direction = NEWEST_FIRST;
  cbl::Date start;
  cbl::Date end;
  // startId and endId work with getHistory but not getDeletedHistory.
  revid_t startId = 0;
  revid_t endId = 0;
  // Maximum number of revisions to return. Use PAGER_ALL to get all revisions matching the other requirements.
  int limit = 50;
  // May be initialized with the value of nextQueryContinue obtained in the previous call to continue the enumeration.
  std::string queryContinue;

  // If non-null and there are more than `limit` results, this is set to a string that can be passed in `queryContinue`
  // in the next call.
  std::string* nextQueryContinue = nullptr;
};

struct RecentChangesParams {
  int prop = 0;  // from RevProp
  int type = 0;  // from RecentChangeType
  int show = 0;  // from RecentChangesShow
  std::string user;
  std::string title;
  std::string tag;
  NamespaceList namespaceList;
  EventsDir direction = NEWEST_FIRST;
  cbl::Date start;
  cbl::Date end;
  int limit = PAGER_ALL;
  // May be initialized with the value of nextQueryContinue obtained in the previous call to continue the enumeration.
  std::string queryContinue;

  // If non-null and there are more than `limit` results, this is set to a string that can be passed in `queryContinue`
  // in the next call.
  std::string* nextQueryContinue = nullptr;
};

struct LogEventsParams {
  int prop = 0;                      // from RevProp (combination of values).
  LogEventType type = LE_UNDEFINED;  // from LogEventType. LE_UNDEFINED means all.
  std::string user;
  std::string title;
  EventsDir direction = NEWEST_FIRST;
  cbl::Date start;
  cbl::Date end;
  int limit = PAGER_ALL;
};

enum CategoryMembersSort {
  CMS_SORTKEY,
  CMS_TIMESTAMP,
};

enum CategoryMembersProp {
  CMP_SORTKEY_PREFIX = 1,
  CMP_TIMESTAMP = 2,
};

struct CategoryMember {
  std::string title;
  std::string sortkeyPrefix;
  cbl::Date timestamp;
};

struct CategoryMembersParams {
  std::string title;  // Title including the namespace, e.g. "Category:Physics".
  int prop = 0;       // From CategoryMembersProp.
  CategoryMembersSort sort = CMS_SORTKEY;
  EventsDir direction = NEWEST_FIRST;
  cbl::Date start;
  cbl::Date end;
  int limit = PAGER_ALL;

  // Output parameters.
  // At least one of these two pointers should be non-null (usually members if prop != 0 and titlesOfMembers if
  // prop == 0).
  std::vector<CategoryMember>* members = nullptr;
  std::vector<std::string>* titlesOfMembers = nullptr;
  // The size of the category according to MediaWiki own counter. This is what is returned by getCategoriesCount and it
  // may occasionally get out of sync with the real size. There is no reason to request it except for checking if
  // this counter is actually out of sync.
  int* sizeEstimate = nullptr;
};

struct BacklinksParams {
  std::string title;
  FilterRedirMode filterRedir = FR_ALL;
  NamespaceList namespaceList;
};

struct TransclusionsParams {
  std::string title;  // Title including the namespace, e.g. "Template:Infobox".
  NamespaceList namespaceList;
};

struct AllPagesParams {
  std::string prefix;
  FilterRedirMode filterRedir = FR_ALL;
  int protectType = 0;
  int protectLevel = PRL_NONE;
  int namespace_ = NS_MAIN;
  int limit = PAGER_ALL;
};

struct UserContribsParams {
  // Exactly one of user or userPrefix should be set.
  std::string user;
  std::string userPrefix;
  int prop = 0;  // from RevProp
  int show = 0;
  std::string tag;
  NamespaceList namespaceList;
  EventsDir direction = NEWEST_FIRST;
  cbl::Date start;
  cbl::Date end;
  int limit = PAGER_ALL;
  // May be initialized with the value of nextQueryContinue obtained in the previous call to continue the enumeration.
  std::string queryContinue;

  // If non-null and there are more than `limit` results, this is set to a string that can be passed in `queryContinue`
  // in the next call.
  std::string* nextQueryContinue = nullptr;
};

struct RenderParams {
  std::string text;
  // When parsing magic words such as {{PAGENAME}}, assume that the page has this title.
  std::string title;
  bool disableEditSection = false;
};

using EmergencyStopTest = std::function<bool()>;

// Client class for MediaWiki API.
class Wiki : public WikiBase {
public:
  Wiki();
  ~Wiki() override;

  // == Session ==
  // Logs in. Should be called before any other function, except those from the HTTP section.
  // Example: logIn("https://en.wikipedia.org/w", "MyBot@MyAccountFromSpecialBotPasswords", "secret", "mybot.session")
  // See LoginParams for detailed explanation about url, userName and password.
  // The session file stores session cookies and site information (e.g. namespaces). If sessionFile is empty, the
  // session is not saved to disk. If sessionFile is set but does not exist yet, the function logs in and save the
  // session to that file. If sessionFile exists, the function restores the session from that file without sending any
  // request.
  void logIn(const std::string& url, const std::string& userName, const std::string& password,
             const std::string& sessionFile = std::string());
  // Version taking parameters in LoginParams.
  virtual void logIn(const LoginParams& loginParams, const std::string& sessionFile = std::string());

  // Returns the user name, split before the first '@' if it contains one.
  // For bot accounts created via Special:BotPasswords, this returns the name of the main user.
  const std::string& externalUserName() const { return m_externalUserName; }
  // Returns the user name passed to log in.
  // For bot accounts created via Special:BotPasswords, this may be of the form 'MainUserName@BotAccountName'.
  const std::string& internalUserName() const override { return m_internalUserName; }

  // Retry to log in using the same parameters as the last call to log in.
  // Can be called if a loss of session is detected.
  bool retryToLogIn() override;

  // == Read api ==
  const SiteInfo& siteInfo() const { return m_siteInfo; }

  // Reads the specified properties of the current revision of page `title`, where `properties` is a combination of
  // values from RevProp.
  // All other variants of readPage call this one, so this is the only one that needs to be overridden in tests.
  virtual Revision readPage(const std::string& title, int properties);

  // Variant of the previous function that also initializes writeToken (if non-null) so that it can be used with
  // writePage. If a WikiError is thrown, writeToken is left unchanged.
  virtual Revision readPage(const std::string& title, int properties, WriteToken* writeToken);

  // Reads the content of the current revision of page `title`.
  // If writeToken is non-null, initializes writeToken so that it can be used with writePage
  virtual std::string readPageContent(const std::string& title, WriteToken* writeToken = nullptr);

  // Variant of the previous function that does not throw an exception if the page does not exist. Instead, it returns
  // an empty string and sets writeToken to a token that allows page creation.
  virtual std::string readPageContentIfExists(const std::string& title, WriteToken* writeToken = nullptr);

  // Reads a arbitrary revision identified by its revision id (oldid).
  virtual Revision readRevision(revid_t revid, int properties);

  // Reads the content of an arbitrary revision identified by its revision id.
  virtual std::string readRevisionContent(revid_t revid);

  // Reads information about the current revision of multiple pages.
  // Input:
  //   properties: Which properties to retrieve. Combination of values from RevProp.
  //   revisions: Prefilled vector where the title of each Revision is set to the title of a page to read.
  //   flags: Combination of values from ReadPageFlags.
  // Output: Modifies members of `revisions`.
  //   title field: If RP_TITLE is requested, it is replaced by the normalized title (this happens even if the page does
  //     not exist, provided that the title is valid and is not an interwiki). Otherwise it is left unchanged.
  //   revid: If the title is invalid or is an interwiki, it is set to -2. If the page does not exist, it is set to -1.
  //     If the page exists and RP_REVID is requested, it is set to the latest page revision. If the page exists and
  //     RP_REVID is not requested, it is set to 0.
  //   other fields of revisions members: those requested in properties are filled and the content of other fields is
  //     undefined.
  // NOTE: This requests up to 500 pages at a time (for bots). If RP_CONTENT is specified in properties, the output of a
  // single iteration may take hundreds of MB and MediaWiki may fail to respond. Prefer using BulkPageReader in that
  // case.
  virtual void readPages(int properties, std::vector<Revision>& revisions, int readPageFlags = 0);

  // Reads information about multiple revisions.
  // Input: The "revid" property of each Revision gives the revision to read.
  // Output: The fields requested in properties are filled and the content of other fields is undefined. If the revid
  //   of a Revision is not found, its title is set to INVALID_TITLE.
  // NOTE: As with readPages, RP_CONTENT may generate a very large output.
  virtual void readRevisions(int properties, std::vector<Revision>& revisions);

  // Returns true if a page exists, false otherwise.
  // Throws InvalidParameterError if `title` is an invalid title, a special page or an interwiki.
  virtual bool pageExists(const std::string& title);

  virtual std::vector<std::string> getPageLinks(const std::string& title);
  virtual PagesStringProperties getPagesLinks(const std::vector<std::string>& titles);
  virtual std::vector<std::string> getPageCategories(const std::string& title);
  virtual PagesStringProperties getPagesCategories(const std::vector<std::string>& titles);
  virtual std::map<std::string, std::vector<std::pair<std::string, cbl::Date>>> getPagesCategoriesWithDate(
      const std::vector<std::string>& titles);
  virtual std::vector<std::string> getPageTemplates(const std::string& title);
  virtual PagesStringProperties getPagesTemplates(const std::vector<std::string>& titles);
  virtual std::vector<std::string> getPageImages(const std::string& title);
  virtual PagesStringProperties getPagesImages(const std::vector<std::string>& titles);
  virtual std::vector<std::string> getPageLangLinks(const std::string& title);
  virtual PagesStringProperties getPagesLangLinks(const std::vector<std::string>& titles, const std::string& lang = "");
  virtual std::unordered_map<std::string, bool> getPagesDisambigStatus(const std::vector<std::string>& titles);
  virtual std::unordered_map<std::string, std::string> getPagesWikibaseItems(const std::vector<std::string>& titles);
  virtual std::vector<PageProtection> getPageProtections(const std::string& title);
  virtual std::unordered_map<std::string, std::vector<PageProtection>> getPagesProtections(
      const std::vector<std::string>& titles);
  virtual ImageSize getImageSize(const std::string& title);
  virtual std::unordered_map<std::string, ImageSize> getImagesSize(const std::vector<std::string>& titles);

  virtual std::vector<Revision> getHistory(const HistoryParams& params);
  virtual std::vector<Revision> getDeletedHistory(const HistoryParams& params);

  virtual std::vector<RecentChange> getRecentChanges(const RecentChangesParams& params);

  virtual std::vector<LogEvent> getLogEvents(const LogEventsParams& params);

  virtual void getCategoryMembers(const CategoryMembersParams& params);
  virtual std::vector<std::string> getCategoryMembers(const std::string& category);
  virtual std::unordered_map<std::string, int> getCategoriesCount(const std::vector<std::string>& titles);

  virtual std::vector<std::string> getBacklinks(const BacklinksParams& params);
  virtual std::vector<std::string> getBacklinks(const std::string& title);

  // Reads the list of redirects that contain a link to title.
  // NOTE: Some of these pages might not redirect to title (some redirects have hidden content with links inside).
  virtual std::vector<std::string> getRedirects(const std::string& title);

  virtual std::vector<std::string> getTransclusions(const TransclusionsParams& params);
  virtual std::vector<std::string> getTransclusions(const std::string& title);

  virtual std::vector<std::string> getAllPages(const AllPagesParams& params);
  virtual std::vector<std::string> getPagesByPrefix(const std::string& prefix);

  virtual void getUsersInfo(int properties, std::vector<UserInfo>& users);
  virtual std::vector<Revision> getUserContribs(const UserContribsParams& params);
  virtual std::vector<std::string> getUsersInGroup(UserGroup userGroup);

  virtual std::string expandTemplates(const std::string& code, const std::string& title = "API");
  virtual std::string renderAsHTML(const RenderParams& params);

  virtual std::vector<std::string> searchText(const std::string& query, int maxResults = 10);
  virtual std::vector<std::string> getExtURLUsage(const std::string& url, int maxResults = 10);

  // == Write api ==
  // Retrieves a MediaWiki token to perform an action. All the following functions obtain such a token automatically
  // when needed, so you should not need to call this unless you use the low-level interface (apiRequest).
  std::string getToken(TokenType tokenType) override;
  void clearTokenCache() override;

  // Creates or replaces the content of a page.
  // `writeToken` should normally be obtained by calling readPage/readPageContent. This ensures that edit conflicts are
  // detected.
  // `flags` is a combination of values from EditPageFlags. By default:
  // - The edit is marked with the bot flag, but not as minor.
  // - The edit is rejected if `content` is empty.
  // - The edit is rejected if the page contained {{nobots}} before the change (this information is stored in
  //   writeToken).
  virtual void writePage(const std::string& title, const std::string& content, const WriteToken& writeToken,
                         const std::string& summary = std::string(), int flags = 0);

  // Appends some text to a page. No new line is automatically added between the previous content and the new text.
  // In most cases, readPage(Content)/writePage should be used instead of this function. Unlike writePage, appendToPage
  // is not idempotent and thus is not retried in case of transient errors. Also, appendToPage does not take a
  // WriteToken, so it ignores {{nobots}}.
  virtual void appendToPage(const std::string& title, const std::string& content,
                            const std::string& summary = std::string(), int flags = 0);

  // Helper function that calls readPageContent and then writePage.
  // Can create non-existing pages. In that case, transformContent is called with an empty string.
  // transformContent may be called multiple times in case of an edit conflict.
  void editPage(const std::string& title,
                const std::function<void(std::string& content, std::string& summary)>& transformContent, int flags = 0);

  // Renames a page and possibly its talk page.
  // `flags` is a combination of values from MovePageFlags.
  virtual void movePage(const std::string& oldTitle, const std::string& newTitle,
                        const std::string& summary = std::string(), int flags = 0);

  virtual void setPageProtection(const std::string& title, const std::vector<PageProtection>& protections,
                                 const std::string& reason = std::string());

  virtual void deletePage(const std::string& title, const std::string& reason = std::string());

  virtual void purgePage(const std::string& title);

  virtual void emailUser(const std::string& user, const std::string& subject, const std::string& text,
                         bool ccme = false);

  virtual void flowNewTopic(const std::string& title, const std::string& topic, const std::string& content,
                            int flags = 0);

  // Sets a hook function called at the beginning of writePage and all other mutating functions, normally for the
  // purpose of checking if some emergency stop mechanism was activated.
  // If the function returns true, an EmergencyStopError is thrown and the operation is not executed. The hook is also
  // allowed to throw arbitrary exceptions. They are propagated to the caller of the mutating function (after adding
  // some context for a WikiError).
  void setEmergencyStopTest(const EmergencyStopTest& test);
  // Activates a default emergency stop test that returns true when the talk page of the logged-in user is modified.
  // Edits done more than a few seconds before the activation of the test are ignored.
  void enableDefaultEmergencyStopTest();
  // Deactivates any active emergency stop test.
  void clearEmergencyStopTest();
  // If an emergency stop test is active, calls it and returns its result. Otherwise, returns false.
  bool isEmergencyStopTriggered() override;

  // == HTTP ==
  cbl::HTTPClient& httpClient() override;
  // Cannot be called after logIn().
  void setHTTPClient(std::unique_ptr<cbl::HTTPClient> httpClient);
  // Sets the number of seconds to wait before each HTTP request. If this function is called before logIn, logIn will
  // ignore the value defined in LoginParams.
  void setDelayBeforeRequests(int delay);
  // Sets the number of seconds to wait between edits, or other mutating requests. If this function is called before
  // logIn, logIn will ignore the value defined in LoginParams.
  // "between" means that the waiting time depends on the time at which the previous edit was done. For instance:
  // wiki.setDelayBeforeRequests(5); wiki.writePage(...); sleep(1); wiki.writePage(...);
  // => In the second call, writePage will sleep only 5 - 1 = 4 additional seconds before writing the page.
  void setDelayBetweenEdits(int delay);

  // == Client-side parsing of titles and redirects ==
  // Those functions require that logIn is called with readSiteInfo=true in LoginParams.

  // Normalizes a title and splits the namespace part from the rest.
  // This relies on a client-side implementation which not fully accurate. In particular, this function does not have
  // access to the gender of users, so it always uses the default name of the User namespace for normalization
  // (e.g. "Utilisateur" and not "Utilisatrice" in French). If you need the same normalization as MediaWiki, use
  // readPage(s) with prop = RP_TITLE.
  TitleParts parseTitle(std::string_view title, NamespaceNumber defaultNamespaceNumber = NS_MAIN,
                        int parseTitleFlags = PTF_DEFAULT) const;
  std::string normalizeTitle(std::string_view title, NamespaceNumber defaultNamespaceNumber = NS_MAIN) const;
  // If `title` is in namespace `expectedNamespace`, returns `title` without the namespace prefix (and without anchor).
  // Otherwise, returns an empty string.
  std::string stripNamespace(std::string_view title, NamespaceNumber expectedNamespace) const;
  int getTitleNamespace(std::string_view title) const;
  // If title has an associated talk page, returns this talk page. If title is already a talk page, returns it (possibly
  // normalized). Otherwise, returns an empty string (e.g. for interwikis or namespaces without an associated talk
  // namespace).
  std::string getTalkPage(const std::string& title) const;
  // If title is a talk page, returns its associated non-talk page. Otherwise, returns title (possibly normalized).
  std::string getSubjectPage(const std::string& title) const;
  // Builds a wikilink to `target`.
  // Usually, this just adds double square brackets (e.g. "A" => "[[A]]"), but there are some cases where a colon is
  // added before the title (e.g. for categories and files).
  std::string makeLink(const std::string& target) const;
  // Returns true if `code` is a redirect. Sets target and/or anchor to the normalized target and anchor of the
  // redirect, if they are not null.
  bool readRedirect(std::string_view code, std::string* target, std::string* anchor) const;

protected:
  void setInternalUserName(std::string_view userName);

  SiteInfo m_siteInfo;

private:
  // == Session ==
  void clearSession();
  // Saves wiki address, username and session cookie to a buffer, so that the session can be restored later without
  // password.
  std::string sessionToString() const;
  // Saves wiki address, username and session cookie to a file, so that the session can be restored later without
  // password.
  void sessionToFile(const std::string& fileName) const;
  // Restores a session saved with sessionToBuffer.
  // Throws: cbl::ParseError.
  void sessionFromString(const std::string& buffer);
  // Restores a session saved with sessionToFile.
  // Throws: cbl::FileNotFoundError, cbl::SystemError, cbl::ParseError.
  void sessionFromFile(const std::string& fileName);
  void loadSiteInfo();
  // Sets the cookies of m_httpClient. Does not change the value of m_wikiURL, m_internalUserName, or any other internal
  // state. This temporarily changes m_internalUserName (which is why userName is passed by value).
  // Throws: WikiError.
  void loginInternal(std::string userName, const std::string& password, bool clientLogin);

  // == Write API ==
  std::string getTokenUncached(TokenType tokenType);

  std::unique_ptr<cbl::HTTPClient> m_httpClient;
  std::string m_internalUserName;
  std::string m_externalUserName;
  std::string m_password;
  std::string m_sessionFile;
  bool m_delayBeforeRequestsOverridden = false;
  bool m_delayBetweenEditsOverridden = false;

  std::string m_tokenCache[TOK_MAX];
  EmergencyStopTest m_emergencyStopTest;
};

}  // namespace mwc

#endif
