#include "xml_dump.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "cbl/date.h"

using std::string;

namespace mwc {
namespace {

const char* const DUMP_TEXT_BG = "<text ";
const char* const DUMP_TEXT_END = "</text>";
const int DUMP_TEXT_BG_LEN = strlen(DUMP_TEXT_BG);

char unescapeXML(char* p) {
  char* p1 = p;
  char* p2 = p;
  char finalChar = 0;

  for (; *p2; p1++, p2++) {
    if (*p2 == '<') {
      finalChar = '<';
      break;
    } else if (*p2 == '&') {
      p2++;
      if (*p2 == 'a')
        *p1 = '&';
      else if (*p2 == 'l')
        *p1 = '<';
      else if (*p2 == 'g')
        *p1 = '>';
      else if (*p2 == 'q')
        *p1 = '"';
      for (p2++; *p2 && *p2 != ';'; p2++) {}
      if (!*p2) break;
    } else {
      *p1 = *p2;
    }
  }
  *p1 = 0;
  return finalChar;
}

}  // namespace

PagesDump::PagesDump() : m_inputFile(stdin) {}

PagesDump::PagesDump(FILE* inputFile) : m_inputFile(inputFile) {}

PagesDump::~PagesDump() {
  if (m_buffer) {
    free(m_buffer);
  }
}

char* PagesDump::getTag(const char* tag) {
  char* pStart;
  do {
    if (getline(&m_buffer, &m_length, m_inputFile) == -1) {
      return nullptr;
    }
    pStart = strstr(m_buffer, tag);
  } while (!pStart);
  pStart += strlen(tag);
  unescapeXML(pStart);
  return pStart;
}

bool PagesDump::getArticle() {
  char* buffer;
  buffer = getTag("<title>");
  if (!buffer) return false;
  m_title = buffer;
  buffer = getTag("<id>");
  if (!buffer) return false;
  m_pageid = atoll(buffer);
  buffer = getTag("<timestamp>");
  if (!buffer) return false;
  m_timestamp = cbl::Date::fromISO8601(buffer);
  m_state = 0;
  return true;
}

bool PagesDump::getLine() {
  if (m_state == 2) {
    return false;
  } else if (m_state != 0) {
    if (getline(&m_buffer, &m_length, m_inputFile) == -1) {
      return false;
    }
    m_line = m_buffer;
  } else {
    char* pStart;
    do {
      if (getline(&m_buffer, &m_length, m_inputFile) == -1) {
        return false;
      }
      if (strstr(m_buffer, "</page>")) {
        m_state = 2;
        return false;
      }
      pStart = strstr(m_buffer, DUMP_TEXT_BG);
    } while (!pStart);
    pStart += DUMP_TEXT_BG_LEN;
    for (; *pStart && *pStart != '>'; pStart++) {}
    if (!*pStart) return false;
    m_line = pStart + 1;
    m_state = 1;
  }

  if (unescapeXML(m_line)) {
    m_state = 2;
  }

  return true;
};

void PagesDump::getContent(string& wcode) {
  wcode.clear();
  while (getLine()) {
    wcode += m_line;
  }
}

}  // namespace mwc
