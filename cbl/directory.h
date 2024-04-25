#ifndef CBL_DIRECTORY_H
#define CBL_DIRECTORY_H

#include <string>
#include <string_view>
#include <vector>

namespace cbl {

// Returns true if `path` refers to an existing directory, false otherwise.
// A directory is considered as non-existent if the user does not have the required permissions to stat it.
bool isDirectory(const std::string& path);

// Creates directory `path`.
// The parent directory must exist. It is not an error if the directory already exists.
// Throws: FileNotFoundError (if the parent directory does not exist), PermissionError, SystemError.
void makeDir(const std::string& path);

}  // namespace cbl

#endif
