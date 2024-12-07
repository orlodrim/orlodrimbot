#include "init_wiki.h"
#include <climits>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>
#include "cbl/args_parser.h"
#include "cbl/file.h"
#include "cbl/log.h"
#include "cbl/path.h"
#include "cbl/string.h"
#include "mwclient/wiki.h"

using std::string;
using std::string_view;

namespace mwc {

constexpr const char* MWCLIENT_ID_DIR_VARIABLE = "LIBMWCLIENT_ID_DIR";

static const char* getLoginFileFromConfig(StandardWikiConfig config) {
  switch (config) {
    case FRENCH_WIKIPEDIA_BOT:
      return "idwp.txt";
    case FRENCH_WIKIPEDIA_SYSOP:
      return "idadmin.txt";
    case COMMONS_WIKI_ANONYMOUS:
      return "idcommons.txt";
  }
  throw std::invalid_argument("Invalid config passed to getLoginFileFromConfig");
};

static void getFlagsFromConfig(StandardWikiConfig config, const char*& loginFileFlag, const char*& sessionFileFlag) {
  switch (config) {
    case FRENCH_WIKIPEDIA_BOT:
      loginFileFlag = "--loginfile";
      sessionFileFlag = "--sessionfile";
      break;
    case FRENCH_WIKIPEDIA_SYSOP:
      loginFileFlag = "--sysoploginfile";
      sessionFileFlag = "--sysopsessionfile";
      break;
    case COMMONS_WIKI_ANONYMOUS:
      loginFileFlag = "--commonsloginfile";
      sessionFileFlag = "--commonssessionfile";
      break;
    default:
      throw std::invalid_argument("Invalid config passed to getFlagsFromConfig");
  }
};

WikiFlags::WikiFlags(StandardWikiConfig config) : m_config(config) {
  m_loginFile = getLoginFileFromConfig(config);
}

void WikiFlags::declareFlags(cbl::ArgsParser& parser) {
  const char* loginFileFlag = nullptr;
  const char* sessionFileFlag = nullptr;
  getFlagsFromConfig(m_config, loginFileFlag, sessionFileFlag);
  parser.addArgs(loginFileFlag, &m_loginFile, sessionFileFlag, &m_sessionFile);
}

string WikiFlags::sessionFile() const {
  string sessionFile = m_sessionFile;
  if (sessionFile.empty()) {
    size_t dotPosition = m_loginFile.rfind('.');
    CBL_ASSERT(dotPosition != string::npos);
    sessionFile = m_loginFile.substr(0, dotPosition) + ".session";
  }
  return sessionFile;
}

static void resolveLoginAndSessionFiles(string& loginFile, string& sessionFile) {
  if (loginFile.find('/') != string::npos) {
    return;  // Resolve only file names without a path.
  }
  const char* const defaultIdDir = getenv(MWCLIENT_ID_DIR_VARIABLE);
  if (defaultIdDir == nullptr || *defaultIdDir == '\0') {
    return;  // Environment variable not set.
  }
  string loginFileInIdDir = cbl::joinPaths(defaultIdDir, loginFile);
  if (!cbl::fileExists(loginFileInIdDir)) {
    return;  // Not found in the default directory, so leave a chance to find it in the current directory instead.
  }
  loginFile = loginFileInIdDir;
  if (sessionFile.find('/') == string::npos) {
    sessionFile = cbl::joinPaths(defaultIdDir, sessionFile);
  }
}

void initWikiFromFlags(const WikiFlags& flags, Wiki& wiki) {
  string resolvedLoginFile = flags.loginFile();
  string resolvedSessionFile = flags.sessionFile();
  resolveLoginAndSessionFiles(resolvedLoginFile, resolvedSessionFile);
  if (resolvedLoginFile == resolvedSessionFile) {
    throw std::invalid_argument("The id file '" + resolvedLoginFile + "' must not be the same as the session file");
  }

  LoginParams loginParams;
  string loginFileContent = cbl::readFile(resolvedLoginFile);
  for (string_view line : cbl::splitLines(loginFileContent)) {
    size_t equalPosition = line.find('=');
    if (equalPosition == string_view::npos) continue;
    string_view param = line.substr(0, equalPosition);
    string_view value = line.substr(equalPosition + 1);

    if (param == "url") {
      loginParams.url = value;
    } else if (param == "userName") {
      loginParams.userName = value;
    } else if (param == "password") {
      loginParams.password = value;
    } else if (param == "clientLogin") {
      int parsedValue = cbl::parseIntInRange(cbl::legacyStringConv(value), 0, 1, -1) != 0;
      if (parsedValue == -1) {
        throw LoginConfigParseError("Cannot parse 'clientLogin' param in '" + resolvedLoginFile + "'");
      }
      loginParams.clientLogin = parsedValue;
    } else if (param == "userAgent") {
      loginParams.userAgent = value;
    } else if (param == "delayBeforeRequests") {
      loginParams.delayBeforeRequests = cbl::parseIntInRange(cbl::legacyStringConv(value), 0, INT_MAX, -1);
      if (loginParams.delayBeforeRequests == -1) {
        throw LoginConfigParseError("Cannot parse 'delayBeforeRequests' param in '" + resolvedLoginFile + "'");
      }
    } else if (param == "delayBetweenEdits") {
      loginParams.delayBetweenEdits = cbl::parseIntInRange(cbl::legacyStringConv(value), 0, INT_MAX, -1);
      if (loginParams.delayBetweenEdits == -1) {
        throw LoginConfigParseError("Cannot parse 'delayBetweenEdits' param in '" + resolvedLoginFile + "'");
      }
    } else if (param == "maxLag") {
      loginParams.maxLag = cbl::parseIntInRange(cbl::legacyStringConv(value), 0, INT_MAX, -1);
      if (loginParams.maxLag == -1) {
        throw LoginConfigParseError("Cannot parse 'maxLag' param in '" + resolvedLoginFile + "'");
      }
    } else {
      throw LoginConfigParseError("Invalid parameter '" + string(param) + "' in '" + resolvedLoginFile + "'");
    }
  }

  wiki.logIn(loginParams, resolvedSessionFile);
}

void parseArgsAndInitWikiFromFlags(StandardWikiConfig config, int argc, const char* const* argv, Wiki& wiki) {
  WikiFlags wikiFlags(config);
  cbl::parseArgs(argc, argv, &wikiFlags);
  initWikiFromFlags(wikiFlags, wiki);
}

}  // namespace mwc
