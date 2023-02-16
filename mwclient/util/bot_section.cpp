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

// Extracts the first wikicode comment in `code` from `position`.
// Sets `comment` to the full comment including delimiters and `normalizedContent` to the normalized inner text.
// If there are multiple "<!--" before the first "-->", assumes that the comment starts from the last one.
static bool extractComment(string_view code, size_t& position, string_view& comment, string& normalizedContent) {
  constexpr string_view OPENING = "<!--", CLOSURE = "-->";
  size_t commentStart = code.find(OPENING, position);
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

// Returns pointers to the beginning and the end of the bot section, i.e. text between <!-- BEGIN BOT SECTION --> and
// <!-- END BOT SECTION -->.
// Returns {nullptr, nullptr} if there is no bot section or {<pointer>, nullptr} if the bot section is not closed.
static pair<const char*, const char*> getBotSectionBoundaries(string_view code) {
  size_t position = 0;
  string_view comment;
  string normalizedContent;
  const char* sectionStart = nullptr;
  const char* sectionEnd = nullptr;
  while (extractComment(code, position, comment, normalizedContent)) {
    if (normalizedContent == "BEGIN BOT SECTION" || normalizedContent == "DÉBUT DE LA ZONE DE TRAVAIL DU BOT") {
      sectionStart = comment.data() + comment.size();
      break;
    }
  }
  if (sectionStart != nullptr) {
    while (extractComment(code, position, comment, normalizedContent)) {
      if (normalizedContent == "END BOT SECTION" || normalizedContent == "FIN DE LA ZONE DE TRAVAIL DU BOT") {
        sectionEnd = comment.data();
      }
    }
  }
  return {sectionStart, sectionEnd};
}

string_view readBotSection(string_view code) {
  auto [start, end] = getBotSectionBoundaries(code);
  if (start != nullptr && end == nullptr) {
    end = code.data() + code.size();
  }
  string_view botSection(start, end - start);
  if (!botSection.empty() && botSection.front() == '\n') {
    botSection.remove_prefix(1);
  }
  return botSection;
}

bool replaceBotSection(string& code, string_view newBotSection, int flags) {
  auto [start, end] = getBotSectionBoundaries(code);
  if (start == nullptr && (flags & BS_MUST_EXIST)) {
    return false;
  }
  string codeAfterBotSection = end != nullptr ? code.substr(end - code.data()) : "<!-- END BOT SECTION -->";
  if (start != nullptr) {
    code.resize(start - code.data());
  } else {
    if (!code.empty() && code.back() != '\n') {
      code += "\n";
    }
    code += "<!-- BEGIN BOT SECTION -->";
  }
  if (!(flags & BS_COMPACT)) {
    code += '\n';
  }
  code += newBotSection;
  if (!(flags & BS_COMPACT) && !newBotSection.empty() && newBotSection.back() != '\n') {
    code += '\n';
  }
  code += codeAfterBotSection;
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
