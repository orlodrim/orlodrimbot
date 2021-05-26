#include "bot_section.h"
#include <string>
#include <string_view>
#include "cbl/log.h"
#include "cbl/unittest.h"

using std::string;
using std::string_view;
namespace mwc {

class BotSectionTest : public cbl::Test {
private:
  CBL_TEST_CASE(readBotSection) {
    CBL_ASSERT_EQ(readBotSection("<!--BEGIN BOT SECTION-->123<!--END BOT SECTION-->"), "123");
    CBL_ASSERT_EQ(readBotSection("<!--  BEGIN BOT SECTION  -->123<!--  END BOT SECTION  -->"), "123");
    CBL_ASSERT_EQ(readBotSection("<!-- begin bot section -->123<!-- end bot section -->"), "123");
    CBL_ASSERT_EQ(
        readBotSection("<!-- début de la zone de travail du bot -->123<!-- fin de la zone de travail du bot -->"),
        "123");
    CBL_ASSERT_EQ(
        readBotSection("<!-- DÉBUT DE LA ZONE DE TRAVAIL DU BOT -->123<!-- FIN DE LA ZONE DE TRAVAIL DU BOT -->"),
        "123");

    CBL_ASSERT(readBotSection("Abc<!-- BEGIN BOT SECTION").data() == nullptr);
    CBL_ASSERT_EQ(readBotSection("Abc<!-- BEGIN BOT SECTION -->Def"), "Def");
    CBL_ASSERT_EQ(readBotSection("Abc<!-- BEGIN BOT SECTION --><!-- END BOT SECTION -->Ghi"), "");
    CBL_ASSERT_EQ(readBotSection("<ref><!--</ref><!-- BEGIN BOT SECTION -->A<!-- END BOT SECTION -->"), "A");
    CBL_ASSERT_EQ(readBotSection("Abc<!-- BEGIN BOT SECTION -->Defg<!-- END BOT SECTION -->Ghijklm"), "Defg");
    CBL_ASSERT_EQ(readBotSection("Abc<!-- BEGIN BOT SECTION -->De<!--fg<!-- END BOT SECTION -->Ghijklm"), "De<!--fg");
    CBL_ASSERT(readBotSection("Abc<!-- BEGIN BOT SECTION --->De<!--fg<!-- END BOT SECTION -->Ghijklm").data() ==
               nullptr);
    CBL_ASSERT(readBotSection("Abc<!-- BEGIN BOT SECTION ->De<!--fg<!-- END BOT SECTION -->Ghijklm").data() == nullptr);
    CBL_ASSERT_EQ(readBotSection("Abc<!-- BEGIN BOT SECTION --><!-- END BOT SECTION "), "<!-- END BOT SECTION ");
    CBL_ASSERT_EQ(readBotSection("Abc<!-- BEGIN BOT SECTION --><!-- END BOT SECTION -"), "<!-- END BOT SECTION -");
    CBL_ASSERT_EQ(readBotSection("Abc<!-- BEGIN BOT SECTION --><!-- END BOT SECTION --"), "<!-- END BOT SECTION --");

    CBL_ASSERT_EQ(readBotSection("<!-- BEGIN BOT SECTION -->\n<!-- END BOT SECTION -->"), "");
    CBL_ASSERT_EQ(readBotSection("<!-- BEGIN BOT SECTION -->\nTest\n<!-- END BOT SECTION -->"), "Test\n");
  }
  CBL_TEST_CASE(readBotSection_longInput) {
    string text;
    text.reserve(500000 * 3 + 100);
    for (int i = 0; i < 500000; i++) {
      text += "<!--";
    }
    text += "<!-- BEGIN BOT SECTION -->a<!-- END BOT SECTION -->";
    CBL_ASSERT_EQ(readBotSection(text), "a");
  }

  void checkReplaceBotSection(string_view oldCode, string_view newBotSection, int flags, string_view expectedNewCode) {
    string newCode(oldCode);
    bool result = replaceBotSection(newCode, newBotSection, flags);
    if (expectedNewCode == "<FAILURE>") {
      CBL_ASSERT(!result) << oldCode;
      CBL_ASSERT_EQ(newCode, oldCode);
    } else {
      CBL_ASSERT(result) << oldCode;
      CBL_ASSERT_EQ(newCode, expectedNewCode) << oldCode;
    }
  }
  CBL_TEST_CASE(replaceOrAddBotSection) {
    checkReplaceBotSection("<!-- BEGIN BOT SECTION --><!-- END BOT SECTION -->", "Hello", 0,
                           "<!-- BEGIN BOT SECTION -->\nHello\n<!-- END BOT SECTION -->");
    checkReplaceBotSection("<!-- BEGIN BOT SECTION --><!-- END BOT SECTION -->", "Hello\n", 0,
                           "<!-- BEGIN BOT SECTION -->\nHello\n<!-- END BOT SECTION -->");
    checkReplaceBotSection("<!-- BEGIN BOT SECTION --><!-- END BOT SECTION -->", "Hello", BS_COMPACT,
                           "<!-- BEGIN BOT SECTION -->Hello<!-- END BOT SECTION -->");
    checkReplaceBotSection("Abc <!-- BEGIN BOT SECTION -->\nShort\n<!-- END BOT SECTION --> def", "Much longer", 0,
                           "Abc <!-- BEGIN BOT SECTION -->\nMuch longer\n<!-- END BOT SECTION --> def");
    checkReplaceBotSection("Abc <!-- BEGIN BOT SECTION -->\nPretty long\n<!-- END BOT SECTION --> def", "Short", 0,
                           "Abc <!-- BEGIN BOT SECTION -->\nShort\n<!-- END BOT SECTION --> def");
    // Broken end.
    checkReplaceBotSection("X<!-- BEGIN BOT SECTION -->Y", "Hello", 0,
                           "X<!-- BEGIN BOT SECTION -->\nHello\n<!-- END BOT SECTION -->");
    checkReplaceBotSection("X<!-- BEGIN BOT SECTION -->Y", "Hello", BS_MUST_EXIST,
                           "X<!-- BEGIN BOT SECTION -->\nHello\n<!-- END BOT SECTION -->");
    checkReplaceBotSection("X<!-- BEGIN BOT SECTION -->Y<!-- END BOT SECTIO -->Z", "Hello", 0,
                           "X<!-- BEGIN BOT SECTION -->\nHello\n<!-- END BOT SECTION -->");
    checkReplaceBotSection("X<!-- BEGIN BOT SECTION -->Y<!-- END BOT SECTIO", "Hello", 0,
                           "X<!-- BEGIN BOT SECTION -->\nHello\n<!-- END BOT SECTION -->");
    // Multiple tags.
    checkReplaceBotSection(
        "A<!-- BEGIN BOT SECTION -->B<!-- BEGIN BOT SECTION -->C<!-- END BOT SECTION -->D<!-- END BOT SECTION -->E",
        "Hello", 0, "A<!-- BEGIN BOT SECTION -->\nHello\n<!-- END BOT SECTION -->E");
    checkReplaceBotSection(
        "A<!-- BEGIN BOT SECTION -->B<!-- END BOT SECTION -->C<!-- BEGIN BOT SECTION -->D<!-- END BOT SECTION -->E",
        "Hello", 0, "A<!-- BEGIN BOT SECTION -->\nHello\n<!-- END BOT SECTION -->E");
    // Bot section not found.
    checkReplaceBotSection("No bot section here", "Hello", 0,
                           "No bot section here\n<!-- BEGIN BOT SECTION -->\nHello\n<!-- END BOT SECTION -->");
    checkReplaceBotSection("No bot section here", "Hello", BS_MUST_EXIST, "<FAILURE>");
    checkReplaceBotSection("<!-- BEGIN BOT SECTIO --><!-- END BOT SECTION -->", "Hello", BS_MUST_EXIST, "<FAILURE>");
  }
};

}  // namespace mwc

int main() {
  mwc::BotSectionTest().run();
  return 0;
}
