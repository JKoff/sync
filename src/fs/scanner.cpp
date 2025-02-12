#include "scanner.h"
#include "../util/log.h"

using namespace std;


////////////////
// scanSingle //
////////////////

void scanSingle(const std::filesystem::path &path, function<void (const FileRecord&)> callback) {
    try {
        File f(path);
        FileRecord filerec(f);
        callback(filerec);
    } catch (does_not_exist_error e) {
        // Sometimes a file is gone by the time we get to it, and that's fine.
        FileRecord filerec(FileRecord::Type::DOES_NOT_EXIST, NULL_HASH, path);
        callback(filerec);
    } catch (const exception &e) {
        throw runtime_error("Error while scanning " + path.string() + ": " + e.what());
    }
}


/////////////////////
// performFullScan //
/////////////////////

void performFullScan(
    const Directory &dir,
    struct dirent *dp,
    function<void (const FileRecord&)> callback,
    function<bool (const std::filesystem::path &)> filterFn
);

void processFile(
    const File &f,
    function<void (const FileRecord&)> callback,
    function<bool (const std::filesystem::path &)> filterFn
) {
    if (!filterFn(f.path)) {
        return;
    }

    FileRecord filerec(f);
    callback(filerec);
    
    if (f.isDir()) {
        Directory subdir(f);
        subdir.forEach([&callback, &filterFn] (const std::filesystem::directory_entry& entry) {
            performFullScan(entry.path(), callback, filterFn);
        });
    }
}

void performFullScan(
    const std::filesystem::path &path,
    function<void (const FileRecord&)> callback,
    function<bool (const std::filesystem::path &)> filterFn
) {
    if (!filterFn(path)) {
        return;
    }

    try {
        File f(path);
        processFile(f, callback, filterFn);
    } catch (does_not_exist_error e) {
        // Sometimes a file is gone by the time we get to it, and that's fine.
        FileRecord filerec(FileRecord::Type::DOES_NOT_EXIST, NULL_HASH, path);
        callback(filerec);
    }
}
