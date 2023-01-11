#ifndef CBL_FILE_H
#define CBL_FILE_H

#include <cstdio>
#include <string>
#include <string_view>

namespace cbl {

// Returns true if `path` refers to an existing file, false otherwise.
// A file is considered as non-existent if the user does not have the required permissions to stat it.
bool fileExists(const std::string& path);

// Reads the content of `file` from the current position to its end.
// The file must be seekable (e.g. not a FIFO).
// Throws: SystemError.
std::string readOpenedFile(FILE* file);

// Reads the content of file `path`.
// The file must be seekable (e.g. not a FIFO). If `path` is a directory, the behavior is undefined.
// Throws: FileNotFoundError, PermissionError, SystemError.
std::string readFile(const std::string& path);

// Removes a file. Also works with empty directories.
// Throws: FileNotFoundError (only if mustExist is true), PermissionError, SystemError.
void removeFile(const std::string& path, bool mustExist);

// Writes `content` to file `path`.
// Overwrites the file if it already exists. Not atomic: in case of failure, the content may be partially written.
// Throws: FileNotFoundError (e.g. if the directory does not exist), PermissionError, SystemError.
void writeFile(const std::string& path, std::string_view content);

// Same as writeFile, but writes to a temporary file first and then rename to the final name.
// This is atomic only if the rename operation itself is atomic, which is usually true on local Linux file systems.
// Attributes of the original file (permissions, owner, group) are not preserved. Even if the target file is read-only,
// it will be overwritten if that the user has write access to the parent directory.
// Throws: SystemError.
void writeFileAtomically(const std::string& path, std::string_view content);

}  // namespace cbl

#endif
