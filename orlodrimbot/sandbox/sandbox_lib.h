#ifndef SANDBOX_LIB_H
#define SANDBOX_LIB_H

#include <string>
#include <vector>
#include "mwclient/wiki.h"

struct SandboxPage {
  // Title of the sandbox page.
  std::string page;
  // Title of the template used to reset the sandbox (including the namespace).
  std::string templatePage;
  // Do not clean up the sandbox if it has been modified in the last minSeconds seconds.
  int minSeconds;
};

class SandboxCleaner {
public:
  SandboxCleaner(mwc::Wiki* wiki, std::vector<SandboxPage> sandboxes);
  ~SandboxCleaner();
  // Cleans up all sandboxes.
  // As an optimization, a sandbox page is written only if either the sandbox or the template were recently modified.
  // This can be bypassed with force=true. On the other hand, force does not bypass minSeconds.
  void run(bool force, bool dryRun);

private:
  mwc::Wiki* m_wiki;
  std::vector<SandboxPage> m_sandboxes;
};

#endif
