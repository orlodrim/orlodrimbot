#include "containers_helpers.h"
#include <map>
#include <memory>
#include <string>
#include <utility>
#include "log.h"
#include "unittest.h"

using std::make_unique;
using std::map;
using std::string;
using std::unique_ptr;

namespace cbl {

class ContainersHelpersTest : public cbl::Test {
private:
  CBL_TEST_CASE(findOrNullMap) {
    map<string, string> mainComponent = {{"ocean", "water"}, {"mountain", "rock"}};

    string* lookupResult = findOrNull(mainComponent, "ocean");
    CBL_ASSERT(lookupResult != nullptr);
    CBL_ASSERT_EQ(*lookupResult, "water");
    *lookupResult = "salted water";
    CBL_ASSERT_EQ(mainComponent.at("ocean"), "salted water");

    CBL_ASSERT(findOrNull(mainComponent, "air") == nullptr);
  }
  CBL_TEST_CASE(findOrNullConstMap) {
    const map<string, string> mainComponent = {{"ocean", "water"}, {"mountain", "rock"}};
    const string* lookupResult = findOrNull(mainComponent, "ocean");
    CBL_ASSERT(lookupResult != nullptr);
    CBL_ASSERT_EQ(*lookupResult, "water");
    CBL_ASSERT(findOrNull(mainComponent, "air") == nullptr);
  }
  CBL_TEST_CASE(findPtrOrNullMap) {
    string s1 = "water";
    string s2 = "rock";
    const map<string, string*> mainComponent = {{"ocean", &s1}, {"mountain", &s2}};

    string* lookupResult = findPtrOrNull(mainComponent, "ocean");
    CBL_ASSERT(lookupResult != nullptr);
    CBL_ASSERT_EQ(*lookupResult, "water");
    *lookupResult = "salted water";
    CBL_ASSERT_EQ(*mainComponent.at("ocean"), "salted water");

    CBL_ASSERT(findOrNull(mainComponent, "air") == nullptr);
  }
  CBL_TEST_CASE(findPtrOrNullConstPointers) {
    string s1 = "water";
    string s2 = "rock";
    const map<string, const string*> mainComponent = {{"ocean", &s1}, {"mountain", &s2}};
    const string* lookupResult = findPtrOrNull(mainComponent, "ocean");
    CBL_ASSERT(lookupResult != nullptr);
    CBL_ASSERT_EQ(*lookupResult, "water");
    CBL_ASSERT(findPtrOrNull(mainComponent, "air") == nullptr);
  }
  CBL_TEST_CASE(findPtrOrNullUniquePtr) {
    map<string, unique_ptr<string>> mainComponent;
    mainComponent["ocean"] = make_unique<string>("water");
    mainComponent["mountain"] = make_unique<string>("rock");

    string* lookupResult = findPtrOrNull(mainComponent, "ocean");
    CBL_ASSERT(lookupResult != nullptr);
    CBL_ASSERT_EQ(*lookupResult, "water");
    *lookupResult = "salted water";
    CBL_ASSERT_EQ(*mainComponent.at("ocean"), "salted water");

    CBL_ASSERT(findOrNull(mainComponent, "air") == nullptr);
  }
};

}

int main() {
  cbl::ContainersHelpersTest().run();
  return 0;
}
