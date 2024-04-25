#ifndef PROCESSING_LIB_H
#define PROCESSING_LIB_H

#include <memory>
#include <string>
#include <vector>
#include "mwclient/util/xml_dump.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/dump/processing/processes/process.h"

namespace dump_processing {

class ProcessGroup {
public:
  explicit ProcessGroup(Environment* environment);
  void addProcessByName(const std::string& name, const std::string& parameters);
  void runOnDump(mwc::PagesDump& dump);
  void runOnPagesForTest(const std::vector<mwc::Revision>& revisions);

private:
  void initializeProcesses();
  void finalizeProcesses();

  Environment* m_environment = nullptr;
  std::vector<std::unique_ptr<Process>> m_processes;
};

std::vector<std::string> getValidProcessNames();

}  // namespace dump_processing

#endif
