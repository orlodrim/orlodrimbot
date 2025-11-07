#include "detect_standard_message.h"
#include <string>
#include <string_view>
#include <vector>
#include "cbl/log.h"
#include "cbl/string.h"
#include "cbl/unittest.h"

using std::string;
using std::string_view;
using std::vector;

namespace wikiutil {

constexpr string_view AFD_MESSAGES = R"(
== L'article Le Silence de la Mule est proposé à la suppression ==

{| align="center" style="background-color: transparent;" cellpadding="5px" cellspacing="5px"
| [[Fichier:Questionmark.png|70px|Page proposée à la suppression]]
| Bonjour,

L’article « '''{{Lien à supprimer|1=Le Silence de la Mule}}''' » est proposé à la suppression ({{cf.}} [[Wikipédia:Pages à supprimer]]). Après avoir pris connaissance des [[Wikipédia:Notoriété|critères généraux d’admissibilité des articles]] et des [[Wikipédia:Liste des critères spécifiques de notoriété|critères spécifiques]], vous pourrez [[Aide:Arguments à éviter lors d'une procédure de suppression|donner votre avis]] sur la page de discussion '''[[{{TALKPAGENAME:Le Silence de la Mule}}/Suppression]]'''.

Le meilleur moyen d’obtenir un consensus pour la conservation de l’article est de fournir des [[Wikipédia:Citez vos sources|sources secondaires fiables et indépendantes]]. Si vous ne pouvez trouver de telles sources, c’est que l’article n’est probablement pas admissible. N’oubliez pas que les [[Wikipédia:Principes fondateurs|principes fondateurs]] de Wikipédia ne garantissent aucun droit à avoir un article sur Wikipédia.

<nowiki />[[Utilisateur:Chris a liege|Chris a liege]] ([[Discussion utilisateur:Chris a liege|discuter]]) 17 octobre 2015 à 02:22 (CEST)
|}

== L'article Chasse gardée est proposé à la suppression ==
[[Image:Icono consulta borrar.png|70px|link=|Page proposée à la suppression|gauche]]
Bonjour,

L’article « '''{{Lien à supprimer|1=Chasse gardée}}''' » est proposé à la suppression ({{cf.}} [[Wikipédia:Pages à supprimer]]). Après avoir pris connaissance des [[Wikipédia:Critères d'admissibilité des articles|critères généraux d’admissibilité des articles]] et des [[:Catégorie:Wikipédia:Admissibilité des articles|critères spécifiques]], vous pourrez [[Aide:Arguments à éviter lors d'une procédure de suppression|donner votre avis]] sur la page de discussion '''[[{{TALKPAGENAME:Chasse gardée}}/Suppression]]'''.

Le meilleur moyen d’obtenir un consensus pour la conservation de l’article est de fournir des [[Wikipédia:Citez vos sources|sources secondaires fiables et indépendantes]]. Si vous ne pouvez trouver de telles sources, c’est que l’article n’est probablement pas admissible. N’oubliez pas que les [[Wikipédia:Principes fondateurs|principes fondateurs]] de Wikipédia ne garantissent aucun droit à avoir un article sur Wikipédia. [[Utilisateur:Enrevseluj|Enrevseluj]] ([[Discussion utilisateur:Enrevseluj|d]]) 9 avril 2013 à 18:17 (CEST)

== L'article Embuscade de Skikda est proposé à la suppression ==
[[Image:Questionmark.png|70px|link=|Page proposée à la suppression|gauche]]
Bonjour,

L’article « '''{{Lien à supprimer|1=Embuscade de Skikda}}''' » est proposé à la suppression ({{cf.}} [[Wikipédia:Pages à supprimer]]). Après avoir pris connaissance des [[Wikipédia:Critères d'admissibilité des articles|critères généraux d’admissibilité des articles]] et des [[:Catégorie:Wikipédia:Admissibilité des articles|critères spécifiques]], vous pourrez [[Aide:Arguments à éviter lors d'une procédure de suppression|donner votre avis]] sur la page de discussion '''[[{{TALKPAGENAME:Embuscade de Skikda}}/Suppression]]'''.

Le meilleur moyen d’obtenir un consensus pour la conservation de l’article est de fournir des [[Wikipédia:Citez vos sources|sources secondaires fiables et indépendantes]]. Si vous ne pouvez trouver de telles sources, c’est que l’article n’est probablement pas admissible. N’oubliez pas que les [[Wikipédia:Principes fondateurs|principes fondateurs]] de Wikipédia ne garantissent aucun droit à avoir un article sur Wikipédia.

<nowiki />[[Utilisateur:La femme de menage|La femme de menage]] ([[Discussion utilisateur:La femme de menage|discuter]]) 8 avril 2015 à 21:28 (CEST)

== Avertissement suppression « [[Like a Monster]] » ==

Bonjour,

L’article « '''{{Lien à supprimer|1=Like a Monster}}''' » est proposé à la suppression ({{cf.}} [[Wikipédia:Pages à supprimer]]). En tant que participant à l'article ou projet associé, vous êtes invités à donner votre avis à l’aune de l’existence de [[Wikipédia:Citez vos sources|sources secondaires fiables et indépendantes]] et des [[Wikipédia:Critères d'admissibilité des articles|critères généraux]] et [[:Catégorie:Wikipédia:Admissibilité des articles|spécifiques]] d'admissibilité.

Les liens sur les éléments pertinents sont les bienvenus. N’oubliez pas que les [[Wikipédia:Principes fondateurs|principes fondateurs]] de Wikipédia ne garantissent aucun droit à avoir un article sur Wikipédia.

<center>[[{{TALKPAGENAME:Like a Monster}}/Suppression|{{bouton cliquable|Accéder au débat|couleur=blue}}]]</center>

[[Utilisateur:Chris a liege|Chris a liege]] ([[Discussion utilisateur:Chris a liege|discuter]]) 15 août 2017 à 01:00 (CEST)

== Avertissement suppression « [[:Peine perdue]] » ==


[[Image:Circle-icons-caution.svg|70px|link=Discussion:Peine perdue/Suppression|Page proposée à la suppression|gauche]]
Bonjour,

L’article « '''{{Lien à supprimer|1=Peine perdue}}''' » est proposé à la suppression ({{cf.}} [[Wikipédia:Pages à supprimer]]). En tant que participant à l'article ou projet associé, vous êtes invité à donner votre avis à l’aune de l’existence de [[Wikipédia:Citez vos sources|sources secondaires fiables et indépendantes]] et des [[Wikipédia:Critères d'admissibilité des articles|critères généraux]] et [[:Catégorie:Wikipédia:Admissibilité des articles|spécifiques]] d'admissibilité.

N’oubliez pas que les [[Wikipédia:Principes fondateurs|principes fondateurs]] de Wikipédia ne garantissent aucun droit à avoir un article sur Wikipédia.

<center>[[{{TALKPAGENAME:Peine perdue}}/Suppression|{{bouton cliquable|Accéder au débat|couleur=blue}}]]</center>

[[Utilisateur:Chris a liege|Chris a liege]] ([[Discussion utilisateur:Chris a liege|discuter]]) 16 mars 2021 à 18:29 (CET)

== Avertissement suppression « [[:Match de rugby à XV France - Irlande (2006)]] » ==


{{BMA début|bordure=information}}
[[Image:Circle-icons-caution.svg|70px|link=Discussion:Match de rugby à XV France - Irlande (2006)/Suppression|Page proposée à la suppression|droite]]
Bonjour,

L’article « '''{{Lien à supprimer|1=Match de rugby à XV France - Irlande (2006)}}''' » est proposé à la suppression ({{cf.}} [[Wikipédia:Pages à supprimer]]). Après avoir pris connaissance des [[Wikipédia:Notoriété|critères généraux d’admissibilité des articles]] et des [[Wikipédia:Liste des critères spécifiques de notoriété|critères spécifiques]], vous pourrez [[Aide:Arguments à éviter lors d'une procédure de suppression|donner votre avis]] sur la page de discussion '''[[{{TALKPAGENAME:Match de rugby à XV France - Irlande (2006)}}/Suppression]]'''.

Le meilleur moyen d’obtenir un consensus pour la conservation de l’article est de fournir des [[Wikipédia:Citez vos sources|sources secondaires fiables et indépendantes]]. Si vous ne pouvez trouver de telles sources, c’est que l’article n’est probablement pas admissible.

N’oubliez pas que les [[Wikipédia:Principes fondateurs|principes fondateurs]] de Wikipédia ne garantissent aucun droit à avoir un article sur Wikipédia.

<center>[[{{TALKPAGENAME:Match de rugby à XV France - Irlande (2006)}}/Suppression|{{bouton cliquable|Accéder au débat|couleur=blue}}]]</center>{{BMA fin}}

[[Utilisateur:Chris a liege|Chris a liege]] ([[Discussion utilisateur:Chris a liege|discuter]]) 14 décembre 2021 à 23:04 (CET)

== L'admissibilité de l'article « [[:Up For You & I]] » est débattue ==

{{BMA début|bordure=information}}
[[Image:Circle-icons-caution.svg|70px|link=Discussion:Up For You & I/Admissibilité|Page proposée au débat d'admissibilité|droite]]
Bonjour,

L’article « '''{{Lien à supprimer|1=Up For You & I}}''' » fait l'objet d'un débat d'admissibilité ({{cf.}} [[Wikipédia:Débat d'admissibilité]]). Après avoir pris connaissance des [[Wikipédia:Notoriété|critères généraux d’admissibilité des articles]] et des [[Wikipédia:Liste des critères spécifiques de notoriété|critères spécifiques]], vous pourrez [[Aide:Arguments à éviter lors d'une procédure de suppression|donner votre avis]] sur la page de discussion '''[[{{TALKPAGENAME:Up For You & I}}/Admissibilité]]'''.

Le meilleur moyen d’obtenir un consensus sur l'admissibilité de l’article est de fournir des [[Wikipédia:Citez vos sources|sources secondaires fiables et indépendantes]]. Si vous ne pouvez trouver de telles sources, c’est que l’article n’est probablement pas admissible.

N’oubliez pas que les [[Wikipédia:Principes fondateurs|principes fondateurs]] de Wikipédia ne garantissent aucun droit à avoir un article sur Wikipédia.

<div style="text-align: center;">[[{{TALKPAGENAME:Up For You & I}}/Admissibilité|{{bouton cliquable|Accéder au débat|couleur=blue}}]]</div>{{BMA fin}}

[[Utilisateur:Chris a liege|Chris a liege]] ([[Discussion utilisateur:Chris a liege|discuter]]) 26 mars 2022 à 23:27 (CET)

== L'admissibilité de l'article sur « Sentier du Littoral acadien » est débattue ==

<div class="bma" style="background-color:#F8F9FA; padding:1.2rem; margin-top:.5em; border:0px solid #EBEEF0; border-top-color:#3366CC; border-top-width:.4rem; border-radius:.20rem; box-shadow:0 0 0.1em #999999;">
[[Fichier:Circle-icons-caution.svg|70px|droite|Page proposée au débat d'admissibilité]]
Bonjour,

L’article « '''{{Lien à supprimer|1=Sentier du Littoral acadien}}''' » fait l'objet d'un débat d'admissibilité ({{cf.}} [[Wikipédia:Débat d'admissibilité]]). Il débouchera sur la conservation, la suppression ou la fusion de l'article. Après avoir pris connaissance des [[Wikipédia:Notoriété|critères généraux d’admissibilité des articles]] et des [[Wikipédia:Liste des critères spécifiques de notoriété|critères spécifiques]], vous pourrez [[Aide:Arguments à éviter lors d'un débat d'admissibilité|donner votre avis]] sur la page de discussion '''[[Discussion:Sentier du Littoral acadien/Admissibilité]]'''.

Le meilleur moyen d’obtenir un consensus pour la conservation de l’article est de fournir des [[Wikipédia:Citez vos sources|sources secondaires fiables et indépendantes]]. Si vous ne pouvez trouver de telles sources, c’est que l’article n’est probablement pas admissible. N’oubliez pas que les [[Wikipédia:Principes fondateurs|principes fondateurs]] de Wikipédia ne garantissent aucun droit à avoir un article sur Wikipédia.

<div style="text-align:center">[[Discussion:Sentier du Littoral acadien/Admissibilité|{{bouton cliquable|Accéder au débat|couleur=blue}}]]</div>
</div>[[Utilisateur:Shawn à Montréal|Shawn à Montréal]] ([[Discussion utilisateur:Shawn à Montréal|discuter]]) 27 avril 2023 à 22:00 (CEST)
)";

constexpr string_view DID_YOU_KNOW_MESSAGES = R"(
== Proposition d'anecdote pour la page d'accueil ==

Une proposition d'anecdote pour [[Wikipédia:Le saviez-vous ?/Anecdotes sur l'accueil|la section ''{{citation|Le Saviez-vous ?}}'']] de [[Wikipédia:Accueil_principal|la page d'accueil]], et basée sur l'article [[Bataille de Patay]], a été proposée sur [[WP:LSV|la page dédiée]].<br>
N'hésitez pas à apporter votre contribution sur la rédaction de l'anecdote, l'ajout de source dans l'article ou votre avis sur la proposition. '''La discussion est accessible [[Wikipédia:Le saviez-vous ?/Anecdotes proposées#ID_16946|ici]]'''.<br>
Une fois l'anecdote acceptée ou refusée pour publication, la discussion est ensuite archivée [[Discussion:Bataille de Patay/LSV_16946|là]].<br>
<small>(ceci est un message automatique du [[Wikipédia:Bot|bot]] {{u-|GhosterBot}} le 18 avril 2019 à 08:46, sans '''bot flag''')</small>

== Proposition d'anecdote pour la page d'accueil : [[Jean-Baptiste de Chateaubriand]] ==

Une anecdote fondée sur l'article [[Jean-Baptiste de Chateaubriand]] a été '''[[Wikipédia:Le saviez-vous ?/Anecdotes proposées#ID_24345|proposée ici]]''' (une fois acceptée ou refusée, elle est [[Discussion:Jean-Baptiste de Chateaubriand/LSV_24345|archivée là]]). N'hésitez pas à apporter votre avis sur sa pertinence ou sa formulation et à ajouter des sources dans l'article.<br />
''Les anecdotes sont destinées à la section [[Wikipédia:Le saviez-vous ?/Anecdotes sur l'accueil|{{citation|Le Saviez-vous ?}}]] de [[Wikipédia:Accueil_principal|la page d'accueil]] de Wikipédia. Elles doivent d'abord être proposées sur [[WP:LSV|la page dédiée]].''<br />{{#if: Jean-Baptiste de Chateaubriand|<small>Pour placer ces notifications sur une sous-page spécifique, consultez [[Utilisateur:GhosterBot/Explication_notification_projet|cette documentation]].</small><br />}}
<small>(ceci est un message automatique du [[Wikipédia:Bot|bot]] {{u-|GhosterBot}} le 30 août 2025 à 15:47, sans '''bot flag''')</small>
)";

vector<string_view> splitSections(string_view text) {
  vector<string_view> sections;
  while (!text.empty()) {
    size_t nextTitle = text.find("\n=");
    size_t endOfSection = nextTitle == string_view::npos ? text.size() : nextTitle + 1;
    if (text.starts_with("=")) {
      sections.push_back(text.substr(0, endOfSection));
    }
    text.remove_prefix(endOfSection);
  }
  return sections;
}

class DetectStandardMessageTest : public cbl::Test {
private:
  CBL_TEST_CASE(AfD) {
    for (string_view section : splitSections(AFD_MESSAGES)) {
      CBL_ASSERT_EQ(detectStandardMessage(section).type, StandardMessage::AFD) << section;
      CBL_ASSERT_EQ(detectStandardMessage(cbl::trim(section)).type, StandardMessage::AFD) << section;
    }
  }
  CBL_TEST_CASE(AfDWithResponse) {
    string_view section = splitSections(AFD_MESSAGES).front();
    CBL_ASSERT(section.ends_with("\n"));
    vector<string> afdMessagesWithResponse = {
        cbl::concat(section, ":Test. [[Utilisateur:X|X]]\n"),
        cbl::concat(section, "Test. [[Utilisateur:X|X]]\n"),
        cbl::concat(section, ":Test.\n"),
    };
    for (const string& message : afdMessagesWithResponse) {
      CBL_ASSERT_EQ(detectStandardMessage(message).type, StandardMessage::NONE) << message;
    }
  }
  CBL_TEST_CASE(DidYouKnow) {
    for (string_view section : splitSections(DID_YOU_KNOW_MESSAGES)) {
      CBL_ASSERT_EQ(detectStandardMessage(section).type, StandardMessage::DID_YOU_KNOW) << section;
      CBL_ASSERT_EQ(detectStandardMessage(cbl::trim(section)).type, StandardMessage::DID_YOU_KNOW) << section;
    }
  }
  CBL_TEST_CASE(CustomMessage) {
    string_view message =
        "== Section ==\n"
        "Test. [[Utilisateur:X|X]] ([[Discussion utilisateur:X|discuter]]) 1 janvier 2000 à 00:00 (CET)\n";
    CBL_ASSERT_EQ(detectStandardMessage(message).type, StandardMessage::NONE) << message;
  }
};

}  // namespace wikiutil

int main() {
  wikiutil::DetectStandardMessageTest().run();
  return 0;
}
