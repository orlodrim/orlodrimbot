#include "thread.h"
#include <string_view>
#include <vector>
#include "cbl/log.h"
#include "cbl/unittest.h"

using std::string_view;
using std::vector;

namespace talk_page_archiver {

class ThreadTest : public cbl::Test {
public:
  CBL_TEST_CASE(parseCodeAsThreads) {
    string_view inputCode =
        "Line 1\n"
        "=== Line 2 ===\n"
        "Line 3\n"
        "== Line 4 ==\n"
        "Line 5\n"
        "=== Line 6 ===\n"
        "== Line 7\n"
        "== Line 8 ==\n"
        "=Line 9=\n"
        "Line 10\n"
        "==Line 11==\n"
        "Line 12\n"
        "== Line 13 == <!-- --> <!-- -->\n"
        "Line 14\n";
    vector<Thread> threads = parseCodeAsThreads(inputCode);
    CBL_ASSERT_EQ(static_cast<int>(threads.size()), 6);
    CBL_ASSERT_EQ(threads[0].titleLevel(), 0);
    CBL_ASSERT_EQ(threads[0].text(), "Line 1\n=== Line 2 ===\nLine 3\n");
    CBL_ASSERT_EQ(threads[1].titleLevel(), 2);
    CBL_ASSERT_EQ(threads[1].text(), "== Line 4 ==\nLine 5\n=== Line 6 ===\n== Line 7\n");
    CBL_ASSERT_EQ(threads[2].titleLevel(), 2);
    CBL_ASSERT_EQ(threads[2].text(), "== Line 8 ==\n");
    CBL_ASSERT_EQ(threads[3].titleLevel(), 1);
    CBL_ASSERT_EQ(threads[3].text(), "=Line 9=\nLine 10\n");
    CBL_ASSERT_EQ(threads[4].titleLevel(), 2);
    CBL_ASSERT_EQ(threads[4].text(), "==Line 11==\nLine 12\n");
    CBL_ASSERT_EQ(threads[5].titleLevel(), 2);
    CBL_ASSERT_EQ(threads[5].text(), "== Line 13 == <!-- --> <!-- -->\nLine 14\n");
  }
};

}  // namespace talk_page_archiver

int main() {
  talk_page_archiver::ThreadTest().run();
  return 0;
}
