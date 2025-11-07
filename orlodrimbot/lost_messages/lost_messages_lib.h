#ifndef LOST_MESSAGES_LIB_H
#define LOST_MESSAGES_LIB_H

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "cbl/date.h"
#include "cbl/json.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/live_replication/recent_changes_reader.h"
#include "message_classifier.h"

enum class PostCategory {
  QUESTION,
  NON_WIKI_QUESTION,
  THANKS,
  DRAFT,
  OTHER,
};

enum class SectionType {
  WELCOME_MESSAGE,
  SALEBOT_REVERT_MESSAGE,
  SALEBOT_DELETION_MESSAGE,
  SALEBOT_POST_DELETION_MESSAGE,
  NAGGOBOT_UNDELETE_REQUEST_MESSAGE,
  ORLODRIMBOT_CONVERTED_TO_DRAFT,
  OTHER,
};

struct RevisionToCheck {
  std::string page;
  std::string user;
  mwc::revid_t revid = mwc::INVALID_REVID;
};

struct Post {
  std::string page;
  std::string user;
  cbl::Date timestamp;
  mwc::revid_t revid = mwc::INVALID_REVID;
  mwc::revid_t previousRevid = mwc::INVALID_REVID;
  mwc::revid_t welcomeRevid = mwc::INVALID_REVID;
  int numEdits = 0;
  bool hasNonWelcomeBotMessage = false;
};

struct PostAnalysis {
  std::string pageContent;
  std::string_view diff;  // View of pageContent if not empty.
  std::string mentor;     // Always non-empty if sectionType is WELCOME_MESSAGE. Always empty if onDraftTalk is true.
  bool onDraftTalk = false;
  SectionType sectionType = SectionType::OTHER;
  int messageIndentation = 0;
  int answerStart = -1;
  int answerEnd = -1;
  MessageClassification classification;
};

struct MentorState {
  bool anythingForwarded = false;
  bool thanksForwarded = false;
};

class LostMessages {
public:
  explicit LostMessages(mwc::Wiki* wiki, const std::string& mentorStatesFile,
                        std::unique_ptr<MessageClassifier> messageClassifier = nullptr);
  void runOnRecentChanges(live_replication::RecentChangesReader& recentChangesReader, json::Value& state,
                          bool dryRun = false);
  void runForUser(const std::string& user, bool onDraftPage = false, bool dryRun = false);

  // Returns the first post by `user` if only bots and the user edited the page so far.
  // `ignoreLaterChanges` changes the behavior to ignore all changes after the first one by `user`. This can be used to
  // extract historical first posts to create benchmarks.
  std::optional<Post> extractPostOfUser(const RevisionToCheck& revisionToCheck);

private:
  std::vector<Post> extractPostsFromRecentChanges(live_replication::RecentChangesReader& recentChangesReader,
                                                  json::Value& state);
  void processPosts(const std::vector<Post>& posts, bool dryRun);
  bool extractPostContent(const Post& post, PostAnalysis& postAnalysis);
  bool analyzePost(const Post& post, PostAnalysis& postAnalysis);

  void loadMentorStatesFile();
  const MentorState& getMentorState(const std::string& mentor) const;
  void setMentorState(const std::string& mentor, bool setAnythingForwarded, bool setThanksForwarded);

  bool postOnMentorTalkPage(const Post& post, const PostAnalysis& postAnalysis, std::string_view title,
                            std::string_view messageBody, bool dryRun);
  bool hasOptedInToReceiveThanks(const std::string& mentor) const;

  mwc::Wiki* m_wiki;
  std::string m_mentorStatesFile;
  std::unordered_map<std::string, MentorState> m_mentorStates;
  std::unique_ptr<MessageClassifier> m_messageClassifier;
};

#endif
