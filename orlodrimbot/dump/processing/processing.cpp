// Reads a Wikipedia dump once and runs one or multiple processes on each page.
// This is significantly faster than doing a pass on the dump for each task, especially when they have to be
// decompressed on the fly.
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "cbl/args_parser.h"
#include "cbl/string.h"
#include "mwclient/util/init_wiki.h"
#include "mwclient/util/xml_dump.h"
#include "mwclient/wiki.h"
#include "orlodrimbot/dump/processing/processes/process.h"
#include "processing_lib.h"

using std::string;
using std::string_view;
using std::unordered_map;
using std::vector;

struct ProcessParams {
  string flagName;
  string flagValue;
};

int main(int argc, char** argv) {
  cbl::ArgsParser argsParser;
  unordered_map<string, ProcessParams> processParamsByName;
  for (const string& processName : dump_processing::getValidProcessNames()) {
    ProcessParams& processParams = processParamsByName[processName];
    processParams.flagName = "--" + processName + "-params";
    // This depends on the fact that insertion in unordered_map does not invalidate pointers to elements.
    argsParser.addArgs(processParams.flagName.c_str(), &processParams.flagValue);
  }
  mwc::WikiFlags wikiFlags(mwc::FRENCH_WIKIPEDIA_BOT);
  string dataDir;
  string processesNamesStr;
  argsParser.addArgs(&wikiFlags, "--datadir,required", &dataDir, "--processes,required", &processesNamesStr);
  argsParser.run(argc, argv);
  vector<string> processesNames;
  for (string_view processName : cbl::split(processesNamesStr, ',')) {
    // Validate process names before running potentially slow initialization.
    if (processParamsByName.count(cbl::legacyStringConv(processName)) == 0) {
      std::cerr << "Invalid process '" << processName << "'\n";
      exit(1);
    }
    processesNames.push_back(std::string(processName));
  }

  mwc::Wiki wiki;
  mwc::initWikiFromFlags(wikiFlags, wiki);
  if (!dataDir.empty() && !dataDir.ends_with("/")) {
    dataDir += '/';
  }
  dump_processing::Environment environment(wiki, dataDir);
  dump_processing::ProcessGroup processGroup(&environment);
  for (const string& processName : processesNames) {
    processGroup.addProcessByName(processName, processParamsByName.at(processName).flagValue);
  }

  mwc::PagesDump dump;
  processGroup.runOnDump(dump);
  return 0;
}
