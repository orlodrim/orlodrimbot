#include "tempfile.h"
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include "error.h"

using std::string;

namespace cbl {

TempFile::TempFile() {
  m_path = "/tmp/tmpXXXXXX";
  int fd = mkstemp(&m_path[0]);
  if (fd == -1) {
    throw SystemError("mkstemp failed");
  }
  close(fd);
}

TempFile::~TempFile() {
  remove(m_path.c_str());
}

TempDir::TempDir() : TempDir("tmp") {}

TempDir::TempDir(const string& prefix) {
  if (prefix.find('\'') != string::npos) {
    throw std::invalid_argument("prefix must not contain single quotes");
  }
  m_path = "/tmp/" + prefix + "XXXXXX";
  const char* result = mkdtemp(&m_path[0]);
  if (!result) {
    throw SystemError("mkdtemp failed");
  }
}

TempDir::~TempDir() {
  string command = "rm -rf '" + m_path + "'";
  if (system(command.c_str()) != 0) {
    // Ignore failures.
  }
}

}  // namespace cbl
