#include "raw_lib.h"
#include <re2/re2.h>
#include <climits>
#include <string>
#include <string_view>
#include "cbl/date.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "mwclient/parser.h"
#include "mwclient/util/templates_by_name.h"
#include "mwclient/wiki.h"
#include "newsletter_distributor.h"

using cbl::Date;
using cbl::DateDiff;
using newsletters::Distributor;
using std::string;
using std::string_view;

static string getIssueSubpage(const string& issueTitle) {
  size_t slashPosition = issueTitle.find('/');
  return issueTitle.substr(slashPosition == string::npos ? 0 : slashPosition + 1);
}

bool RAWDistributor::compareIssues(const std::string& issue1, const std::string& issue2) {
  return getIssueSubpage(issue1) < getIssueSubpage(issue2);
}

Distributor::Result RAWDistributor::canBeCurrentIssueTitle(const string& issueTitle) const {
  static const re2::RE2 reValidIssue("[12][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]");
  string subpage = getIssueSubpage(issueTitle);
  if (!RE2::FullMatch(subpage, reValidIssue) || issueTitle != getSubpagesPrefix() + subpage) {
    return Result(issueTitle, "not a valid title",
                  "[[" + issueTitle + "]] n'est pas un titre valide pour un numéro de RAW.");
  }
  Date issueDate = Date::fromISO8601(subpage + "T00:00:00Z");
  Date now = Date::now();
  if (issueDate < now - DateDiff::fromDays(5)) {
    return Result(issueTitle, "too old", "le numéro est trop ancien pour être publié.");
  } else if (issueDate > now + DateDiff::fromDays(5)) {
    return Result(issueTitle, "in the future", "le numéro a une date trop éloignée dans le futur.");
  }
  return Result();
}

Distributor::Result RAWDistributor::isIssueReadyForPublication(const std::string& issueTitle, int& issueNumber) const {
  const int MIN_PAGE_SIZE = 250;
  string content;
  try {
    content = m_wiki->readPageContent(issueTitle);
  } catch (const mwc::PageNotFoundError& error) {
    return Result(issueTitle, error.what(), "la page n'existe pas.");
  } catch (const mwc::WikiError& error) {
    return Result(issueTitle, error.what(), "la lecture de la page a échoué.");
  }
  if (content.size() < MIN_PAGE_SIZE) {
    return Result(issueTitle, "page too short", "la page est trop courte.");
  }

  bool templateFound = false;
  issueNumber = 0;
  wikicode::List parsedCode = wikicode::parse(content);
  for (const wikicode::Template& template_ : wikicode::getTemplatesByName(*m_wiki, parsedCode, "RAW/En-tête")) {
    issueNumber = cbl::parseIntInRange(template_.getParsedFields()["numéro"], 1, INT_MAX, 0);
    templateFound = true;
    break;
  }
  if (issueNumber == 0) {
    if (templateFound) {
      return Result(issueTitle, "issue number not found in {{RAW/En-tête}}",
                    "le modèle {{m|RAW/En-tête}} ne contient pas de numéro d'édition valide.");
    } else {
      return Result(issueTitle, "{{RAW/En-tête}} not found", "modèle {{m|RAW/En-tête}} non trouvé dans la page.");
    }
  }

  return Result();
}

string RAWDistributor::getIssueFromSection(const string& section) {
  static const re2::RE2 reNewsletterTitle(
      "^== *\\[\\[(:w:fr:)?Wikipédia:(RAW|Regards sur l'actualité de la Wikimedia)/[-0-9]+(/[0-9]+)?\\|"
      "RAW [-0-9]+]] *==\n");
  if (!RE2::PartialMatch(section, reNewsletterTitle)) {
    return "";
  }
  size_t start = section.find('/');
  CBL_ASSERT(start != string::npos);
  start++;
  size_t end = section.find('|', start);
  CBL_ASSERT(end != string::npos);
  return getSubpagesPrefix() + section.substr(start, end - start);
}

bool RAWDistributor::isStandardNewsletterSection(const string& message) {
  static const re2::RE2 reLineToIgnore(
      "^(\\{\\{(Regards sur l'actualité de la Wikimedia/PdD|RAW/PdD|RAW/Distribution)\\||"
      "<!-- Message envoyé par|"
      "(— |-- )?\\[\\[([Uu]ser|[Uu]tilisateur|[Uu]ser_talk):(Cantons-de-l|BeBot)|\\s*$|"
      "<table.*Regards sur l'actualité de la Wikimedia|"
      "<small>À partir)");
  bool titleRead = false;
  for (string_view line : cbl::splitLines(message)) {
    if (!titleRead) {
      titleRead = true;
    } else if (!RE2::PartialMatch(line, reLineToIgnore)) {
      return false;
    }
  }
  return true;
}

void RAWDistributor::prepareMessage(const string& issueTitle, string& title, string& nowikiTitle, string& content,
                                    string& editSummary) {
  string subpage = getIssueSubpage(issueTitle);
  nowikiTitle = "RAW " + subpage;
  title = "[[" + issueTitle + "|" + nowikiTitle + "]]";
  content = "{{RAW/Distribution|" + subpage + "}}";
}

void RAWDistributor::prepareTweet(const string& issueTitle, int issueNumber, string& text, string& image,
                                  string& editSummary) {
  text = "Le n° " + std::to_string(issueNumber) +
         " des « Regards sur l'actualité de la Wikimedia » est sorti : https://fr.wikipedia.org/wiki/" + issueTitle;
  image = "Proposition Washington.svg";
  editSummary = "Annonce de la publication de RAW " + getIssueSubpage(issueTitle);
}

void RAWDistributor::sendFailureNotification(const string& issueTitle, const string& displayableError) const {
  string content;
  if (!issueTitle.empty()) {
    content += "La distribution de [[" + issueTitle + "]] a échoué : ";
  }
  content += displayableError;
  try {
    m_wiki->writePage("User:OrlodrimBot/RAW/Erreur", content, mwc::WriteToken::newWithoutConflictDetection());
  } catch (const mwc::WikiError& error) {
    CBL_ERROR << "An error happened while trying to write the error report on wiki: " << error.what();
  }
}
