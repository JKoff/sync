#include "scanner.h"

using namespace std;


////////////////
// scanSingle //
////////////////

void scanSingle(const string &path, function<void (const FileRecord&)> callback) {
    try {
        File f(path.c_str());
        FileRecord filerec(f);
        callback(filerec);
    } catch (does_not_exist_error e) {
        // Sometimes a file is gone by the time we get to it, and that's fine.
        FileRecord filerec(FileRecord::Type::DOES_NOT_EXIST, NULL_HASH, path);
        callback(filerec);
    }
}


/////////////////////
// performFullScan //
/////////////////////

void performFullScan(
    const Directory &dir,
    struct dirent *dp,
    function<void (const FileRecord&)> callback,
    function<bool (const string &)> filterFn
);

void processFile(
    const File &f,
    function<void (const FileRecord&)> callback,
    function<bool (const string &)> filterFn
) {
    if (!filterFn(f.path)) {
        return;
    }

    FileRecord filerec(f);
    callback(filerec);
    
    if (f.isDir()) {
        Directory subdir(f);
        subdir.forEach([&callback, &subdir, &filterFn] (struct dirent *dp) {
            performFullScan(subdir, dp, callback, filterFn);
        });
    }
}

// Absolute version
void performFullScan(
    const string &path,
    function<void (const FileRecord&)> callback,
    function<bool (const string &)> filterFn
) {
    if (!filterFn(path)) {
        return;
    }

    try {
        File f(path.c_str());
        processFile(f, callback, filterFn);
    } catch (does_not_exist_error e) {
        // Sometimes a file is gone by the time we get to it, and that's fine.
        FileRecord filerec(FileRecord::Type::DOES_NOT_EXIST, NULL_HASH, path);
        callback(filerec);
    }
}

// Relative version
void performFullScan(
    const Directory &dir,
    struct dirent *dp,
    function<void (const FileRecord&)> callback,
    function<bool (const string &)> filterFn
) {
    // Sometimes you get a file with invalid characters.
    // This is a decision to ignore these rather than just fail.
    try {
        // Possible race condition in condition since filename might change.
        // This is fine as long as we remember to start listening for
        // change events before the initial traverse starts.
        File f(dir, dp->d_name);

        if (!filterFn(f.path)) {
            return;
        }

        processFile(f, callback, filterFn);
    } catch (does_not_exist_error e) {
        // Sometimes a file is gone by the time we get to it, and that's fine.
        FileRecord filerec(FileRecord::Type::DOES_NOT_EXIST, NULL_HASH, dir.path);
        callback(filerec);
    }
}
