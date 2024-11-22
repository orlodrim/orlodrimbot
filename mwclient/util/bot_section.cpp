#include "bot_section.h"
#include <string>
#include <string_view>
#include <utility>
#include "cbl/string.h"
#include "cbl/unicode_fr.h"
#include "mwclient/wiki.h"

using std::pair;
using std::string;
using std::string_view;

namespace mwc {
namespace {

// Extracts the first wikicode comment in `code` from `position`.
// Sets `comment` to the full comment including delimiters and `normalizedContent` to the normalized inner text.
// If there are multiple "<!--" before the first "-->", assumes that the comment starts from the last one.
bool extractComment(string_view code, size_t& position, size_t& commentStart, string_view& comment,
                    string& normalizedContent) {
  constexpr string_view OPENING = "<!--", CLOSURE = "-->";
  commentStart = code.find(OPENING, position);
  while (commentStart != string_view::npos) {
    size_t textStart = commentStart + OPENING.size();
    position = code.find(OPENING, textStart);
    size_t textEnd = code.substr(0, position).find(CLOSURE, textStart);
    if (textEnd != string_view::npos) {
      comment = code.substr(commentStart, textEnd + CLOSURE.size() - commentStart);
      normalizedContent = unicode_fr::toUpperCase(cbl::trim(code.substr(textStart, textEnd - textStart)));
      return true;
    }
    commentStart = position;
  }
  return false;
}

struct SplitPage {
  string_view prefix;
  string_view botSection;
  string_view suffix;
  bool hasBeginMarker = false;
  bool hasEndMarker = false;
  int64_t updateCounter = 0;
};

SplitPage parseBotSection(string_view code) {
  constexpr string_view UPDATE_COUNTER_PREFIX = "UPDATE #";

  SplitPage splitPage;
  size_t position = 0;
  size_t commentStart = 0;
  string_view comment;
  string normalizedContent;
  int state = 0;
  size_t sectionStart = 0;

  while (extractComment(code, position, commentStart, comment, normalizedContent)) {
    switch (state) {
      case 0:
        if (normalizedContent == "BEGIN BOT SECTION" || normalizedContent == "DÉBUT DE LA ZONE DE TRAVAIL DU BOT") {
          splitPage.hasBeginMarker = true;
          sectionStart = commentStart + comment.size();
          splitPage.prefix = code.substr(0, sectionStart);
          state = 1;
        }
        break;
      case 1:
        state = 2;
        if (commentStart == splitPage.prefix.size() && normalizedContent.starts_with(UPDATE_COUNTER_PREFIX)) {
          splitPage.updateCounter = atoll(normalizedContent.c_str() + UPDATE_COUNTER_PREFIX.size());
          if (splitPage.updateCounter < 0 || splitPage.updateCounter >= 0x7FFF'FFFF'FFFF'FFFF) {
            splitPage.updateCounter = 0;
          }
          sectionStart += comment.size();
          break;
        }
        [[fallthrough]];
      case 2:
      case 3:
        if (normalizedContent == "END BOT SECTION" || normalizedContent == "FIN DE LA ZONE DE TRAVAIL DU BOT") {
          splitPage.hasEndMarker = true;
          splitPage.botSection = code.substr(sectionStart, commentStart - sectionStart);
          splitPage.suffix = code.substr(commentStart);
          state = 3;
        }
        break;
    }
  }
  switch (state) {
    case 0:
      splitPage.prefix = code;
      break;
    case 1:
    case 2:
      splitPage.botSection = code.substr(sectionStart);
      break;
  }

  return splitPage;
}

bool botSectionChanged(string_view oldBotSection, string_view newBotSection, int flags) {
  if (!(flags & BS_COMPACT)) {
    if (!oldBotSection.starts_with("\n")) {
      return true;
    }
    oldBotSection.remove_prefix(1);
    if (!newBotSection.empty() && newBotSection.back() != '\n') {
      if (!oldBotSection.ends_with("\n")) {
        return true;
      }
      oldBotSection.remove_suffix(1);
    }
  }
  return oldBotSection != newBotSection;
}

}  // namespace

string_view readBotSection(string_view code) {
  string_view botSection = parseBotSection(code).botSection;
  if (botSection.starts_with("\n")) {
    botSection.remove_prefix(1);
  }
  return botSection;
}

bool replaceBotSection(string& code, string_view newBotSection, int flags) {
  SplitPage splitPage = parseBotSection(code);

  if (!splitPage.hasBeginMarker && (flags & BS_MUST_EXIST)) {
    return false;
  } else if (!botSectionChanged(splitPage.botSection, newBotSection, flags)) {
    // Exit early if there is no change. With BS_UPDATE_COUNTER, this prevents changing the page only to increment the
    // counter.
    return true;
  }

  string_view newLine1 = !splitPage.hasBeginMarker && !code.empty() && code.back() != '\n' ? "\n" : "";
  string_view beginMarker = !splitPage.hasBeginMarker ? "<!-- BEGIN BOT SECTION -->" : "";
  string updaterCounterComment = (flags & BS_UPDATE_COUNTER)
                                     ? cbl::concat("<!-- update #", std::to_string(splitPage.updateCounter + 1), " -->")
                                     : "";
  string_view newLine2 = !(flags & BS_COMPACT) ? "\n" : "";
  string_view endMarker = !splitPage.hasEndMarker ? "<!-- END BOT SECTION -->" : "";
  string_view newLine3 = !(flags & BS_COMPACT) && !newBotSection.empty() && newBotSection.back() != '\n' ? "\n" : "";

  code = cbl::concat(splitPage.prefix, newLine1, beginMarker, updaterCounterComment, newLine2, newBotSection, newLine3,
                     endMarker, splitPage.suffix);
  return true;
}

bool replaceBotSectionInPage(Wiki& wiki, const string& title, string_view newBotSection, const string& summary,
                             int botSectionFlags) {
  bool result = false;
  wiki.editPage(title, [&](string& content, string& editPageSummary) {
    editPageSummary = summary;
    result = replaceBotSection(content, newBotSection, botSectionFlags);
  });
  return result;
}

}  // namespace mwc
