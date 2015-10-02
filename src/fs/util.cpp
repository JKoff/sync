#include "util.h"

#include "../util.h"
#include "../util/log.h"

using namespace std;

bool filterPath(const string &root, const vector<regex> excludes, const string &path) {
    // Scanner's ignoring these anyway since it doesn't want to infinitely recurse into .,
    // but the watcher doesn't ignore these so we ignore them here.

    if (path.size() == root.size()) {
        return true;
    }

    string relpath = path.substr(root.length() + 1);

    for (const regex &r : excludes) {
        if (regex_search(relpath, r)) {
            return false;
        }
    }

    return true;
}
