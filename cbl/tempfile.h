#ifndef CBL_TEMPFILE_H
#define CBL_TEMPFILE_H

#include <string>

namespace cbl {

class TempFile {
public:
  TempFile();
  ~TempFile();
  const std::string& path() const { return m_path; }

private:
  std::string m_path;
};

class TempDir {
public:
  TempDir();
  explicit TempDir(const std::string& prefix);
  ~TempDir();
  const std::string& path() const { return m_path; }

private:
  std::string m_path;
};

}  // namespace cbl

#endif
