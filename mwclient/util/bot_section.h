// Module to read and write bot sections.
// A bot section is a section in wikicode delimited by <!-- BEGIN BOT SECTION --> and <!-- END BOT SECTION -->.
// Only one bot section per page is supported.
#ifndef MWC_UTIL_BOT_SECTION_H
#define MWC_UTIL_BOT_SECTION_H

#include <string>
#include <string_view>

namespace mwc {

// Reads the bot section in `code`.
// If there is no bot section, returns string_view().
// If the closing comment is missing, returns the content from <!-- BEGIN BOT SECTION --> to the end of the page.
// If <!-- BEGIN BOT SECTION --> is immediately followed by '\n', the '\n' is stripped from the returned string.
std::string_view readBotSection(std::string_view code);

enum BotSectionFlags {
  // Return an error if there is no bot section on the page.
  BS_MUST_EXIST = 1,
  // Do not add any '\n' before or after the content.
  BS_COMPACT = 2,
};

// Replaces the content of the bot section in `code` with `newBotSection`.
// Unless BS_COMPACT is set, a '\n' is inserted between <!-- BEGIN BOT SECTION --> and newBotSection and a
// '\n' is inserted before <!-- END BOT SECTION --> if newBotSection does not already end with '\n'.
// Unless BS_MUST_EXIST is set, a new bot section is created at the end of the page is none is found.
bool replaceBotSection(std::string& code, std::string_view newBotSection, int flags = 0);

}  // namespace mwc

#endif
