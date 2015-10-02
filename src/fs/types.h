#ifndef FS_TYPES_H
#define FS_TYPES_H

#include <dirent.h>
#include <functional>
#include <string>
#include <sys/stat.h>
#include "../util/serialize.h"

//////////////////
// Public Types //
//////////////////

typedef uint64_t HashT;
typedef std::string Relpath;
typedef std::string Abspath;
const HashT NULL_HASH = 0;

class File;

class FileRecord {
public:
	enum class Type : uint8_t {
		FILE,
		DIRECTORY,
		DOES_NOT_EXIST
	};

	FileRecord(const File &f);
	FileRecord(Type type, HashT version, Abspath path, mode_t mode=0);

	Type type;
	mode_t mode;
	HashT version;
	Abspath path;
};

std::ostream& operator<<(std::ostream &os, const FileRecord::Type &type);

void serialize(std::ostream &stream, const FileRecord::Type &val);
void deserialize(std::istream &stream, FileRecord::Type &val);


class Directory {
	bool exhausted;
	DIR *d;
public:
	Directory() = delete;
	Directory(const Directory &that) = delete;
	Directory& operator=(const Directory &that) = delete;

	Directory(const File &f);
	~Directory();
	void forEach(std::function<void (struct dirent*)> f);
	void remove();

	int fd;
	std::string path;
};

class File {
	void init(int fd, std::string path);
public:
	File() = delete;
	File(const File &that) = delete;
	File& operator=(const File &that) = delete;

	// absolute constructor
	File(const char *path);
	File(const std::string &path);
	// relative constructor
	File(const Directory &dir, const char *name);

	HashT hash() const;
	bool isDir() const;
	void remove();

	~File();

	int fd;
	std::string path;
    struct stat statbuf;
};


///////////////////
// Private Types //
///////////////////

class does_not_exist_error : public std::runtime_error {
public:
    explicit does_not_exist_error(const std::string &str) : std::runtime_error(str) { }
    explicit does_not_exist_error(const char *str) : std::runtime_error(str) { }
};

#endif
