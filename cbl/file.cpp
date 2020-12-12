#include "file.h"
#include <errno.h>
#include <sys/stat.h>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "error.h"

using std::string;

namespace cbl {

bool fileExists(const string& path) {
  struct stat sb;
  int statResult = stat(path.c_str(), &sb);
  return statResult == 0 || errno == EOVERFLOW;
}

string readOpenedFile(FILE* file) {
  if (fseek(file, 0, SEEK_END) == -1) {
    throw SystemError("fseek failed: " + getCErrorString(errno));
  }

  long length = ftell(file);
  if (length == -1) {
    throw SystemError("ftell failed: " + getCErrorString(errno));
  } else if (length == LONG_MAX) {
    // Reading a directory is undefined behavior, but this helps to provide a better error message in some cases.
    throw InternalError("ftell returned LONG_MAX (may indicate a directory)");
  }

  if (fseek(file, 0, SEEK_SET) == -1) {
    throw SystemError("fseek failed: " + getCErrorString(errno));
  }

  string content;
  if (length > 0) {  // &content[0] is undefined if length == 0.
    content.resize(length);
    errno = 0;
    long freadResult = fread(&content[0], 1, length, file);
    if (freadResult != length) {
      throw SystemError(errno != 0 ? "fread failed: " + getCErrorString(errno) : "bad file length");
    }
  }
  return content;
}

string readFile(const string& path) {
  FILE* file = fopen(path.c_str(), "r");
  if (file == nullptr) {
    int savedErrno = errno;
    if (savedErrno == ENOENT || savedErrno == ENOTDIR) {
      throw FileNotFoundError("File '" + path + "' does not exist");
    }
    string errorMessage = "Cannot open '" + path + "': " + getCErrorString(savedErrno);
    if (savedErrno == EACCES) {
      throw PermissionError(errorMessage);
    }
    throw SystemError(errorMessage);
  }
  RunOnDestroy fileCloser([file]() { fclose(file); });
  try {
    return readOpenedFile(file);
  } catch (const SystemError& error) {
    throw SystemError("Cannot read '" + path + "': " + error.what());
  }
}

static void writeOpenedFileAndCloseIt(const string& path, FILE* file, const string& content) {
  if (file == nullptr) {
    int savedErrno = errno;
    string errorMessage = "Cannot open '" + path + "' in write mode: " + getCErrorString(savedErrno);
    if (savedErrno == ENOENT || savedErrno == ENOTDIR) {
      throw FileNotFoundError(errorMessage);
    } else if (savedErrno == EACCES) {
      throw PermissionError(errorMessage);
    }
    throw SystemError(errorMessage);
  }
  if (fwrite(content.c_str(), 1, content.size(), file) != content.size()) {
    int savedErrno = errno;
    fclose(file);
    throw SystemError("Cannot write '" + path + "': " + getCErrorString(savedErrno));
  }
  if (fclose(file) != 0) {
    int savedErrno = errno;
    throw SystemError("Cannot write '" + path + "': " + getCErrorString(savedErrno));
  }
}

void writeFile(const string& path, const string& content) {
  writeOpenedFileAndCloseIt(path, fopen(path.c_str(), "w"), content);
}

void writeFileAtomically(const string& path, const string& content) {
  string tempPath = path + ".tmp-XXXXXX";
  int fd = mkstemp(&tempPath[0]);
  if (fd == -1) {
    int savedErrno = errno;
    string errorMessage = "Cannot write '" + path + "' because mkstemp failed: " + getCErrorString(savedErrno);
    if (savedErrno == ENOENT || savedErrno == ENOTDIR) {
      throw FileNotFoundError(errorMessage);
    } else if (savedErrno == EACCES) {
      throw PermissionError(errorMessage);
    } else {
      throw SystemError(errorMessage);
    }
  }
  writeOpenedFileAndCloseIt(tempPath, fdopen(fd, "w"), content);
  if (rename(tempPath.c_str(), path.c_str()) != 0) {
    int savedErrno = errno;
    throw SystemError("Cannot write '" + path + "' because renaming from '" + tempPath +
                      "' failed: " + getCErrorString(savedErrno));
  }
}

void removeFile(const string& path, bool mustExist) {
  if (remove(path.c_str()) == 0) {
    return;
  }
  int savedErrno = errno;
  if (savedErrno == ENOENT) {
    if (mustExist) {
      throw FileNotFoundError("File '" + path + "' does not exist");
    }
    return;
  }
  string errorMessage = "Cannot remove '" + path + "': " + getCErrorString(savedErrno);
  if (savedErrno == EPERM || savedErrno == EACCES) {
    throw PermissionError(errorMessage);
  }
  throw SystemError(errorMessage);
}

}  // namespace cbl
