#ifndef YUTOOLS_FILE_UTIL_HPP_
#define YUTOOLS_FILE_UTIL_HPP_

#include <string>
#include <vector>

namespace file_util {

std::vector<std::string> ScanDirectory(const std::string& path);

std::string ReadLink(const std::string& path);

bool IsFile(const std::string& path);
bool IsDirectory(const std::string& path);
bool IsOwned(const std::string& path);
size_t GetFileSize(const std::string& path);

}  // namespace file_util

#endif  // YUTOOLS_FILE_UTIL_HPP_
