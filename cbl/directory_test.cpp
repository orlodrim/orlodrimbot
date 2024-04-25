#include "directory.h"
#include <algorithm>
#include <string>
#include "error.h"
#include "file.h"
#include "log.h"
#include "string.h"
#include "tempfile.h"
#include "unittest.h"

using std::string;

namespace cbl {

class DirectoryTest : public cbl::Test {
public:
  DirectoryTest() {
    m_existingFile = m_tempDir.path() + "/existing-file";
    writeFile(m_existingFile, "test content");
  }
  CBL_TEST_CASE(isDirectory) {
    CBL_ASSERT(isDirectory(m_tempDir.path()));
    CBL_ASSERT(!isDirectory(m_existingFile));
    CBL_ASSERT(!isDirectory(m_tempDir.path() + "/non-existing-file"));
  }
  CBL_TEST_CASE(makeDir_Standard) {
    makeDir(m_tempDir.path() + "/subdir");
    CBL_ASSERT(isDirectory(m_tempDir.path() + "/subdir"));
    makeDir(m_tempDir.path() + "/subdir");  // Should not fail.
  }
  CBL_TEST_CASE(makeDir_NoOverwriteFile) {
    bool exceptionThrown = false;
    try {
      makeDir(m_existingFile);
    } catch (const SystemError&) {
      exceptionThrown = true;
    }
    CBL_ASSERT(exceptionThrown);
    string content = readFile(m_existingFile);
    CBL_ASSERT_EQ(content, "test content");
  }
  CBL_TEST_CASE(makeDir_NotRecursive) {
    bool exceptionThrown = false;
    try {
      makeDir(m_tempDir.path() + "/subdir2/subsubdir");
    } catch (const FileNotFoundError&) {
      exceptionThrown = true;
    }
    CBL_ASSERT(exceptionThrown);
    CBL_ASSERT(!fileExists(m_tempDir.path() + "/subdir2"));
  }

private:
  TempDir m_tempDir;
  string m_existingFile;
};

}  // namespace cbl

int main() {
  cbl::DirectoryTest().run();
  return 0;
}
