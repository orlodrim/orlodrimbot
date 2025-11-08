#include "emergency_stop.h"
#include <string>
#include <vector>
#include "cbl/date.h"
#include "cbl/log.h"
#include "mwclient/wiki.h"

using cbl::Date;
using cbl::DateDiff;
using mwc::Revision;
using mwc::RP_TIMESTAMP;
using mwc::RP_USER;
using mwc::UIP_EDIT_COUNT;
using mwc::UserInfo;
using mwc::Wiki;
using std::string;
using std::vector;

AdvancedUsersEmergencyStopTest::AdvancedUsersEmergencyStopTest(Wiki& wiki)
    : m_wiki(&wiki), m_initializationDate(Date::now() - DateDiff::fromMinutes(6)) {}

bool AdvancedUsersEmergencyStopTest::isEmergencyStopTriggered() {
  string userName = m_wiki->externalUserName();
  CBL_ASSERT(!userName.empty()) << "Emergency stop works only for logged in users";
  string stopPage = "User talk:" + userName;
  Revision revision = m_wiki->readPage(stopPage, RP_TIMESTAMP | RP_USER);

  if (revision.timestamp <= m_initializationDate) {
    // Simple case: no recent edit on the talk page of the bot.
    return false;
  }

  // The page was edited.
  // Require a minimum edit count, otherwise it's probably a mistake.
  bool advancedUser = true;
  if (!revision.user.empty()) {
    vector<UserInfo> users(1);
    users[0].name = revision.user;
    m_wiki->getUsersInfo(UIP_EDIT_COUNT, users);
    advancedUser = users[0].editCount >= 50;
  }
  // Does the page contains anything unexpected? If the change was reverted, it can be ignored.
  string content = m_wiki->readPageContent(stopPage);
  bool pageContainsMessage = content != "{{/En-tête}}" && !content.empty();

  if (advancedUser && pageContainsMessage) {
    // We need to stop.
    return true;
  } else {
    // The change can be ignored. Resets the initialization date so that the next check becomes trivial again.
    m_initializationDate = revision.timestamp;
    return false;
  }
}
