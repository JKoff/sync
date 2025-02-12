#include "index.h"

#include <exception>
#include <iostream>

#include "util.h"
#include "util/log.h"

using namespace std;

////////////
// Public //
////////////

Index::Index(const Abspath &root) : root(root) {
	IndexEntry entry;
	entry.type = FileRecord::Type::DIRECTORY;
	this->paths[L""] = entry;
}

Index::~Index() {
}

HashT Index::hash(Relpath path) {
	lock_guard<recursive_mutex> lock(this->stateMutex);
	return this->paths[path].hash;
}

size_t Index::size() {
	lock_guard<recursive_mutex> lock(this->stateMutex);
	return this->paths.size();
}

void Index::update(const FileRecord &rec) {
	lock_guard<recursive_mutex> lock(this->stateMutex);

	Relpath path = rec.path.lexically_relative(this->root);
	if (path.empty()) {
		// Just ignore it...
		return;
	}

	Relpath parent = path.parent_path();
	if (parent.empty() && this->paths.find(parent) == this->paths.end()) {
		// cout << "Ignoring " << rec.path << " because we don't have " << parent << " indexed." << endl;
		return;
	}

	this->paths[path].type = rec.type;

	switch (rec.type) {
	case FileRecord::Type::DOES_NOT_EXIST:
	{
		// The original is going to get mutated as we go down, breaking iteration.
		set<Relpath> childrenCopy = this->paths[path].children;
		for (const Relpath &child : childrenCopy) {
			FileRecord subrec(FileRecord::Type::DOES_NOT_EXIST, 0, this->root / child);
			this->update(subrec);
		}

		this->paths.erase(path);
		this->paths[parent].children.erase(path);
		break;
	}
	case FileRecord::Type::FILE:
	case FileRecord::Type::DIRECTORY:
	case FileRecord::Type::SYMLINK:
		// if (rand() % 100 == 0) {
		// 	LOG("Updating " << path << " with version " << rec.version << " and original path " << rec.path);
		// }
		this->paths[path].mode = rec.mode;
		this->paths[path].version = rec.version;
		this->paths[path].targetPath = rec.targetPath;  // only used by symlinks
		this->paths[parent].children.insert(path);
		break;
	}

	if (this->rebuildInProgress) {
		// In this case we'll perform an optimized full rebuild after the updates stop coming in.
	} else {
		this->updateHash(path);

		// Now that index is locally updated, make sure all parents' hashes are updated.
		list<std::filesystem::path> parents = pathParents(path);
		// We can rely on pathParents' ordering guarantees (more nested -> less nested).
		for (auto parent : parents) {
			this->updateHash(parent);
		}

		stringstream ss;
		ss << this->hash();
		STATUSGLOBAL("H(index)", this->hash());
		StatusLine::Set("|index|", this->size());
	}
}

void Index::rebuildBlock(std::function<void ()> fn) {
	lock_guard<recursive_mutex> lock(this->stateMutex);

	this->rebuildInProgress = true;
	fn();
	this->rebuildIndex(L"");
	this->rebuildInProgress = false;

	LOG("Rebuild completed with " << this->size() << " items and hash=" << this->hash());

	STATUSGLOBAL("H(index)", this->hash());
	StatusLine::Set("|index|", this->size());
}

void Index::diff(
	function<deque<Relpath> (const deque<Relpath> &)> oracleFn,
	function<void (const PolicyFile &)> emitFn
) {
	lock_guard<recursive_mutex> lock(this->stateMutex);

	deque<Relpath> seen;
	deque<Relpath> processing;

	seen.push_back(L"");

	// For each level
	while (!seen.empty()) {
		std::map<Relpath, int> tests; // 1 if diff was EQUIVALENT, 0 if diff was DIFFERENT
		for (Relpath path : seen) {
			tests[path]++;
		}

		// We need to filter seen to only contain paths that failed diff.
		seen = oracleFn(seen);

		for (Relpath path : seen) {
			tests[path]--;
		}
		
		for (const auto& [path, val] : tests) {
			if (val == 0) {
				// diff is DIFFERENT
				this->repeatOffenders[path]++;
			} else {
				// diff is EQUIVALENT
				this->repeatOffenders.erase(path);
			}
		}

		LOG("The following files failed diff more than once in a row: ");
		for (const auto& [path, val] : this->repeatOffenders) {
			LOG("  " << path << " failed " << val << " times.");
		}

		// processing is empty
		swap(processing, seen);
		// now seen is empty, processing is full

		// For each item of said level
		for (; !processing.empty(); processing.pop_front()) {
			Relpath path = processing.front();
			
			PolicyFile policyFile = { path, this->paths[path].targetPath, this->paths[path].type };
			emitFn(policyFile);

			// Push each child
			for (Relpath childKey : this->paths[path].children) {
				seen.push_back(childKey);
			}
		}
	}
}

void Index::setEpoch(const Relpath &path, uint64_t epoch) {
	lock_guard<recursive_mutex> lock(this->stateMutex);
	this->paths[path].epoch = epoch;
}

void Index::setExpectedHash(const Relpath &path, HashT expectedHash) {
	lock_guard<recursive_mutex> lock(this->stateMutex);
	this->paths[path].expectedHash = expectedHash;
}

HashT Index::expectedHash(const Relpath &path) {
	lock_guard<recursive_mutex> lock(this->stateMutex);
	return this->paths[path].expectedHash;
}

list<Relpath> Index::commit(uint64_t epoch) {
	lock_guard<recursive_mutex> lock(this->stateMutex);
	list<Relpath> result;
	this->forEach(L"", [epoch,&result] (const Relpath &path, const IndexEntry &entry) {
		if (entry.epoch == epoch && entry.expectedHash == entry.hash) {
			// This node was a match, so all its descendants are fine.
			return false;
		}

		if (entry.epoch != epoch) {
			result.push_front(path);
			return false;
		}

		return true;
	});
	return result;
}

set<Relpath> Index::children(const Relpath &path) {
	return this->paths[path].children;
}


/////////////
// Private //
/////////////

HashT hashCombine(HashT seed, const Relpath &str) {
	HashT result = seed;

	for (wchar_t c : str.wstring()) {
		result = result * 101 + c;
	}

	return result;
}

HashT hashCombine(HashT seed, HashT other) {
	return seed * 101 + other;
}

void Index::rebuildIndex(const Relpath &path) {
	// Pre-condition: we assume that a lock has already been captured by the caller.
	// Update a path's hash based on descendants' hashes, computing descendants' hashes
	// as it goes along.

	HashT result = 0;

	result = hashCombine(result, path);
	result = hashCombine(result, this->paths[path].version);

	for (Relpath childKey : this->paths[path].children) {
		this->rebuildIndex(childKey);

		result = hashCombine(result, childKey);
		result = hashCombine(result, this->paths[childKey].hash);
	}

	this->paths[path].hash = result;
}

void Index::updateHash(const Relpath &path) {
	// Pre-condition: we assume that a lock has already been captured by the caller.
	// Update a path's hash based on descendants' hashes.

	HashT result = 0;

	// We need to use a path relative to root so that hashes will be identical
	// on different replicas despite possibly-different roots.

	result = hashCombine(result, path);
	result = hashCombine(result, this->paths[path].version);

	for (Relpath childKey : this->paths[path].children) {
		result = hashCombine(result, childKey);
		result = hashCombine(result, this->paths[childKey].hash);
	}

	this->paths[path].hash = result;
}

void Index::forEach(const Relpath &path, function<bool (const Relpath &, const IndexEntry &)> fn) {
	if (!fn(path, this->paths[path])) {
		return;
	}

	for (Relpath childKey : this->paths[path].children) {
		this->forEach(childKey, fn);
	}
}
