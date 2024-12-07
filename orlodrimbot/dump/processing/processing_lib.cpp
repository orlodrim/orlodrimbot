#include "processing_lib.h"
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "mwclient/util/xml_dump.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/dump/processing/processes/modules.h"
#include "orlodrimbot/dump/processing/processes/process.h"
#include "orlodrimbot/dump/processing/processes/templates.h"
#include "orlodrimbot/dump/processing/processes/titles.h"

using std::string;
using std::unique_ptr;
using std::vector;

namespace dump_processing {

struct ProcessDef {
  const char* name;
  std::function<Process*()> factory;
};

static const ProcessDef PROCESS_DEFS[] = {
    {"modules", []() { return new Modules; }},
    {"templates", []() { return new Templates; }},
    {"titles", []() { return new Titles; }},
};

ProcessGroup::ProcessGroup(Environment* environment) : m_environment(environment) {}

void ProcessGroup::addProcessByName(const std::string& name, const std::string& parameters) {
  for (const ProcessDef& processDef : PROCESS_DEFS) {
    if (name == processDef.name) {
      std::unique_ptr<Process> process(processDef.factory());
      process->setName(name);
      process->setParameters(parameters);
      process->setEnvironment(m_environment);
      m_processes.push_back(std::move(process));
      return;
    }
  }
  throw std::invalid_argument("Invalid process name: '" + name + "'");
}

void ProcessGroup::initializeProcesses() {
  for (const unique_ptr<Process>& process : m_processes) {
    process->prepare();
  }
}

void ProcessGroup::finalizeProcesses() {
  for (const unique_ptr<Process>& process : m_processes) {
    process->finalize();
  }
}

void ProcessGroup::runOnDump(mwc::PagesDump& dump) {
  initializeProcesses();
  Page page(m_environment->wiki());
  for (int iPage = 1; dump.getArticle(); iPage++) {
    if (iPage % 10000 == 0) std::cerr << iPage << " pages read" << std::endl;
    page.reset(dump);
    for (const unique_ptr<Process>& process : m_processes) {
      process->processPage(page);
    }
  }
  finalizeProcesses();
}

void ProcessGroup::runOnPagesForTest(const vector<mwc::Revision>& revisions) {
  initializeProcesses();
  Page page(m_environment->wiki());
  for (const mwc::Revision& revision : revisions) {
    page.reset(revision.title, 1, revision.timestamp, revision.content);
    for (const unique_ptr<Process>& process : m_processes) {
      process->processPage(page);
    }
  }
  finalizeProcesses();
}

vector<string> getValidProcessNames() {
  vector<string> validProcessNames;
  for (const ProcessDef& processDef : PROCESS_DEFS) {
    validProcessNames.push_back(processDef.name);
  }
  return validProcessNames;
}

}  // namespace dump_processing
