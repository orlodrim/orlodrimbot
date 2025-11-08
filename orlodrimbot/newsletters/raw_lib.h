#ifndef RAW_LIB_H
#define RAW_LIB_H

#include <string>
#include "newsletter_distributor.h"

class RAWDistributor : public newsletters::Distributor {
public:
  using Distributor::Distributor;

protected:
  std::string getSubscribedToString() const override { return "abonné à Regards sur l'actualité de la Wikimedia"; }
  std::string getSubpagesPrefix() const override { return "Wikipédia:RAW/"; }
  std::string getSubscriptionPage() const override { return "Wikipédia:RAW/Inscription"; }
  bool compareIssues(const std::string& issue1, const std::string& issue2) override;
  Result canBeCurrentIssueTitle(const std::string& issueTitle) const override;
  Result isIssueReadyForPublication(const std::string& issueTitle, int& issueNumber) const override;
  std::string getIssueFromSection(const std::string& section) override;
  bool isStandardNewsletterSection(const std::string& message) override;
  void prepareMessage(const std::string& issueTitle, std::string& title, std::string& nowikiTitle, std::string& content,
                      std::string& editSummary) override;
  bool enableTwitterPublication() const override { return true; }
  void prepareTweet(const std::string& issueTitle, int issueNumber, std::string& text, std::string& image,
                    std::string& editSummary) override;
  void sendFailureNotification(const std::string& issueTitle, const std::string& displayableError) const override;
};

#endif
