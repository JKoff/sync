#ifndef FS_UTIL_H
#define FS_UTIL_H

#include <regex>
#include <string>
#include <vector>

bool filterPath(const std::string &root, const std::vector<std::regex> excludes, const std::string &path);

#endif
