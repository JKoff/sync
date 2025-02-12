#include "types.h"

#include <errno.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string.h>
#include <unistd.h>

#include "retter/algorithms/xxHash/xxhash.h"

#include "../util/log.h"

using namespace std;

HashT HashString(const string &s) {
    XXH64_state_t st;
    if (XXH64_reset(&st, 0) == XXH_ERROR) {
        throw runtime_error("Could not reset xxhash state for string: " + s);
    }

    if (XXH64_update(&st, s.data(), s.size()) == XXH_ERROR) {
        throw runtime_error("Could not update xxhash for string: " + s);
    }
        
    return XXH64_digest(&st);
}


////////////////
// FileRecord //
////////////////

FileRecord::FileRecord(const File &f) {
    HashT version = NULL_HASH;

    FileRecord::Type type;
    if (std::filesystem::is_directory(f.statbuf)) {
        type = Type::DIRECTORY;
        version = 0;
    } else if (std::filesystem::is_regular_file(f.statbuf)) {
        type = Type::FILE;
        version = f.hash();
    } else if (std::filesystem::is_symlink(f.statbuf)) {
        type = Type::SYMLINK;
        this->targetPath = std::filesystem::read_symlink(f.path);
        version = HashString(this->targetPath.string());
    } else {
        throw runtime_error("Unknown stat type.");
    }

    this->path = f.path;
    this->type = type;
    this->mode = f.statbuf.permissions();
    this->version = version;
}

// No validation takes place in this case.
FileRecord::FileRecord(Type type, HashT version, Abspath path, std::filesystem::perms mode)
    : type(type), mode(mode), version(version), path(path) { }


//////////////////////
// FileRecord::Type //
//////////////////////

std::ostream& operator<<(std::ostream &os, const FileRecord::Type &type) {
    switch (type) {
        case FileRecord::Type::FILE:
            os << "FILE";
            break;
        case FileRecord::Type::DIRECTORY:
            os << "DIRECTORY";
            break;
        case FileRecord::Type::SYMLINK:
            os << "SYMLINK";
            break;
        case FileRecord::Type::DOES_NOT_EXIST:
            os << "GONE";
            break;
    }

    return os;
}

std::wostream& operator<<(std::wostream &os, const FileRecord::Type &type) {
    switch (type) {
        case FileRecord::Type::FILE:
            os << "FILE";
            break;
        case FileRecord::Type::DIRECTORY:
            os << "DIRECTORY";
            break;
        case FileRecord::Type::SYMLINK:
            os << "SYMLINK";
            break;
        case FileRecord::Type::DOES_NOT_EXIST:
            os << "GONE";
            break;
    }

    return os;
}

void serialize(std::ostream &stream, const FileRecord::Type &val) {
    serialize(stream, static_cast<uint8_t>(val));
}

void deserialize(std::istream &stream, FileRecord::Type &val) {
    uint8_t tmp;
    deserialize(stream, tmp);
    val = static_cast<FileRecord::Type>(tmp);
}

///////////////
// Directory //
///////////////

Directory::Directory(const File &f) : exhausted(false), path(f.path) { }

Directory::~Directory() {
}

void Directory::forEach(function<void (const std::filesystem::directory_entry&)> f) {
    // Have we already done a forEach?
    // Current code doesn't support rewind, so...
    if (this->exhausted) {
        throw runtime_error("Cannot call Directory forEach more than once.");
    }
    this->exhausted = true;

    for (auto const& entry : std::filesystem::directory_iterator{this->path}) {
        if (!entry.is_regular_file() && !entry.is_directory() && !entry.is_symlink()) {
            ERR("Found non-regular file in " << this->path << ": " << entry);
            StatusLine::Add("irregularFile", 1);
            continue;
        }
    	if (entry.path().filename() == "." || entry.path().filename() == "..") {
            continue;
	    }

        f(entry);
    }
}

void Directory::remove() {
	this->forEach([] (const std::filesystem::directory_entry& entry) {
        // Possible race condition in condition since filename might change.
        // This is fine as long as we remember to start listening for
        // change events before the initial traverse starts.
        File f(entry.path());
        f.remove();
	});

    std::filesystem::remove(this->path);
}


//////////
// File //
//////////

File::File(const std::filesystem::path& path) {
    this->init(path);
}

void File::init(const std::filesystem::path& path) {
    std::filesystem::file_status statbuf = std::filesystem::symlink_status(path);
    if (!std::filesystem::exists(statbuf)) {
        throw does_not_exist_error("File does not exist: " + path.string());
    }

    // Commit
    this->path = path;
    this->statbuf = statbuf;
}

// Hash timing:
// - no-op: 1.3s (0.863u+0.366s)
// - xxhash32: 13.125s (2.304u+2.278s)
// - xxhash64: sometimes same as xxhash32, sometimes 3.426s (1.895u+0.813s). puzzling.

HashT File::hash() const {
    const size_t BLOCK_SIZE = 1024;
    unsigned char buf[BLOCK_SIZE];

    ifstream f(this->path, ifstream::binary);

    XXH64_state_t st;
    if (XXH64_reset(&st, 0) == XXH_ERROR) {
        throw runtime_error("Could not reset xxhash state for file: " + this->path.string());
    }

    f.seekg(0, f.end);
    int len = f.tellg();
    f.seekg(0, f.beg);

    while (len) {
        size_t sz = len > BLOCK_SIZE ? BLOCK_SIZE : len;
        f.read((char*)buf, sz);

        if (XXH64_update(&st, buf, sz) == XXH_ERROR) {
            throw runtime_error("Could not update xxhash for file: " + this->path.string());
        }
        
        len -= sz;
    }

    HashT result = XXH64_digest(&st);

    return result;
}

bool File::isDir() const {
    return std::filesystem::is_directory(this->statbuf);
}

bool File::isLink() const {
    return std::filesystem::is_symlink(this->statbuf);
}

void File::remove() {
    if (this->isDir()) {
        Directory subdir(*this);
        subdir.remove();
    } else {
        std::filesystem::remove(this->path);
    }
}

File::~File() { }
