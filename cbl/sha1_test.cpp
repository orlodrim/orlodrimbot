#include "sha1.h"
#include "log.h"

namespace cbl {

void testSHA1() {
  CBL_ASSERT_EQ(sha1(""), "da39a3ee5e6b4b0d3255bfef95601890afd80709");
  CBL_ASSERT_EQ(sha1("w"), "aff024fe4ab0fece4091de044c58c9ae4233383a");
  CBL_ASSERT_EQ(sha1("wiki"), "d5ea4e40ddbfc76e763c5050492ab2e15a27e01e");
  CBL_ASSERT_EQ(sha1("wiki wiki wiki wiki"), "76e3e238f7c15869647a39d842f26bce0b6627f4");
}

}  // namespace cbl

int main() {
  cbl::testSHA1();
  return 0;
}
