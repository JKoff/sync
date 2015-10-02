#include "types.h"

#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <unistd.h>

#include "retter/xxHash/xxhash.h"

using namespace std;


////////////////
// FileRecord //
////////////////

FileRecord::FileRecord(const File &f) {
    HashT version = NULL_HASH;

    FileRecord::Type type;
    if (S_ISDIR(f.statbuf.st_mode)) {
        type = Type::DIRECTORY;
        version = 0;
    } else if (S_ISREG(f.statbuf.st_mode)) {
        type = Type::FILE;
        version = f.hash();
    } else {
        throw runtime_error("Unknown stat type.");
    }

    this->path = f.path;
    this->type = type;
    this->mode = f.statbuf.st_mode;
    this->version = version;
}

// No validation takes place in this case.
FileRecord::FileRecord(Type type, HashT version, std::string path, mode_t mode)
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

Directory::Directory(const File &f) : exhausted(false) {
    DIR *d = fdopendir(f.fd);
    if (d == nullptr) {
        throw runtime_error("Could not turn fd into DIR*.");
    }

    // Commit
    this->d = d;
    this->fd = f.fd;
    this->path = f.path;
}

Directory::~Directory() {
    closedir(this->d);
}

void Directory::forEach(function<void (struct dirent*)> f) {
    // Have we already done a forEach?
    // Current code doesn't support rewind, so...
    if (this->exhausted) {
        throw runtime_error("Cannot call Directory forEach more than once.");
    }
    this->exhausted = true;

    struct dirent *dp;

    while ((dp = readdir(this->d)) != nullptr) {
    	if (dp->d_name[0] == '.') {
	        if (dp->d_name[1] == 0) {
	            continue;
	        } else if (dp->d_name[1] == '.' || dp->d_name[2] == 0) {
	            continue;
	        }
	    }

        f(dp);
    }
}

void Directory::remove() {
	this->forEach([this] (struct dirent *dp) {
        // Possible race condition in condition since filename might change.
        // This is fine as long as we remember to start listening for
        // change events before the initial traverse starts.
        File f(*this, dp->d_name);
        f.remove();
	});

	rmdir(this->path.c_str());
}


//////////
// File //
//////////

File::File(const char *path) {
    this->init(open(path, O_RDONLY), path);
}
File::File(const string &path) {
    this->init(open(path.c_str(), O_RDONLY), path);
}
File::File(const Directory &dir, const char *name) {
    stringstream ss;
    ss << dir.path << "/" << name;

    this->init(openat(dir.fd, name, O_RDONLY), ss.str());
}

void File::init(int fd, string path) {
    if (fd == -1) {
        throw does_not_exist_error("Could not open file from path: " + path);
    }

    struct stat statbuf;
    if (fstat(fd, &statbuf) != 0) {
        throw runtime_error("Could not stat file: " + path);
    }

    // Commit
    this->fd = fd;
    this->statbuf = statbuf;
    this->path = path;
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
        throw runtime_error("Could not reset xxhash state for file: " + this->path);
    }

    f.seekg(0, f.end);
    int len = f.tellg();
    f.seekg(0, f.beg);

    while (len) {
        size_t sz = len > BLOCK_SIZE ? BLOCK_SIZE : len;
        f.read((char*)buf, sz);

        if (XXH64_update(&st, buf, sz) == XXH_ERROR) {
            throw runtime_error("Could not update xxhash for file: " + this->path);
        }
        
        len -= sz;
    }

    HashT result = XXH64_digest(&st);

    return result;
}

bool File::isDir() const {
    return S_ISDIR(this->statbuf.st_mode);
}

void File::remove() {
    if (this->isDir()) {
        Directory subdir(*this);
        subdir.remove();
    } else {
    	unlink(this->path.c_str());
    }
}

File::~File() {
    close(this->fd);
}
