#include "process.h"
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "cbl/date.h"
#include "cbl/log.h"
#include "cbl/string.h"
#include "mwclient/parser.h"
#include "mwclient/util/xml_dump.h"
#include "mwclient/wiki.h"

using std::string;
using std::string_view;
using std::unordered_map;
using std::vector;

namespace dump_processing {

Environment::Environment(mwc::Wiki& wiki, const string& dataDir) : m_wiki(&wiki), m_dataDir(dataDir) {}

enum PageKnownProperties {
  PKP_PARSEDCODE = 1,
  PKP_LINKS = 2,
  PKP_TEMPLATES = 4,
  PKP_TEMPLATES_WITH_NAME = 8,
};

Page::Page(mwc::Wiki& wiki) : m_wiki(&wiki) {}

void Page::reset(const string& title, int64_t pageid, const cbl::Date& timestamp, const string& code) {
  m_title = title;
  m_pageid = pageid;
  m_timestamp = timestamp;
  m_code = code;
  resetInternal();
}

void Page::reset(mwc::PagesDump& dump) {
  m_title = dump.title();
  m_pageid = dump.pageid();
  m_timestamp = dump.timestamp();
  dump.getContent(m_code);
  resetInternal();
}

void Page::resetInternal() {
  mwc::TitleParts titleParts = m_wiki->parseTitle(m_title);
  m_namespace = titleParts.namespaceNumber;
  m_prefix = string(titleParts.namespace_());
  m_unprefixedTitle = string(titleParts.unprefixedTitle());
  m_knownProperties = 0;
  m_parsedCode.clear();
  m_links.clear();
  m_templates.clear();
}

const wikicode::List& Page::parsedCode() {
  if (!(m_knownProperties & PKP_PARSEDCODE)) {
    m_parsedCode = wikicode::parse(m_code);
    m_knownProperties |= PKP_PARSEDCODE;
  }
  return m_parsedCode;
}

const vector<const wikicode::Link*>& Page::links() {
  if (!(m_knownProperties & PKP_LINKS)) {
    m_links.clear();
    for (const wikicode::Link& link : parsedCode().getLinks()) {
      m_links.push_back(&link);
    }
    m_knownProperties |= PKP_LINKS;
  }
  return m_links;
}

const vector<const wikicode::Template*>& Page::templates() {
  if (!(m_knownProperties & PKP_TEMPLATES)) {
    m_templates.clear();
    for (const wikicode::Template& template_ : parsedCode().getTemplates()) {
      m_templates.push_back(&template_);
    }
    m_knownProperties |= PKP_TEMPLATES;
  }
  return m_templates;
}

Process::Process(const vector<string>& validParameters) {
  for (const string& key : validParameters) {
    m_parameters[key];
  }
}

Process::~Process() {}

void Process::prepare() {}

void Process::finalize() {
  if (m_mainOutputFile != nullptr) {
    CBL_ASSERT_EQ(fclose(m_mainOutputFile), 0) << "Failed to close the output file of process " << m_name;
    m_mainOutputFile = nullptr;
  }
}

void Process::setName(const string& name) {
  m_name = name;
}

void Process::setEnvironment(Environment* environment) {
  m_environment = environment;
}

void Process::setParameters(const string& parameters) {
  for (string_view keyAndValue : cbl::split(parameters, ',', /* ignoreLastFieldIfEmpty = */ true)) {
    size_t colonPosition = keyAndValue.find(':');
    if (colonPosition == string::npos) {
      throw std::invalid_argument(
          cbl::concat("Process '", m_name, "' got a parameter with no value: '", keyAndValue, "'"));
    }
    string key(keyAndValue.substr(0, colonPosition));
    unordered_map<string, string>::iterator it = m_parameters.find(key);
    if (it == m_parameters.end()) {
      throw std::invalid_argument(cbl::concat("Process '", m_name, "' got an invalid parameter '", key, "'"));
    } else if (!it->second.empty()) {
      throw std::invalid_argument(cbl::concat("Process '", m_name, "' got two values for parameter '", key, "'"));
    }
    it->second = string(keyAndValue.substr(colonPosition + 1));
  }
}

bool Process::hasParameter(const std::string& key) const {
  return m_parameters.count(key) != 0;
}

const string& Process::getParameter(const string& key) const {
  unordered_map<string, string>::const_iterator paramIt = m_parameters.find(key);
  CBL_ASSERT(paramIt != m_parameters.end())
      << "Internal error: process '" << m_name << "' has no parameter '" << key << "'";
  CBL_ASSERT(!paramIt->second.empty()) << "Missing parameter '" << key << "' for process '" << m_name << "'";
  return paramIt->second;
}

string Process::getAbsolutePath(const string& fileName) {
  return fileName.starts_with("/") ? fileName : m_environment->dataDir() + fileName;
}

void Process::openMainOutputFileFromParam(const string& key) {
  CBL_ASSERT(m_mainOutputFile == nullptr);
  string fullPath = getAbsolutePath(getParameter(key));
  m_mainOutputFile = fopen(fullPath.c_str(), "w");
  CBL_ASSERT(m_mainOutputFile != nullptr) << "Cannot write to '" << fullPath << "'";
}

void Process::writePageToSimpleDump(const Page& page) const {
  FILE* outputFile = mainOutputFile();
  fprintf(outputFile, "%s\n", page.title().c_str());
  for (string_view line : cbl::splitLines(page.code())) {
    fprintf(outputFile, " %.*s\n", static_cast<int>(line.size()), line.data());
  }
}

}  // namespace dump_processing
