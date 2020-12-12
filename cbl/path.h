// String operations on paths. These functions perform no disk access.
#ifndef CBL_PATH_H
#define CBL_PATH_H

#include <string>
#include <string_view>

namespace cbl {

// Returns the directory containing `path`, e.g. getDirName("/usr/bin/gcc") = "/usr/bin".
// `path` may be either an absolute or a relative path. Only '/' is recognized as a path separator.
// Edge cases: getDirName("") = "", getDirName("somefile") = "", getDirName("somedir/") = "somedir".
std::string getDirName(const std::string& path);

// Strips everything until the last slash from path, e.g. getBaseName("/usr/bin/gcc") = "gcc".
// Only '/' is recognized as a path separator.
std::string getBaseName(const std::string& path);

// Joins two paths with '/', e.g. joinPaths("/usr", "bin/gcc") = "/usr/bin/gcc".
// If path1 already ends with '/', no additional '/' is inserted between path1 and path2.
// If path1 is empty, path2 is returned.
// path2 must be a relative path, i.e. it must not start with '/'.
std::string joinPaths(std::string_view path1, std::string_view path2);

}  // namespace cbl

#endif
