#ifndef INDEX_H
#define INDEX_H

#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <string>

#include "fs/scanner.h"
#include "process/policy/policy.h"

class Index {
	struct IndexEntry {
		// Filesystem structure
		FileRecord::Type type;
		std::filesystem::perms mode;
		HashT version = 0;
		std::set<Relpath> children;
		std::filesystem::path targetPath;  // for symlinks

		// Merkle tree node value
		HashT hash;

		// Needed by replica for diffing
		uint64_t epoch;

		// Useful for troubleshooting
		HashT expectedHash;
	};

public:
	Index() = delete;
	Index(const Abspath &root);
	void update(const FileRecord &rec);
	HashT hash(Relpath path=L"");
	size_t size();
	~Index();

	// For optimized full rebuild
	void rebuildBlock(std::function<void ()> fn);
	// For diffing two indexes
	void diff(
		std::function<std::deque<Relpath> (const std::deque<Relpath> &)> oracleFn,
		std::function<void (const PolicyFile &)> emitFn
	);

	/////////////////////////////////
	// Used by replica for diffing //
	/////////////////////////////////
	void setEpoch(const Relpath &path, uint64_t epoch);
	// returns list of files to delete
	std::list<Abspath> commit(uint64_t epoch);


	//////////////////////////////
	// Used for troubleshooting //
	//////////////////////////////
	std::set<Relpath/*path*/> children(const Relpath &path);
	HashT expectedHash(const Relpath &path);
	void setExpectedHash(const Relpath &path, HashT expectedHash);

private:
	// Update Merkle tree upward from path.
	void updateHash(const Relpath &path);
	// Optimized full index rebuild, for use after updateDefer.
	void rebuildIndex(const Relpath &path);

	// Traversal function returns false to prevent recursion into a branch.
	void forEach(
		const Relpath &path,
		std::function<bool (const Relpath &, const IndexEntry &)> fn
	);

	Abspath root;
	std::map<Relpath, IndexEntry> paths;
	std::recursive_mutex stateMutex;
	bool rebuildInProgress = false;
    //leveldb::DB* db;
};

#endif
