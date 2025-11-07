#include "message_classifier.h"
#include <memory>
#include <string>
#include <string_view>
#include "cbl/error.h"
#include "cbl/http_client.h"
#include "cbl/json.h"
#include "cbl/llm_query.h"
#include "cbl/log.h"
#include "cbl/string.h"

using std::make_unique;
using std::string;
using std::string_view;

// TODO: Some examples are omitted here as they should be anonymized first.
constexpr string_view PROMPT = R"prompt(
Ta tâche est de reconnaître la langue et la catégorie d'un message en wikicode posté sur une page de discussion de Wikipédia.
Le message est délimité par les marqueurs [début entrée] et [fin entrée].
Donne la langue comme un code ISO 639-1 (par défaut "fr" s'il n'y a aucun mot identifiable).
Classe le message dans l'une des catégories : WikiQuestion, NonWikiQuestion, Thanks, ArticleDraft, Other.
Par ailleurs, vérifie si l'utilisateur indique être bloqué en écriture (attribut "blocked")

Procède de la façon suivante :
- Analyse si l'utilisateur pose une question ou exprime implicitement une demande, fait part d'une incompréhension ou d'une frustration (sans être menaçant ni injurieux). Cela peut passer par une formule de politesse telle que "merci de votre réponse". Dans ce cas, la réponse est WikiQuestion, soit NonWikiQuestion. Sinon, la réponse est Thanks, ArticleDraft ou Other. Cas particulier : si le message contient une déclaration de conflit d'intérêt, seules les questions directes et explicites doivent conduire à classer en WikiQuestion / NonWikiQuestion.
- Pour distinguer entre WikiQuestion et NonWikiQuestion : classe dans WikiQuestion si la question concerne le fonctionnement de Wikipédia, la modification de pages, l'ajout d'images, la mise en forme, les sources, la suppression de pages ou le système de discussion lui-même. Classe dans NonWikiQuestion les autres questions, notamment les questions de connaissance générale, la recherche d'emploi ou de stage. En l'absence de contexte, la présence du mot spécial "monmentor" (le mentor assigné à l'utilisateur) fait pencher vers WikiQuestion.
- Pour distinguer entre Thanks, ArticleDraft et Other : Thanks est pour les messages de remerciements n'attendant pas de réponse. ArticleDraft est pour les brouillons d'article. Un message long et impersonnel (sans pronoms "je", "tu" ou "vous") suggère la catégorie ArticleDraft. Other est pour tous les autres messages, notamment les simples déclarations et les messages menaçants, injurieux ou incompréhensibles.

Voici des exemples, éventuellement associés à une explication.

[début entrée]
Bonjour, comment puis-je insérer une image ? [[Utilisateur:Jack|Jack]] ([[Discussion utilisateur:Jack|discussion]]) 20 juin 2023 à 18:22 (CEST)
[fin entrée]
Sortie : {"language": "fr", "category": "WikiQuestion"}

[début entrée]
Je n'ai pas trouvé comment créer un article. Pourriez-vous m'aider [[Utilisateur:Mathilde|Mathilde]] ([[Discussion utilisateur:Mathilde|discussion]]) 1 juillet 2021 à 7:15 (CEST)
[fin entrée]
Sortie : {"language": "fr", "category": "WikiQuestion"}

[début entrée]
Merci madame, je suis perdue et je ne trouve pas où poser une question"
[fin entrée]
Sortie : {"language": "fr", "category": "WikiQuestion"}
Explication : il s'agit d'une demande implicite concernant le système de discussion de Wikipédia.

[début entrée]
I tried to fix the game rules. I don't understand why my change was reverted. [[Utilisateur:Stan|Stan]] ([[Discussion utilisateur:Stan|discussion]]) 30 janvier 2023 à 12:40 (CET)
[fin entrée]
Sortie : {"language": "en", "category": "WikiQuestion"}

[début entrée]
:Bonjour monmentor, je ne comprends pas.
[fin entrée]
Sortie : {"language": "fr", "category": "WikiQuestion"}

[début entrée]
Est-ce que la Lune est une planète ? [[Utilisateur:Pierre|Pierre]] ([[Discussion utilisateur:Pierre|discussion]]) 20 juin 2023 à 18:22 (CEST)
[fin entrée]
Sortie : {"language": "fr", "category": "NonWikiQuestion"}

[début entrée]
Comment installer un antivirus dans Windows ? [[Utilisateur:User4721|User4721]] ([[Discussion utilisateur:User4721|discussion]]) 8 septembre 2019 à 20:55 (CEST)
[fin entrée]
Sortie : {"language": "fr", "category": "NonWikiQuestion"}

[début entrée]
Je cherche un stage pour finir mes études, pourriez-vous m'aider ?
[fin entrée]
Sortie : {"language": "fr", "category": "NonWikiQuestion"}

[début entrée]
Bonjour monmentor,
Merci de votre accueil. [[Utilisateur:ExperteHistoire|ExperteHistoire]] ([[Discussion utilisateur:ExperteHistoire|discussion]]) 14 novembre 2024 à 15:36 (CET)
[fin entrée]
Sortie : {"language": "fr", "category": "Thanks"}

[début entrée]
C'est noté, je tâcherai de lire les liens que vous m'avez donnés. [[Utilisateur:AssistantCom|AssistantCom]] ([[Discussion utilisateur:AssistantCom|discussion]]) 2 février 2021 à 14:30 (CET)
[fin entrée]
Sortie : {"language": "fr", "category": "Thanks"}

[début entrée]
== Claude Roy ==
Claude Roy est né dans à Paris le 28 août 1915. Issu d'un père artiste peintre d'origine espagnole et d'une mère originaire de Charente, il a été élevé à Jarnac. Pendant ses études, il développe une amitié avec le futur président de la République François Mitterrand, avec qui il partageait une partie de son parcours académique. Après avoir fréquenté le lycée Guez-de-Balzac à Angoulême et poursuivi ses études à l'université de Bordeaux, il part à Paris en 1935 dans le dessein de s'inscrire à la faculté de droit.
[fin entrée]
Sortie : {"language": "fr", "category": "ArticleDraft"}

[début entrée]
J'exige que mon article soit restauré immédiatement. Sinon, je vous ferai un procès pour atteinte à la liberté d'expression.
[fin entrée]
Sortie : {"language": "fr", "category": "Other"}
Explication : même si le message contient une demande, il est menaçant et doit donc être classé en "Other".

[début entrée]
Merci de ton accueil, gros connard.
[fin entrée]
Sortie : {"language": "fr", "category": "Other"}
Explication : même si le message contient des remerciements, il est injurieux et doit donc être classé en "Other".

[début entrée]
ok
[fin entrée]
Sortie : {"language": "fr", "category": "Other"}

[début entrée]
Je suis ici pour partager mes connaissances sur l'antiquité. [[Utilisateur:WikiPassion|WikiPassion]] ([[Discussion utilisateur:WikiPassion|discussion]]) 7 décembre 2020 à 5:05 (CET)
[fin entrée]
Sortie : {"language": "fr", "category": "Other"}

Voici l'entrée à traiter.

[début entrée]
<INPUT>
[fin entrée]
Sortie :
)prompt";

string_view getStringOfLanguage(MessageClassification::Language language) {
  switch (language) {
    case MessageClassification::Language::FRENCH:
      return "fr";
    case MessageClassification::Language::ENGLISH:
      return "en";
    case MessageClassification::Language::OTHER:
      return "other";
    case MessageClassification::Language::UNKNOWN:
      break;
  }
  return "unknown";
};

string_view getStringOfCategory(MessageClassification::Category category) {
  switch (category) {
    case MessageClassification::Category::WIKI_QUESTION:
      return "WikiQuestion";
    case MessageClassification::Category::NON_WIKI_QUESTION:
      return "NonWikiQuestion";
    case MessageClassification::Category::THANKS:
      return "Thanks";
    case MessageClassification::Category::ARTICLE_DRAFT:
      return "ArticleDraft";
    case MessageClassification::Category::OTHER:
      return "Other";
    case MessageClassification::Category::UNKNOWN:
      break;
  }
  return "Unknown";
};

MessageClassification::Language getLanguageOfString(string_view languageString) {
  if (languageString == "fr") {
    return MessageClassification::Language::FRENCH;
  } else if (languageString == "en") {
    return MessageClassification::Language::ENGLISH;
  } else if (!languageString.empty()) {
    return MessageClassification::Language::OTHER;
  }
  return MessageClassification::Language::UNKNOWN;
}

MessageClassification::Category getCategoryOfString(string_view categoryString) {
  if (categoryString == "WikiQuestion") {
    return MessageClassification::Category::WIKI_QUESTION;
  } else if (categoryString == "NonWikiQuestion") {
    return MessageClassification::Category::NON_WIKI_QUESTION;
  } else if (categoryString == "Thanks") {
    return MessageClassification::Category::THANKS;
  } else if (categoryString == "ArticleDraft") {
    return MessageClassification::Category::ARTICLE_DRAFT;
  } else if (categoryString == "Other") {
    return MessageClassification::Category::OTHER;
  }
  return MessageClassification::Category::UNKNOWN;
}

string MessageClassification::debugString() const {
  return cbl::concat("{llmLanguage=", getStringOfLanguage(llmLanguage),
                     ", llmCategory=", getStringOfCategory(llmCategory),
                     ", localModelCategory=", getStringOfCategory(localModelCategory), "}");
}

string_view MessageClassifier::getPrompt() {
  return PROMPT;
}

MessageClassifier::MessageClassifier(string_view localClassifierCommand, cbl::LLMClient* llmClient)
    : m_localClassifierCommand(localClassifierCommand), m_llmClient(llmClient) {
  if (m_llmClient == nullptr) {
    m_ownedLLMClient = make_unique<cbl::LLMClient>();
    m_llmClient = m_ownedLLMClient.get();
  }
}

void MessageClassifier::callLocalClassifier(string_view message, MessageClassification& classification) const {
}

void MessageClassifier::callLLM(string_view message, MessageClassification& classification) const {
  json::Value generationConfig = json::parse(R"({
    "responseMimeType": "application/json",
    "responseSchema": {
      "type": "object",
      "properties": {
        "language": { "type": "string" },
        "category": { "type": "string", "enum": ["WikiQuestion", "NonWikiQuestion", "Thanks", "ArticleDraft", "Other"] },
        "blocked": { "type": "boolean" }
      },
      "propertyOrdering": ["language", "category", "blocked"]
    }
  })");
  cbl::LLMResponse response;
  string_view truncatedMessage = message;
  constexpr int MAX_MESSAGE_LENGTH = 10'000;
  if (truncatedMessage.size() >= MAX_MESSAGE_LENGTH) {
    truncatedMessage = truncatedMessage.substr(0, MAX_MESSAGE_LENGTH);
    size_t lastSpace = truncatedMessage.rfind(' ');
    if (lastSpace != string::npos) {
      truncatedMessage = truncatedMessage.substr(0, lastSpace);
    }
  }
  try {
    response =
        m_llmClient->generateResponse({.text = cbl::replace(cbl::trim(PROMPT), "<INPUT>", cbl::trim(truncatedMessage)),
                                       .thinkingBudget = 1024,
                                       .includeThoughts = true,
                                       .generationConfig = generationConfig.copy()});
  } catch (const cbl::HTTPError& error) {
    CBL_ERROR << "LLM query failed: " << error.what();
    return;
  }
  if (m_printThought && !response.thought.empty()) {
    CBL_INFO << "thought=" << response.thought;
  }
  json::Value parsedResponse;
  try {
    parsedResponse = json::parse(response.text);
  } catch (const cbl::ParseError& error) {
    CBL_ERROR << "Failed to parse response from LLM: error=" << error.what() << " response=" << response.text;
    return;
  }
  classification.llmLanguage = getLanguageOfString(parsedResponse["language"].str());
  classification.llmCategory = getCategoryOfString(parsedResponse["category"].str());
  classification.llmBlocked = parsedResponse["blocked"].boolean();
}

MessageClassification MessageClassifier::classify(std::string_view message) const {
  MessageClassification classification;
  callLocalClassifier(message, classification);
  callLLM(message, classification);
  return classification;
}
