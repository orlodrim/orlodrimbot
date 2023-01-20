#ifndef TALK_PAGE_ARCHIVER_ALGORITHM_H
#define TALK_PAGE_ARCHIVER_ALGORITHM_H

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include "mwclient/wiki.h"
#include "orlodrimbot/wikiutil/date_parser.h"

namespace talk_page_archiver {

enum class ThreadAction {
  KEEP,
  ARCHIVE,
  ERASE,
};

// Base class for algorithms deciding whether a thread should be archived based on its content.
// Algorithms should not check the age of the thread. They should make a decision based on other criteria (e.g. the
// presence of a {{done}} template), assuming that the thread is old enough to be archived/erased.
// In fact, computing the age of the thread is a sometimes a costly operation (when there is no signature), so this is
// done by the archiver after running all algorithms, only if an algorithm decides that the thread should not be kept.
class Algorithm {
public:
  explicit Algorithm(std::string_view name) : m_name(name) {}
  Algorithm(const Algorithm&) = delete;
  virtual ~Algorithm() = default;
  Algorithm& operator=(const Algorithm&) = delete;

  // Name of this algorithm as it appears in the "algo" parameter of {{Archivage par bot}}. Should be in lower case.
  const std::string& name() const { return m_name; }
  // Rank of this algorithm in an Algorithms collection. The algorithms should be applied by increasing rank. Typically,
  // the most specific algorithms come first.
  int rank() const { return m_rank; }
  void setRank(int value) { m_rank = value; }

  struct RunResult {
    // What to do on the thread.
    ThreadAction action;
    // If set, the archiver will assume that the thread was last modified on the specified date, unless there is an even
    // more recent signature in the content.
    // This can be used by algorithms that run on pages where threads contain dates with a non-standard format
    // (i.e. they do not look like dates in wiki signatures).
    wikiutil::SignatureDate forcedDate;
  };
  // Function to overload in subclasses that decides which action to perform on a thread based on its content.
  virtual RunResult run(const mwc::Wiki& wiki, std::string_view threadContent) const = 0;

private:
  std::string m_name;
  int m_rank = 0;
};

// Algorithm that unconditionally archives old sections.
class ArchiveOldSectionsAlgorithm : public Algorithm {
public:
  ArchiveOldSectionsAlgorithm() : Algorithm("old") {}
  RunResult run(const mwc::Wiki& wiki, std::string_view threadContent) const override {
    return {.action = ThreadAction::ARCHIVE};
  }
};

// Algorithm that unconditionally erases old sections.
class EraseOldSectionsAlgorithm : public Algorithm {
public:
  EraseOldSectionsAlgorithm() : Algorithm("eraseold") {}
  RunResult run(const mwc::Wiki& wiki, std::string_view threadContent) const override {
    return {.action = ThreadAction::ERASE};
  }
};

// Collection of algorithms that can be queried by name.
// Assigns increasing ranks to algorithms as they are added to the collection.
class Algorithms {
public:
  void add(std::unique_ptr<Algorithm> algorithm);
  // Throws: std::out_of_range.
  const Algorithm& find(std::string_view name) const;

private:
  std::unordered_map<std::string_view, std::unique_ptr<Algorithm>> m_algorithmsByName;
};

// An algorithm and the maximum thread age specified for it.
// The "algo" parameter of {{Archivage par bot}} is a comma-separated list of values that are parsed as
// ParameterizedAlgorithm.
struct ParameterizedAlgorithm {
  const Algorithm* algorithm = nullptr;
  int maxAgeInDays = -1;
};

}  // namespace talk_page_archiver

#endif
