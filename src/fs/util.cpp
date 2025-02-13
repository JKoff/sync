#include "util.h"

#include "../util.h"
#include "../util/log.h"

using namespace std;

bool filterPath(const std::filesystem::path &root, const vector<wregex> excludes, const std::filesystem::path &path) {
    // Scanner's ignoring these anyway since it doesn't want to infinitely recurse into .,
    // but the watcher doesn't ignore these so we ignore them here.

    if (path == root) {
        return true;
    }

    wstring relpath = path.lexically_relative(root).wstring();

    for (const wregex &r : excludes) {
        if (regex_search(relpath, r)) {
            return false;
        }
    }

    return true;
}
