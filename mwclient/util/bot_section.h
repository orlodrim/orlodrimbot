// Module to read and write bot sections.
// A bot section is a section in wikicode delimited by <!-- BEGIN BOT SECTION --> and <!-- END BOT SECTION -->.
// Only one bot section per page is supported.
#ifndef MWC_UTIL_BOT_SECTION_H
#define MWC_UTIL_BOT_SECTION_H

#include <string>
#include <string_view>
#include "mwclient/wiki.h"

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
  // Include a counter in the bot section to prevent MediaWiki from detecting edits as a rollback.
  // This is intended for pages that are regularly reset to a base state. For instance, on a page where the bot
  // publishes a list of errors each month and humans fixes them one by one (removing them from the list), having a
  // counter prevents edits that clear the page from being detected as a rollbacks.
  // This may become more important if T154637 is resolved (https://phabricator.wikimedia.org/T154637#6489777).
  BS_UPDATE_COUNTER = 4,
};

// Replaces the content of the bot section in `code` with `newBotSection`.
// Unless BS_COMPACT is set, a '\n' is inserted between <!-- BEGIN BOT SECTION --> and newBotSection and a
// '\n' is inserted before <!-- END BOT SECTION --> if newBotSection does not already end with '\n'.
// Unless BS_MUST_EXIST is set, a new bot section is created at the end of the page is none is found.
bool replaceBotSection(std::string& code, std::string_view newBotSection, int flags = 0);

bool replaceBotSectionInPage(Wiki& wiki, std::string_view title, std::string_view newBotSection,
                             const std::string& summary = std::string(), int botSectionFlags = 0);

}  // namespace mwc

#endif
