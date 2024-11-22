#ifndef MWC_MOCK_WIKI_H
#define MWC_MOCK_WIKI_H

#include <string>
#include <unordered_map>
#include <vector>
#include "cbl/json.h"
#include "wiki.h"
#include "wiki_defs.h"

namespace mwc {

// Mock for the Wiki class to build tests that run locally.
// A new MockWiki object has the same namespaces as fr.wikipedia.org and contains no pages.
// Basic read/write operations on pages are supported. Most operations that require parsing wikitext such as enumerating
// categories are not implemented and require overloading the class.
class MockWiki : public Wiki {
public:
  using Wiki::readPage;

  MockWiki();

  Revision readPage(const std::string& title, int properties) override;
  void readPages(int properties, std::vector<Revision>& revisions, int flags = 0) override;
  Revision readRevision(revid_t revid, int properties) override;
  void readRevisions(int properties, std::vector<Revision>& revisions) override;

  std::vector<Revision> getHistory(const HistoryParams& params) override;
  std::unordered_map<std::string, std::vector<PageProtection>> getPagesProtections(
      const std::vector<std::string>& titles) override;
  std::vector<std::string> getTransclusions(const std::string& title) override;
  std::vector<std::string> getAllPages(const AllPagesParams& params) override;

  void setPageProtection(const std::string& title, const std::vector<PageProtection>& protections,
                         const std::string& reason = std::string()) override;
  void deletePage(const std::string& title, const std::string& reason = std::string()) override;

  virtual void resetDatabase();
  // Like writePage, but does not check for edit conflicts.
  void setPageContent(const std::string& title, const std::string& content);
  void assertPageLastCommentEquals(const std::string& title, const std::string& expectedComment);
  int getNumPages() const { return m_pages.size(); }
  void hideRevision(const std::string& title, int revIndex);
  void setVerboseWrite(bool verboseWrite) { m_verboseWrite = verboseWrite; }

  // If apiRequest gets called, it means that the user has tried some operation that the mock does not support.
  // In that case, prints the request and fails.
  json::Value apiRequest(const std::string& request, const std::string& data, bool canRetry) override;
  // Returns immediately instead of sleeping.
  void sleep(int seconds) override;

protected:
  void writePageInternal(const std::string& title, const std::string& content, const WriteToken& writeToken,
                         const std::string& summary = std::string(), int flags = 0) override;

private:
  struct Page {
    std::vector<revid_t> revisions;
    std::vector<PageProtection> protections;
  };
  const Page& getPage(const std::string& title) const;
  Page& getMutablePage(const std::string& title);

  std::unordered_map<std::string, Page> m_pages;
  std::unordered_map<revid_t, Revision> m_revisions;
  revid_t m_nextRevid;
  bool m_verboseWrite;
};

}  // namespace mwc

#endif
