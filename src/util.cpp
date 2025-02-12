#include "util.h"

#include <cstdlib>
#include <iomanip>
#include <limits.h>
#include <stdexcept>

using namespace std;

vector<string> tokenize(string str, char delim) {
    vector<string> result;
    if (str.length() == 0) {
        return result;
    }

    // start range 0 if str[0] is not /, otherwise start range 1
    string::size_type pos = str[0] == delim ? 1 : 0;

    do {
        string::size_type next_pos = str.find(delim, pos + 1);
        if (next_pos != string::npos) {
            // end range pos - 1
            result.push_back(str.substr(pos, next_pos - pos));

            // start range pos + 1
            pos = next_pos + 1;
        } else {
            // end range pos - 1
            result.push_back(str.substr(pos));

            // for terminating condition
            pos = next_pos;
        }
    } while (pos != string::npos);

    return result;
}

vector<wstring> tokenize(wstring str, wchar_t delim) {
    vector<wstring> result;
    if (str.length() == 0) {
        return result;
    }

    // start range 0 if str[0] is not /, otherwise start range 1
    wstring::size_type pos = str[0] == delim ? 1 : 0;

    do {
        wstring::size_type next_pos = str.find(delim, pos + 1);
        if (next_pos != wstring::npos) {
            // end range pos - 1
            result.push_back(str.substr(pos, next_pos - pos));

            // start range pos + 1
            pos = next_pos + 1;
        } else {
            // end range pos - 1
            result.push_back(str.substr(pos));

            // for terminating condition
            pos = next_pos;
        }
    } while (pos != wstring::npos);

    return result;
}

list<std::filesystem::path> pathParents(const std::filesystem::path &path) {
    list<std::filesystem::path> result;
    std::filesystem::path prefix = path.parent_path();
    while (!prefix.empty()) {
        result.push_back(prefix);
        prefix = prefix.parent_path();
    }
    return result;
}
