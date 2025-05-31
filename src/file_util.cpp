#include "file_util.hpp"

#include <memory>
#include <functional>

#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

namespace file_util {
using DirPtr = std::unique_ptr<DIR, std::function<int(DIR*)>>;

std::vector<std::string> ScanDirectory(const std::string& path) {
  std::vector<std::string> result;
  DirPtr dir(opendir(path.c_str()), closedir);
  if (!dir) {
    return result;
  }
  struct dirent* entry;
  while ((entry = readdir(dir.get())) != nullptr) {
    std::string name(entry->d_name);
    if (name != "." && name != "..") {
      result.push_back(name);
    }
  }
  return result;
}

std::string ReadLink(const std::string& path) {
  char buffer[PATH_MAX];
  ssize_t len = readlink(path.c_str(), buffer, sizeof(buffer) - 1);
  if (len < 0) {
    return "";
  }
  buffer[len] = '\0';
  return std::string(buffer);
}

bool IsFile(const std::string& path) {
  struct stat st;
  return (stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode));
}

bool IsDirectory(const std::string& path) {
  struct stat st;
  return (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode));
}

bool IsOwned(const std::string& path) {
  struct stat st;
  return (stat(path.c_str(), &st) == 0 && st.st_uid == getuid());
}

size_t GetFileSize(const std::string& path) {
  struct stat st;
  if (stat(path.c_str(), &st) == 0) {
    return st.st_size;
  }
  return 0;
}

}  // namespace file_util
