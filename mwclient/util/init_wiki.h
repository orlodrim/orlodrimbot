// Functions to initialize a Wiki object from a configuration file provided on the command line or derived from
// environment variables.
//
// Configuration files contain a list of lines of the form "param=value", with the same parameters as the LoginParams
// struct.
// Example:
//   url=https://en.wikipedia.org/w
//   userName=UserNameOnWiki
//   password=123456
//   userAgent=UserNameOnWiki (http://en.wikipedia.org/wiki/User:UserNameOnWiki)
//   delayBeforeRequests=1
//
// Typical use:
//   mwc::WikiFlags wikiFlags(mwc::FRENCH_WIKIPEDIA_BOT);
//   cbl::parseArgs(argc, argv, &wikiFlags, /* more command line flags if needed */);
//   mwc::initWikiFromFlags(wikiFlags, wiki);
//
// Each predefined configuration uses different flag names, so that the same program can use several predefined
// configurations at the same time.
// For instance, with FRENCH_WIKIPEDIA_BOT, the path of the configuration file is read from --loginfile and the session
// is saved to the path specified by --sessionfile. If --loginfile is omitted, it defaults to
// $LIBMWCLIENT_ID_DIR/idwp.txt. If --sessionfile is omitted, it is the login file with the extension replaced by
// ".session".
#ifndef MWC_UTIL_INIT_WIKI_H
#define MWC_UTIL_INIT_WIKI_H

#include <string>
#include "cbl/args_parser.h"
#include "mwclient/wiki.h"

namespace mwc {

class LoginConfigParseError : public WikiError {
public:
  using WikiError::WikiError;
};

enum StandardWikiConfig {
  FRENCH_WIKIPEDIA_BOT,
  FRENCH_WIKIPEDIA_SYSOP,
  COMMONS_WIKI_ANONYMOUS,
};

class WikiFlags : public cbl::FlagsConsumer {
public:
  explicit WikiFlags(StandardWikiConfig config);
  void declareFlags(cbl::ArgsParser& parser) override;

  const std::string& loginFile() const { return m_loginFile; }
  std::string sessionFile() const;

private:
  StandardWikiConfig m_config;
  std::string m_loginFile;
  std::string m_sessionFile;
};

// Initializes wiki with the configuration file pointed by flags.loginFile(). If possible, restores the session from
// flags.sessionFile(). The example in the header shows how to initialize the WikiFlag object.
void initWikiFromFlags(const WikiFlags& flags, Wiki& wiki);

// Directly initializes wiki from command line arguments. Can be used if the program does not have any other command
// line argument.
void parseArgsAndInitWikiFromFlags(StandardWikiConfig config, int argc, const char* const* argv, Wiki& wiki);

}  // namespace mwc

#endif
