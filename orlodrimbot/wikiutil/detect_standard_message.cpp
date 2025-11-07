#include "detect_standard_message.h"
#include <re2/re2.h>
#include <string>
#include <string_view>

using std::string;
using std::string_view;

namespace wikiutil {
namespace {

string_view getFirstLine(string_view text) {
  return text.substr(0, text.find('\n'));
}

bool threadContainsResponses(string_view threadContent) {
  return threadContent.find("\n:") != string::npos;
}

int countUserLinks(string_view threadContent) {
  static const re2::RE2 reUserLink(R"(\[\[(Utilisateur|Utilisatrice):)");
  int occurrences = 0;
  re2::StringPiece content = threadContent;
  for (; RE2::FindAndConsume(&content, reUserLink); occurrences++) {}
  return occurrences;
}

bool isAfD(string_view threadContent) {
  static const re2::RE2 reAfDTitle(
      "== *(L['’]admissibilité de .* est débattue|L['’]article .* est proposé à la suppression|"
      "Avertissement suppression .*) *==");
  return RE2::PartialMatch(getFirstLine(threadContent), reAfDTitle) &&
         (threadContent.find("obtenir un consensus pour la conservation") != string_view::npos ||
          threadContent.find("obtenir un consensus sur l'admissibilité") != string_view::npos ||
          threadContent.find("ne garantissent aucun droit à avoir un article") != string::npos ||
          threadContent.find("Accéder au débat") != string::npos) &&
         !threadContainsResponses(threadContent) && countUserLinks(threadContent) <= 1;
}

bool isDidYouKnow(string_view threadContent) {
  static const re2::RE2 reAnecdoteTitle("== *Proposition d['’]anecdote pour la page d['’]accueil.*==");
  return RE2::PartialMatch(getFirstLine(threadContent), reAnecdoteTitle) &&
         threadContent.find("GhosterBot") != string_view::npos && !threadContainsResponses(threadContent);
}

}  // namespace

StandardMessage detectStandardMessage(string_view section) {
  StandardMessage result;
  if (isAfD(section)) {
    result.type = StandardMessage::AFD;
  } else if (isDidYouKnow(section)) {
    result.type = StandardMessage::DID_YOU_KNOW;
  } else {
    result.type = StandardMessage::NONE;
  }
  return result;
}

}  // namespace wikiutil
