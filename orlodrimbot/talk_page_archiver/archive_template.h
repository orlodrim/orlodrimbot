// Library to extract the bot configuration template ({{Archivage par bot}}) and parse its parameters.
#ifndef TALK_PAGE_ARCHIVER_ARCHIVE_TEMPLATE_H
#define TALK_PAGE_ARCHIVER_ARCHIVE_TEMPLATE_H

#include <string>
#include <string_view>
#include <vector>
#include "cbl/error.h"
#include "mwclient/parser.h"
#include "mwclient/wiki.h"
#include "algorithm.h"

namespace talk_page_archiver {

constexpr std::string_view ARCHIVE_TEMPLATE_NAME = "Archivage par bot";
constexpr int ARCHIVE_PARAM_NOT_SET = -1;
constexpr int DEF_MIN_THREADS_LEFT = 5;
constexpr int DEF_MIN_THREADS_TO_ARCHIVE = 2;

// Returns the first {{Archivage par bot}} template on the page.
wikicode::Template* findArchiveTemplate(mwc::Wiki& wiki, wikicode::List& parsedCode);
// Tests whether a page contains {{Archivage par bot}}.
bool containsArchiveTemplate(mwc::Wiki& wiki, std::string_view code);

class ParamsInitializationError : public cbl::Error {
public:
  using Error::Error;
};

// Stores the parameters of {{Archivage par bot}}.
class ArchiveParams {
public:
  ArchiveParams() = default;
  // Throws: ParamsInitializationError.
  ArchiveParams(mwc::Wiki& wiki, const Algorithms& algorithms, const std::string& title, wikicode::List& parsedCode);

  const std::string& archive() const { return m_archive; }
  const std::string& rawArchive() const { return m_rawArchive; }
  std::vector<ParameterizedAlgorithm> algorithms() const { return m_algorithms; }
  int counter() const { return m_counter; }
  int maxarchivesize() const { return m_maxarchivesize; }
  int minthreadsleft() const { return m_minthreadsleft; }
  int minthreadstoarchive() const { return m_minthreadstoarchive; }
  const std::string& archiveheader() const { return m_archiveheader; }
  const std::string& key() const { return m_key; }

private:
  std::string m_archive;
  std::string m_rawArchive;
  std::vector<ParameterizedAlgorithm> m_algorithms;
  int m_counter = ARCHIVE_PARAM_NOT_SET;
  int m_maxarchivesize = ARCHIVE_PARAM_NOT_SET;
  int m_minthreadsleft = ARCHIVE_PARAM_NOT_SET;
  int m_minthreadstoarchive = ARCHIVE_PARAM_NOT_SET;
  std::string m_archiveheader;
  std::string m_key;
};

}  // namespace talk_page_archiver

#endif
