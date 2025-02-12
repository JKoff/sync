#ifndef FS_TYPES_H
#define FS_TYPES_H

#include <dirent.h>
#include <filesystem>
#include <functional>
#include <string>
#include <sys/stat.h>
#include "../util/serialize.h"

//////////////////
// Public Types //
//////////////////

typedef uint64_t HashT;
typedef std::filesystem::path Relpath;
typedef std::filesystem::path Abspath;
const HashT NULL_HASH = 0;

class File;

class FileRecord {
public:
	enum class Type : uint8_t {
		FILE,
		DIRECTORY,
		SYMLINK,
		DOES_NOT_EXIST
	};

	FileRecord(const File &f);
	FileRecord(Type type, HashT version, Abspath path, std::filesystem::perms mode=std::filesystem::perms::none);

	Type type;
	std::filesystem::perms mode;
	HashT version;
	Abspath path;
	std::filesystem::path targetPath;  // only for symlinks
};

std::ostream& operator<<(std::ostream &os, const FileRecord::Type &type);
std::wostream& operator<<(std::wostream &os, const FileRecord::Type &type);

void serialize(std::ostream &stream, const FileRecord::Type &val);
void deserialize(std::istream &stream, FileRecord::Type &val);


class Directory {
	bool exhausted;
public:
	Directory() = delete;
	Directory(const Directory &that) = delete;
	Directory& operator=(const Directory &that) = delete;

	Directory(const File &f);
	~Directory();
	void forEach(std::function<void (const std::filesystem::directory_entry&)> f);
	void remove();

	std::filesystem::path path;
};

class File {
	void init(const std::filesystem::path& path);
public:
	File() = delete;
	File(const File &that) = delete;
	File& operator=(const File &that) = delete;

	File(const std::filesystem::path& path);

	HashT hash() const;
	bool isDir() const;
	bool isLink() const;
	void remove();

	~File();

	// int fd;
	// std::wstring path;
	std::filesystem::path path;
    std::filesystem::file_status statbuf;
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
