#include "directory.h"
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include "error.h"
#include "string.h"

using std::string;

namespace cbl {

bool isDirectory(const string& path) {
  struct stat sb;
  int statResult = stat(path.c_str(), &sb);
  return statResult == 0 && S_ISDIR(sb.st_mode);
}

void makeDir(const string& path) {
  int mkdirResult = mkdir(path.c_str(), 0777);
  if (mkdirResult == 0) return;
  int savedErrno = errno;
  if (savedErrno == EEXIST && isDirectory(path)) return;
  string errorMessage = cbl::concat("Failed to create directory '", path + "': ", getCErrorString(savedErrno));
  if (savedErrno == ENOENT) {
    throw FileNotFoundError(errorMessage);
  } else if (savedErrno == EACCES) {
    throw PermissionError(errorMessage);
  }
  throw SystemError(errorMessage);
}

}  // namespace cbl
