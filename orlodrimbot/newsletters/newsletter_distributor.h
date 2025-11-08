#ifndef NEWSLETTER_DISTRIBUTOR_H
#define NEWSLETTER_DISTRIBUTOR_H

#include <optional>
#include <string>
#include <vector>
#include "cbl/error.h"
#include "cbl/json.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/live_replication/recent_changes_reader.h"

namespace newsletters {

struct Subscriber {
  std::string page;
  bool deleteOldMessages = true;
};

class DistributorError : public cbl::Error {
public:
  DistributorError(const std::string& issueTitle, const std::string& internalError, const std::string& displayableError)
      : Error(internalError), m_issueTitle(issueTitle), m_displayableError(displayableError) {}

  const std::string& issueTitle() const { return m_issueTitle; }
  const std::string& displayableError() const { return m_displayableError; }

private:
  std::string m_issueTitle;
  std::string m_displayableError;
};

class UpdatePageError : public cbl::Error {
public:
  using Error::Error;
};

// Throws: mwc::WikiError.
std::vector<Subscriber> getSubscribers(mwc::Wiki& wiki, const std::string& subscriptionPage);

std::optional<std::string> getUserFromPage(mwc::Wiki& wiki, const std::string& title);

std::optional<mwc::Revision> getLastContribution(mwc::Wiki& wiki, const std::string& user);

class Distributor {
public:
  struct Result {
    Result() = default;
    Result(const std::string& issueTitle, const std::string& internalError, const std::string& displayableError);

    bool ok = true;
    std::string issueTitle;
    std::string internalError;
    std::string displayableError;
  };

  Distributor(mwc::Wiki* wiki, const std::string& stateFile,
              live_replication::RecentChangesReader* recentChangesReader);
  virtual ~Distributor();
  bool run(const std::string& forcedIssue, const std::string& fromPage, const std::string& singlePage, bool force,
           bool dryRun);
  bool wasDistributed(const std::string& issue);

protected:
  virtual std::string getSubscribedToString() const = 0;
  virtual std::string getSubpagesPrefix() const = 0;
  virtual std::string getSubscriptionPage() const = 0;
  virtual bool compareIssues(const std::string& issue1, const std::string& issue2) = 0;
  virtual Result canBeCurrentIssueTitle(const std::string& issueTitle) const = 0;
  virtual Result isIssueReadyForPublication(const std::string& issueTitle, int& issueNumber) const = 0;
  virtual std::string getIssueFromSection(const std::string& section) = 0;
  virtual bool isStandardNewsletterSection(const std::string& message) = 0;
  virtual void prepareMessage(const std::string& issueTitle, std::string& title, std::string& nowikiTitle,
                              std::string& content, std::string& editSummary) = 0;
  virtual bool enableTwitterPublication() const { return false; }
  virtual void prepareTweet(const std::string& issueTitle, int issueNumber, std::string& text, std::string& image,
                            std::string& editSummary) {}
  virtual void sendFailureNotification(const std::string& issueTitle, const std::string& displayableError) const {}

  mwc::Wiki* m_wiki = nullptr;

private:
  void loadState();
  void saveState(bool dryRun);
  bool isValidTargetPage(const std::string& targetPage, const std::string& originalPage);
  // Throws: mwc::WikiError.
  Result isUserAllowedToPublish(const std::string& user);
  std::string getNewIssue(bool dryRun);
  // Throws: DistributorError.
  void runInternal(const std::string& forcedIssue, const std::string& fromPage, const std::string& singlePage,
                   bool force, bool dryRun);
  // Throws: UpdatePageError, mwc::WikiError.
  void postMessage(const std::string& issue, const std::string& targetPage, bool deleteOld, bool dryRun);
  // Throws: UpdatePageError, mwc::WikiError.
  void addTweetProposal(const std::string& issue, int issueNumber, bool dryRun);

  std::string m_stateFile;
  json::Value m_state;
  live_replication::RecentChangesReader* m_recentChangesReader = nullptr;
};

}  // namespace newsletters

#endif
