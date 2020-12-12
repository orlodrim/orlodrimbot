#include "path.h"
#include <string>
#include <string_view>

using std::string;
using std::string_view;

namespace cbl {

string getDirName(const string& path) {
  size_t dirLength = path.rfind('/');
  if (dirLength == 0) {
    dirLength = 1;
  } else if (dirLength == string::npos) {
    dirLength = 0;
  }
  return path.substr(0, dirLength);
}

string getBaseName(const string& path) {
  size_t lastSlash = path.rfind('/');
  return path.substr(lastSlash == string::npos ? 0 : lastSlash + 1);
}

string joinPaths(string_view path1, string_view path2) {
  string path;
  path.reserve(path1.size() + path2.size() + 1);
  path += path1;
  if (!path.empty() && path[path.size() - 1] != '/') {
    path += '/';
  }
  path += path2;
  return path;
}

}  // namespace cbl
