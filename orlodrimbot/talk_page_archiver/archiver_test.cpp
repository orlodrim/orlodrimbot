#include "archiver.h"
#include <fstream>
#include <memory>
#include <string>
#include "cbl/date.h"
#include "cbl/file.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "cbl/tempfile.h"
#include "cbl/unittest.h"
#include "mwclient/mock_wiki.h"
#include "mwclient/parser.h"
#include "mwclient/wiki.h"

using cbl::Date;
using cbl::DateDiff;
using mwc::MockWiki;
using std::string;
using std::unique_ptr;

namespace talk_page_archiver {
namespace {

class TestDataReader {
public:
  TestDataReader(const string& testFile) : m_testFileStream(testFile), m_eof(false) {
    CBL_ASSERT(m_testFileStream) << "Cannot open '" << testFile << "'";
    readLine();
  }
  bool readCommand(string& command) {
    while (!m_eof && m_line.starts_with("#")) {
      readLine();
    }
    if (m_eof) return false;
    CBL_ASSERT(!m_line.starts_with(" "));
    command = m_line;
    readLine();
    return true;
  }
  void readCommandCode(string& code) {
    code.clear();
    while (!m_eof && (m_line.empty() || m_line.starts_with(" "))) {
      if (!m_line.empty()) {
        code.append(m_line, 1, m_line.size() - 1);
      }
      code += '\n';
      readLine();
    }
    if (code.ends_with("\n")) {
      code.resize(code.size() - 1);
    }
  }

private:
  void readLine() {
    if (!getline(m_testFileStream, m_line)) {
      m_eof = true;
      m_line.clear();
    }
  }

  std::ifstream m_testFileStream;
  string m_line;
  bool m_eof;
};

class ArchiverTest : public cbl::Test {
public:
  void setUp() override {
    m_wiki.resetDatabase();
    m_archiver = std::make_unique<Archiver>(&m_wiki, m_tempDir.path() + "/", "", /* dryRun = */ false);
    m_stableRevidsPath = m_tempDir.path() + "/stable_revids.txt";
  }
  void tearDown() override { cbl::removeFile(m_stableRevidsPath, /* mustExist = */ false); }

private:
  void processTestFile(const string& path) {
    TestDataReader testDataReader(path);
    string command;
    string code;
    while (testDataReader.readCommand(command)) {
      int spacePosition = command.find(' ');
      if (spacePosition == -1) {
        spacePosition = command.size();
        command += ' ';
      }
      const string keyword(command, 0, spacePosition);
      const string param(command, spacePosition + 1, command.size() - spacePosition - 1);
      if (keyword == "DATE") {
        const Date d = Date::fromISO8601(param);
        CBL_ASSERT(!d.isNull());
        Date::setFrozenValueOfNow(d);
      } else if (keyword == "WRITE") {
        CBL_ASSERT(!param.empty());
        testDataReader.readCommandCode(code);
        m_wiki.setPageContent(param, code);
      } else if (keyword == "ARCHIVE") {
        CBL_ASSERT(!param.empty());
        m_archiver->archivePages({param});
      } else if (keyword == "CHECK") {
        CBL_ASSERT(!param.empty());
        testDataReader.readCommandCode(code);
        CBL_ASSERT_EQ(m_wiki.readPageContent(param), code);
      } else if (keyword == "CHECK_COMMENT") {
        CBL_ASSERT(!param.empty());
        testDataReader.readCommandCode(code);
        mwc::Revision revision = m_wiki.readPage(param, mwc::RP_COMMENT);
        CBL_ASSERT_EQ(revision.comment, code);
      } else if (keyword == "RESET") {
        m_wiki.resetDatabase();
        Date::setFrozenValueOfNow(Date::fromISO8601("2000-01-01T00:00:00Z"));
      } else {
        CBL_FATAL << "Invalid line in test file '" << path << "' (bad command): " << command;
      }
    }
  }
  CBL_TEST_CASE(FromTestFile) {
    processTestFile("testdata/archiver_tests.txt");
  }
  CBL_TEST_CASE(StableRevids) {
    Date::setFrozenValueOfNow(Date::fromISO8601("2000-02-01T00:00:00Z"));
    // revid = 1
    m_wiki.setPageContent("Discussion utilisateur:User1",
                          "{{Archivage par bot|algo=old(7d)|minthreadstoarchive=1|minthreadsleft=1|counter=}}\n"
                          "== Section 1 ==\n"
                          "[[Utilisateur:Exemple]] 30 janvier 2000 à 00:00 (CET)");
    // revid = 2
    m_wiki.setPageContent("Discussion utilisateur:User2",
                          "{{Archivage par bot|algo=old(7d)|minthreadstoarchive=1|minthreadsleft=1|counter=}}\n"
                          "== Section 1 ==\n"
                          "[[Utilisateur:Exemple]] 30 janvier 2000 à 00:00 (CET)\n"
                          "== Section 2 ==\n"
                          "[[Utilisateur:Exemple]] 30 janvier 2000 à 00:00 (CET)");
    // Detect that 'Discussion utilisateur:User1' is stable.
    m_archiver->archiveAll();
    CBL_ASSERT_EQ(cbl::readFile(m_stableRevidsPath), "1\n");
    // Skip 'Discussion utilisateur:User1', keep it in the list of stable revids.
    m_archiver->archiveAll();
    CBL_ASSERT_EQ(cbl::readFile(m_stableRevidsPath), "1\n");
    // Archive 'Discussion utilisateur:User2'.
    Date::advanceFrozenClock(DateDiff::fromDays(8));
    m_archiver->archiveAll();  // Writes revid 3 (archive) and revid 4 (archived page).
    CBL_ASSERT_EQ(cbl::readFile(m_stableRevidsPath), "1\n");
    // Detect that 'Discussion utilisateur:User2' is also stable.
    m_archiver->archiveAll();
    CBL_ASSERT_EQ(cbl::readFile(m_stableRevidsPath), "1\n4\n");
    // After a change of 'Discussion utilisateur:User1', remove its old revid from the list of stable pages.
    m_wiki.setPageContent("Discussion utilisateur:User1",
                          "{{Archivage par bot|algo=old(7d)|minthreadstoarchive=1|minthreadsleft=1|counter=}}\n"
                          "== Section 1 ==\n"
                          "[[Utilisateur:Exemple]] 30 janvier 2000 à 00:00 (CET) [edited]");
    m_archiver->archiveAll();
    CBL_ASSERT_EQ(cbl::readFile(m_stableRevidsPath), "4\n5\n");
  }

  cbl::TempDir m_tempDir;
  string m_stableRevidsPath;
  mwc::MockWiki m_wiki;
  unique_ptr<Archiver> m_archiver;
};

void callExtractTrackingTemplate(const string& code, bool expectedResult, const string& expectedTrackingTemplate,
                                 const string& expectedCodeInTemplate, const string& expectedHeader,
                                 const string& expectedFooter) {
  MockWiki wiki;
  unique_ptr<wikicode::Template> trackingTemplate;
  string codeInTemplate, header, footer;
  const bool result = extractTrackingTemplate(&wiki, code, trackingTemplate, codeInTemplate, header, footer);
  CBL_ASSERT_EQ(result, expectedResult) << code;
  if (expectedResult) {
    CBL_ASSERT_EQ(trackingTemplate->toString(), expectedTrackingTemplate) << code;
    CBL_ASSERT_EQ(codeInTemplate, expectedCodeInTemplate) << code;
    CBL_ASSERT_EQ(header, expectedHeader) << code;
    CBL_ASSERT_EQ(footer, expectedFooter) << code;
  }
}

class ExtractTrackingTemplateTest : public cbl::Test {
  CBL_TEST_CASE(extractTrackingTemplate) {
    callExtractTrackingTemplate(
        "== Section 1 ==\n"
        "Text\n"
        "== Section 2 ==\n"
        "Text",
        false, "", "", "", "");
    callExtractTrackingTemplate(
        "{{Utilisateur:OrlodrimBot/Suivi catégorie|format sections = -}}\n"
        "*[[Article]]\n"
        "{{Utilisateur:OrlodrimBot/Suivi catégorie/fin}}",
        false, "", "", "", "");

    // Standard case.
    callExtractTrackingTemplate(
        "Header.\n"
        "{{Utilisateur:OrlodrimBot/Suivi catégorie|format sections = == %(mois) %(année) ==}}\n"
        "== Section 1 ==\n"
        "*[[Article]]\n"
        "{{Utilisateur:OrlodrimBot/Suivi catégorie/fin}}\n"
        "Footer.",
        true, "{{Utilisateur:OrlodrimBot/Suivi catégorie|format sections = == %(mois) %(année) ==}}",
        "\n== Section 1 ==\n*[[Article]]\n", "Header.\n", "{{Utilisateur:OrlodrimBot/Suivi catégorie/fin}}\nFooter.");

    // Takes the first tracking template.
    // Template titles are normalized.
    // Extraction also works if it is nested inside another template.
    callExtractTrackingTemplate(
        "{{Boîte déroulante|contenu=\n"
        "{{user:OrlodrimBot/Suivi catégorie|format sections = == %(mois) %(année) ==|A}}\n"
        "x\n"
        "{{Utilisateur:OrlodrimBot/Suivi catégorie/fin}}\n"
        "{{Utilisateur:OrlodrimBot/Suivi catégorie|format sections = == %(mois) %(année) ==|B}}\n"
        "y\n"
        "{{Utilisateur:OrlodrimBot/Suivi catégorie/fin}}\n"
        "}}",
        true, "{{user:OrlodrimBot/Suivi catégorie|format sections = == %(mois) %(année) ==|A}}", "\nx\n",
        "{{Boîte déroulante|contenu=\n",
        "{{Utilisateur:OrlodrimBot/Suivi catégorie/fin}}\n"
        "{{Utilisateur:OrlodrimBot/Suivi catégorie|format sections = == %(mois) %(année) ==|B}}\n"
        "y\n"
        "{{Utilisateur:OrlodrimBot/Suivi catégorie/fin}}\n"
        "}}");

    // Returns an empty code for non-terminated template.
    callExtractTrackingTemplate(
        "{{Boîte déroulante|contenu=\n"
        "{{Utilisateur:OrlodrimBot/Suivi catégorie|format sections = == %(mois) %(année) ==|A}}\n"
        "x\n"
        // Missing {{Utilisateur:OrlodrimBot/Suivi catégorie/fin}}.
        "{{Utilisateur:OrlodrimBot/Suivi catégorie|format sections = == %(mois) %(année) ==|B}}\n"
        "y\n"
        "{{Utilisateur:OrlodrimBot/Suivi catégorie/fin}}\n"
        "}}",
        true, "{{Utilisateur:OrlodrimBot/Suivi catégorie|format sections = == %(mois) %(année) ==|A}}", "",
        "{{Boîte déroulante|contenu=\n",
        "\n"
        "x\n"
        "{{Utilisateur:OrlodrimBot/Suivi catégorie|format sections = == %(mois) %(année) ==|B}}\n"
        "y\n"
        "{{Utilisateur:OrlodrimBot/Suivi catégorie/fin}}\n"
        "}}");
  }
};

}  // namespace
}  // namespace talk_page_archiver

int main() {
  talk_page_archiver::ArchiverTest().run();
  talk_page_archiver::ExtractTrackingTemplateTest().run();
  return 0;
}
