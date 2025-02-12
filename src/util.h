#ifndef UTIL_H
#define UTIL_H

#include <list>
#include <filesystem>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

// Returns list of tokens delimited by delim.
std::vector<std::string> tokenize(std::string str, char delim);
std::vector<std::wstring> tokenize(std::wstring str, wchar_t delim);

// Takes a relative path, and returns parents nested-est to least nested, e.g. [a/b/c, a/b, a]
std::list<std::filesystem::path> pathParents(const std::filesystem::path &path);

#endif
