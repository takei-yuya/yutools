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
  ssize_t len = 1024;
  std::vector<char> buffer(len);
  while (true) {
    ssize_t n = readlink(path.c_str(), buffer.data(), len - 1);
    if (n < 0) {
      return "";
    }
    if (n < len - 1) {
      buffer[n] = '\0';
      return std::string(buffer.data(), n);
    }
    len *= 2;
    buffer.resize(len);
  }
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

ssize_t GetFileSize(const std::string& path) {
  struct stat st;
  if (stat(path.c_str(), &st) == 0) {
    return st.st_size;
  }
  return -1;
}

}  // namespace file_util
