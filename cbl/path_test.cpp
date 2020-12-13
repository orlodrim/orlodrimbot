#include "path.h"
#include "log.h"
#include "unittest.h"

namespace cbl {

class PathTest : public cbl::Test {

  CBL_TEST_CASE(getDirName) {
    CBL_ASSERT_EQ(getDirName(""), "");
    CBL_ASSERT_EQ(getDirName("a"), "");
    CBL_ASSERT_EQ(getDirName("a/"), "a");
    CBL_ASSERT_EQ(getDirName("a/b"), "a");
    CBL_ASSERT_EQ(getDirName("a/b/"), "a/b");
    CBL_ASSERT_EQ(getDirName("a/b/c"), "a/b");
    CBL_ASSERT_EQ(getDirName("/a"), "/");
    CBL_ASSERT_EQ(getDirName("/a/b"), "/a");
    CBL_ASSERT_EQ(getDirName("/"), "/");
  }

  CBL_TEST_CASE(getBaseName) {
    CBL_ASSERT_EQ(getBaseName(""), "");
    CBL_ASSERT_EQ(getBaseName("a"), "a");
    CBL_ASSERT_EQ(getBaseName("a/"), "");
    CBL_ASSERT_EQ(getBaseName("a/b"), "b");
    CBL_ASSERT_EQ(getBaseName("a/b/c"), "c");
    CBL_ASSERT_EQ(getBaseName("/a"), "a");
    CBL_ASSERT_EQ(getBaseName("/"), "");
  }

  CBL_TEST_CASE(joinPaths) {
    CBL_ASSERT_EQ(joinPaths("", "b"), "b");
    CBL_ASSERT_EQ(joinPaths("/", "b"), "/b");
    CBL_ASSERT_EQ(joinPaths("a", "b"), "a/b");
    CBL_ASSERT_EQ(joinPaths("a/", "b"), "a/b");
    // Bad usage, but test those cases anyway to avoid unexpected changes.
    CBL_ASSERT_EQ(joinPaths("", ""), "");
    CBL_ASSERT_EQ(joinPaths("/", ""), "/");
    CBL_ASSERT_EQ(joinPaths("a", ""), "a/");
    CBL_ASSERT_EQ(joinPaths("a", "/b"), "a//b");
  }

};

}  // namespace cbl

int main() {
  cbl::PathTest().run();
  return 0;
}
