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
	this->paths[""] = entry;
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

	Relpath path = rec.path.substr(this->root.length());
	if (path.length() == 0) {
		// Just ignore it...
		return;
	}

	Relpath parent = pathParent(path);
	if (parent.length() > 0 && this->paths.find(parent) == this->paths.end()) {
		// cout << "Ignoring " << rec.path << " because we don't have " << parent << " indexed." << endl;
		return;
	}

	this->paths[path].type = rec.type;

	switch (rec.type) {
	case FileRecord::Type::DOES_NOT_EXIST: {
		// The original is going to get mutated as we go down, breaking iteration.
		set<Relpath> childrenCopy = this->paths[path].children;
		for (const Relpath &child : childrenCopy) {
			FileRecord subrec(FileRecord::Type::DOES_NOT_EXIST, 0, this->root + child);
			this->update(subrec);
		}

		this->paths.erase(path);
		this->paths[parent].children.erase(path);
		break;
	} case FileRecord::Type::FILE:
	case FileRecord::Type::DIRECTORY:
		this->paths[path].mode = rec.mode;
		this->paths[path].version = rec.version;
		this->paths[parent].children.insert(path);
		break;
	}

	if (this->rebuildInProgress) {
		// In this case we'll perform an optimized full rebuild after the updates stop coming in.
	} else {
		this->updateHash(path);

		// Now that index is locally updated, make sure all parents' hashes are updated.
		list<pair<Relpath,Relpath>> parents = pathParents(path);
		// We can rely on pathParents' ordering guarantees (more nested -> less nested).
		for (auto parent_child : parents) {
			Relpath parent = parent_child.first;
			Relpath child = parent_child.second;

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
	this->rebuildIndex("");
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

	seen.push_back("");

	// For each level
	while (!seen.empty()) {
		// We need to filter seen to only contain paths that failed diff.
		seen = oracleFn(seen);

		// processing is empty
		swap(processing, seen);
		// now seen is empty, processing is full

		// For each item of said level
		for (; !processing.empty(); processing.pop_front()) {
			Relpath path = processing.front();
			
			PolicyFile policyFile = { path, this->paths[path].type };
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
	this->forEach("", [epoch,&result] (const Relpath &path, const IndexEntry &entry) {
		if (entry.epoch == epoch && entry.expectedHash == entry.hash) {
			// This node was a match, so all its descendents are fine.
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

	for (char c : str) {
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
