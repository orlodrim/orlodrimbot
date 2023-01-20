#include "sandbox_lib.h"
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "cbl/date.h"
#include "cbl/log.h"
#include "mwclient/wiki.h"

using cbl::Date;
using cbl::DateDiff;
using std::string;
using std::unordered_map;
using std::vector;

SandboxCleaner::SandboxCleaner(mwc::Wiki* wiki, vector<SandboxPage> sandboxes)
    : m_wiki(wiki), m_sandboxes(std::move(sandboxes)) {}

SandboxCleaner::~SandboxCleaner() {}

void SandboxCleaner::run(bool force, bool dryRun) {
  constexpr const char* EDIT_SUMMARY = "Ratissage automatique du bac à sable";
  // Since the cleanup is supposed to happen every 30 minutes, there is no need to clean up if the last edit is older
  // than 30 minutes, unless the template itself changed. In practice, we take some margin because some edits can fail.
  // Also, the cleaner runs in forced mode once a day.
  constexpr int MAX_AGE_FOR_CLEANUP = 3600 * 2;

  // Gets all the data to compute which sandboxes need cleanup in a single query.
  vector<mwc::Revision> revisions;
  revisions.reserve(m_sandboxes.size() * 2);
  for (const SandboxPage& sandbox : m_sandboxes) {
    revisions.emplace_back();
    revisions.back().title = sandbox.page;
    revisions.emplace_back();
    revisions.back().title = sandbox.templatePage;
  }
  m_wiki->readPages(mwc::RP_TIMESTAMP, revisions);
  unordered_map<string, Date> pageTimestamps;
  for (const mwc::Revision& revision : revisions) {
    pageTimestamps[revision.title] = revision.timestamp;
  }

  // Cleanup
  Date now = Date::now();
  for (const SandboxPage& sandbox : m_sandboxes) {
    Date pageTimestamp = pageTimestamps.at(sandbox.page);
    Date templateTimestamp = pageTimestamps.at(sandbox.templatePage);
    Date minTimestamp = now - DateDiff::fromSeconds(sandbox.minSeconds + MAX_AGE_FOR_CLEANUP);
    Date maxTimestampOfPage = now - DateDiff::fromSeconds(sandbox.minSeconds);

    if (!force && pageTimestamp < minTimestamp && templateTimestamp < minTimestamp) {
      CBL_INFO << "Skipping cleanup of '" << sandbox.page << "' because it was not edited after " << minTimestamp;
    } else if (sandbox.minSeconds != 0 && pageTimestamp > maxTimestampOfPage) {
      CBL_INFO << "Skipping cleanup of '" << sandbox.page << "' because it was edited less than " << sandbox.minSeconds
               << " seconds ago";
    } else {
      string content = "{{subst:" + sandbox.templatePage + "}}";
      CBL_INFO << (dryRun ? "[DRY RUN] " : "") << "Cleaning up '" << sandbox.page << "': " << content;
      if (!dryRun) {
        try {
          mwc::WriteToken writeToken =
              mwc::WriteToken::newForEdit(sandbox.page, pageTimestamp, /* needsNoBotsBypass = */ false);
          m_wiki->writePage(sandbox.page, content, writeToken, EDIT_SUMMARY, mwc::EDIT_MINOR);
        } catch (const mwc::WikiError& error) {
          CBL_ERROR << error.what();
        }
      }
    }
  }
}
