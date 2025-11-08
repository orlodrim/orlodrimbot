#include "emergency_stop.h"
#include <string>
#include <vector>
#include "cbl/date.h"
#include "cbl/log.h"
#include "cbl/unittest.h"
#include "mwclient/mock_wiki.h"
#include "mwclient/wiki.h"

using cbl::Date;
using mwc::MockWiki;
using mwc::UserInfo;
using std::string;
using std::vector;

class MockWikiWithUserInfo : public MockWiki {
public:
  using MockWiki::readPage;

  void getUsersInfo(int properties, vector<UserInfo>& users) override {
    usersInfoCalls++;
    CBL_ASSERT_EQ(properties, mwc::UIP_EDIT_COUNT);
    for (UserInfo& user : users) {
      user.editCount = user.name.starts_with("Trusted") ? 50 : 49;
    }
  }

  void setInternalUserName(const string& value) { MockWiki::setInternalUserName(value); }

  int usersInfoCalls = 0;
};

class AdvancedUsersEmergencyStopTestTest : public cbl::Test {
private:
  void setUp() override {
    Date::setFrozenValueOfNow(Date::fromISO8601("1999-12-31T00:00:00Z"));
    m_wiki.resetDatabase();
    m_wiki.setInternalUserName("Bot");
    writeBotPageAs("TrustedUser", "{{/En-tête}}");
    Date::setFrozenValueOfNow(Date::fromISO8601("2000-01-01T00:00:00Z"));
  }

  void writeBotPageAs(const string& user, const string& content) {
    const string botName = m_wiki.externalUserName();
    m_wiki.setInternalUserName(user);
    m_wiki.setPageContent("User talk:" + botName, content);
    m_wiki.setInternalUserName(botName);
  }

  CBL_TEST_CASE(NoEditOnTalkPage) {
    AdvancedUsersEmergencyStopTest test(m_wiki);
    CBL_ASSERT(!test.isEmergencyStopTriggered());
  }

  CBL_TEST_CASE(TalkPageEditedByTrustedUser) {
    Date::setFrozenValueOfNow(Date::fromISO8601("2000-01-01T00:05:00Z"));
    writeBotPageAs("TrustedUser", "{{/En-tête}} stop");
    Date::setFrozenValueOfNow(Date::fromISO8601("2000-01-01T00:10:00Z"));
    AdvancedUsersEmergencyStopTest test(m_wiki);
    CBL_ASSERT(test.isEmergencyStopTriggered());
    writeBotPageAs("TrustedUser", "{{/En-tête}}");
    m_wiki.usersInfoCalls = 0;
    CBL_ASSERT(!test.isEmergencyStopTriggered());
    CBL_ASSERT_EQ(m_wiki.usersInfoCalls, 1);
    CBL_ASSERT(!test.isEmergencyStopTriggered());
    CBL_ASSERT_EQ(m_wiki.usersInfoCalls, 1);
  }

  CBL_TEST_CASE(TalkPageEditedByUntrustedUser) {
    Date::setFrozenValueOfNow(Date::fromISO8601("2000-01-01T00:15:00Z"));
    writeBotPageAs("UntrustedUser", "{{/En-tête}} stop");
    Date::setFrozenValueOfNow(Date::fromISO8601("2000-01-01T00:20:00Z"));
    AdvancedUsersEmergencyStopTest test(m_wiki);
    m_wiki.usersInfoCalls = 0;
    CBL_ASSERT(!test.isEmergencyStopTriggered());
    CBL_ASSERT_EQ(m_wiki.usersInfoCalls, 1);
    CBL_ASSERT(!test.isEmergencyStopTriggered());
    CBL_ASSERT_EQ(m_wiki.usersInfoCalls, 1);
  }

  MockWikiWithUserInfo m_wiki;
};

int main() {
  AdvancedUsersEmergencyStopTestTest advancedUsersEmergencyStopTestTest;
  advancedUsersEmergencyStopTestTest.run();
  return 0;
}
