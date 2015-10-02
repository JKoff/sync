#ifndef UTIL_H
#define UTIL_H

#include <list>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

// Returns list of tokens delimited by delim.
std::vector<std::string> tokenize(std::string str, char delim);

// Returns (parent, child) pairs
// These pairs must go from nested-est to least nested.
std::list<std::pair<std::string,std::string>> pathParents(const std::string &path);

// Has the pre-condition that path has a parent
std::string pathParent(const std::string &path);

// Filename
std::string filename(const std::string &path);

// Wrapper to realpath(3)
std::string realpath(const std::string &path);

#endif
