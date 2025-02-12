#ifndef FS_UTIL_H
#define FS_UTIL_H

#include <filesystem>
#include <regex>
#include <string>
#include <vector>

bool filterPath(const std::filesystem::path &root, const std::vector<std::wregex> excludes, const std::filesystem::path &path);

#endif
