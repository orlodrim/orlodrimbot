#ifndef MWC_UTIL_XML_DUMP_H
#define MWC_UTIL_XML_DUMP_H

#include <cstdint>
#include <cstdio>
#include <string>
#include "cbl/date.h"

namespace mwc {

// Parsing of the pages-meta-current dump.
// This can be fed by calling bzcat on a merged <wiki>-<date>-pages-meta-current.xml.bz2 file or on the concatenation of
// all <wiki>-<date>-pages-meta-current<number>.xml-*.bz2 from a dump.
class PagesDump {
public:
  PagesDump();
  explicit PagesDump(FILE* inputFile);
  ~PagesDump();
  bool getArticle();
  void getContent(std::string& wcode);
  const std::string& title() const { return m_title; }
  int64_t pageid() const { return m_pageid; }
  cbl::Date timestamp() const { return m_timestamp; }

private:
  char* getTag(const char* tag);
  bool getLine();

  FILE* m_inputFile = nullptr;
  std::string m_title;
  int64_t m_pageid = 0;
  cbl::Date m_timestamp;
  char* m_line = nullptr;
  char* m_buffer = nullptr;
  size_t m_length = 0;
  int m_state = 0;
};

}  // namespace mwc

#endif
