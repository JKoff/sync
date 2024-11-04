#include "util.h"

#include <cstdlib>
#include <iomanip>
#include <limits.h>
#include <stdexcept>

using namespace std;

vector<string> tokenize(string str, char delim) {
    vector<string> result;
    if (str.size() == 0) {
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

list<pair<string,string>> pathParents(const string &path) {
    list<pair<string,string>> result;
    if (path.size() == 0) {
        return result;
    }

    // start range n-1 if path[n-1] is not /, otherwise start range n-2
	string::size_type n = path.size();
    string::size_type pos = path[n-1] == '/' ? n - 2 : n - 1;
    string child = path;

    do {
        string::size_type next_pos = path.rfind('/', pos - 1);
        if (next_pos != string::npos) {
            // end range pos - 1
            string parent = path.substr(0, next_pos);
            result.push_back(make_pair(parent, child));
            child = parent;

            // start range pos - 1
            pos = next_pos - 1;
        } else {
            // end range pos - 1
            string parent = path.substr(0, pos);
            result.push_back(make_pair(parent, child));
            child = parent;

            // for terminating condition
            pos = next_pos;
        }
    } while (pos != string::npos);

    return result;
}

string pathParent(const string &path) {
	string::size_type n = path.size();
    if (n == 0) {
        throw runtime_error("pathParent may not be used with a zero-length string.");
    }

    // start range n-1 if path[n-1] is not /, otherwise start range n-2
    string::size_type pos = path[n-1] == '/' ? n - 2 : n - 1;
    if (pos < 1) {
        throw runtime_error("pathParent called with an invalid string: " + path);
    }

    string::size_type next_pos = path.rfind('/', pos - 1);
    if (next_pos == string::npos) {
        throw runtime_error("No parent in pathParent for string: " + path);
    }

    return path.substr(0, next_pos);
}

string filename(const string &path) {
	string::size_type n = path.size();
    if (n == 0) {
        throw runtime_error("filename may not be used with a zero-length string.");
    }

    // start range n-1 if path[n-1] is not /, otherwise start range n-2
    string::size_type pos = path[n-1] == '/' ? n - 2 : n - 1;
    if (pos < 1) {
        throw runtime_error("filename called with an invalid string: " + path);
    }

    string::size_type next_pos = path.rfind('/', pos - 1);
    if (next_pos == string::npos) {
        throw runtime_error("No parent in filename for string: " + path);
    }

    return path.substr(next_pos, pos);
}

string realpath(const string &path) {
	char buf[PATH_MAX];
	if (realpath(path.c_str(), buf) == NULL) {
        throw runtime_error("realpath failed for path: " + path);
    }
	return string(buf);
}
