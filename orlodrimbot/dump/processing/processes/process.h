#ifndef PROCESS_H
#define PROCESS_H

#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>
#include "cbl/date.h"
#include "mwclient/parser.h"
#include "mwclient/util/xml_dump.h"
#include "mwclient/wiki.h"

namespace dump_processing {

class Environment {
public:
  Environment(mwc::Wiki& wiki, const std::string& dataDir);
  mwc::Wiki& wiki() { return *m_wiki; }
  const std::string& dataDir() { return m_dataDir; }

private:
  mwc::Wiki* m_wiki;
  std::string m_dataDir;
};

class Page {
public:
  explicit Page(mwc::Wiki& wiki);
  void reset(const std::string& title, int64_t pageid, const cbl::Date& timestamp, const std::string& code);
  void reset(mwc::PagesDump& dump);

  const std::string& title() const { return m_title; }
  const std::string& prefix() const { return m_prefix; }
  const std::string& unprefixedTitle() const { return m_unprefixedTitle; }
  const int namespace_() const { return m_namespace; }
  const int64_t pageid() const { return m_pageid; }
  const cbl::Date& timestamp() const { return m_timestamp; }
  const std::string& code() const { return m_code; }
  const wikicode::List& parsedCode();
  const std::vector<const wikicode::Link*>& links();
  const std::vector<const wikicode::Template*>& templates();

private:
  void resetInternal();

  mwc::Wiki* const m_wiki;
  int m_knownProperties;
  std::string m_title;
  std::string m_prefix;
  std::string m_unprefixedTitle;
  int m_namespace;
  int64_t m_pageid;
  cbl::Date m_timestamp;
  std::string m_code;
  wikicode::List m_parsedCode;
  std::vector<const wikicode::Link*> m_links;
  std::vector<const wikicode::Template*> m_templates;
};

class Process {
public:
  explicit Process(const std::vector<std::string>& validParameters = {});
  virtual ~Process();
  void setName(const std::string& name);
  void setEnvironment(Environment* environment);
  void setParameters(const std::string& parameters);
  virtual void prepare();
  // Overrides must call the base version.
  virtual void finalize();
  virtual void processPage(Page& page) = 0;

protected:
  Environment& environment() { return *m_environment; }
  bool hasParameter(const std::string& key) const;
  const std::string& getParameter(const std::string& key) const;
  std::string getAbsolutePath(const std::string& fileName);
  void openMainOutputFileFromParam(const std::string& key);
  FILE* mainOutputFile() const { return m_mainOutputFile; }
  void writePageToSimpleDump(const Page& page) const;

private:
  std::string m_name;
  Environment* m_environment = nullptr;
  FILE* m_mainOutputFile = nullptr;
  std::unordered_map<std::string, std::string> m_parameters;
};

class ProcessWithSingleOutputFile : public Process {
public:
  ProcessWithSingleOutputFile() : Process({"output"}) {}
  void prepare() override { openMainOutputFileFromParam("output"); }
};

}  // namespace dump_processing

#endif
