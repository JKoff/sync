#ifndef NET_PROTOCOL_H
#define NET_PROTOCOL_H

// #include <cassert>
// #include <functional>
#include <istream>
// #include <map>
#include <ostream>
#include <string>
// #include <typeindex>
// #include <typeinfo>
#include <vector>
#include "../process/policy/policy.h"
#include "protocol-interface.h"
#include "../util/max-size-buffer.h"

namespace MSG {
	/**
	 * Want to add a message?
	 * 1. Add to Type in protocol-interface.h
	 * 2. Add to this header
	 * 3. Register with Factory in protocol.cpp
	 */

	struct InfoReq : Base {
		virtual void serialize(std::ostream &stream) const { }
		virtual void deserialize(std::istream &stream) { }
	};

	struct InfoResp : Base {
		struct Response {
			std::string instanceId;
			std::string status;
			uint64_t filesIndexed;
			uint64_t hash;

			void serialize(std::ostream &stream) const {
				::serialize(stream, this->instanceId);
				::serialize(stream, this->status);
				::serialize(stream, this->filesIndexed);
				::serialize(stream, this->hash);
			}
			void deserialize(std::istream &stream) {
				::deserialize(stream, this->instanceId);
				::deserialize(stream, this->status);
				::deserialize(stream, this->filesIndexed);
				::deserialize(stream, this->hash);
			}
		};

		std::vector<Response> payloads;

		virtual void serialize(std::ostream &stream) const {
			::serialize(stream, this->payloads);
		}
		virtual void deserialize(std::istream &stream) {
			::deserialize(stream, this->payloads);
		}
	};

	/**
	 * Diff protocol:
	 * - DiffReqs get sent by primary, establishing merkle tree nodes with associated hashes.
	 * - DiffResps are sent by replica, so primary can know where to recurse.
	 * - DiffCommit is sent by primary, so that replica can draw conclusions from absence of
	 *   a path in DiffReqs (i.e. path doesn't doesn't exist).
	 * - All of the above include a txnid for the replica's bookkeeping. Accounting for this
	 *   txnid is the primary's responsibility. This avoids coordination difficulties, since
	 *   there can only be one primary, and the primary will know when a diff txn has started.
	 */

	struct DiffReq : Base {
		static const uint32_t MAX_RECORDS = 256;
		
		struct Query {
			std::string path;
			uint64_t hash;

			void serialize(std::ostream &stream) const {
				::serialize(stream, path);
				::serialize(stream, hash);
			}
			void deserialize(std::istream &stream) {
				::deserialize(stream, path);
				::deserialize(stream, hash);
			}
		};

		uint64_t epoch;
		std::vector<Query> queries;

		virtual void serialize(std::ostream &stream) const {
			::serialize(stream, this->epoch);
			::serialize(stream, this->queries);
		}
		virtual void deserialize(std::istream &stream) {
			::deserialize(stream, this->epoch);
			::deserialize(stream, this->queries);
		}
	};

	struct DiffResp : Base {
		struct Answer {
			std::string path;

			void serialize(std::ostream &stream) const {
				::serialize(stream, this->path);
			}
			void deserialize(std::istream &stream) {
				::deserialize(stream, this->path);
			}
		};

		// |answers| <= |queries| since answers only includes paths with non-matching hashes.
		std::vector<Answer> answers;

		virtual void serialize(std::ostream &stream) const {
			::serialize(stream, this->answers);
		}
		virtual void deserialize(std::istream &stream) {
			::deserialize(stream, this->answers);
		}
	};

	struct DiffCommit : Base {
		uint64_t epoch;

		virtual void serialize(std::ostream &stream) const {
			::serialize(stream, this->epoch);
		}
		virtual void deserialize(std::istream &stream) {
			::deserialize(stream, this->epoch);
		}
	};

	/**
	 * We want to start a transfer. Can include further steps (i.e. transfer to 1 or more
	 * peers, as we transfer to you.)
	 */
	struct XfrEstablishReq : Base {
		PolicyPlan plan;

		virtual void serialize(std::ostream &stream) const {
			::serialize(stream, this->plan);
		}
		virtual void deserialize(std::istream &stream) {
			::deserialize(stream, this->plan);
		}
	};

	/**
	 * Transfer data block.
	 * Closing protocol is that primary sends non-full block to replica.
	 */
	struct XfrBlock : Base {
		static const uint32_t MAX_SIZE = 32 * 1024;

		MaxSizeBuffer<MAX_SIZE> data;

		virtual void serialize(std::ostream &stream) const {
			::serialize(stream, this->data);
		}
		virtual void deserialize(std::istream &stream) {
			::deserialize(stream, this->data);
		}
	};

	struct SyncEstablishReq : Base {
		virtual void serialize(std::ostream &stream) const {}
		virtual void deserialize(std::istream &stream) {}
	};

	struct FullsyncCmd : Base {
		virtual void serialize(std::ostream &stream) const {}
		virtual void deserialize(std::istream &stream) {}
	};

	struct FlushCmd : Base {
		virtual void serialize(std::ostream &stream) const {}
		virtual void deserialize(std::istream &stream) {}
	};

	struct InspectReq : Base {
		std::string path;

		virtual void serialize(std::ostream &stream) const {
			::serialize(stream, this->path);
		}
		virtual void deserialize(std::istream &stream) {
			::deserialize(stream, this->path);
		}
	};

	struct InspectResp : Base {
		struct Child {
			std::string path;
			uint64_t hash;

			void serialize(std::ostream &stream) const {
				::serialize(stream, this->path);
				::serialize(stream, this->hash);
			}
			void deserialize(std::istream &stream) {
				::deserialize(stream, this->path);
				::deserialize(stream, this->hash);
			}
		};

		std::string path;
		uint64_t hash;
		std::vector<Child> children;

		virtual void serialize(std::ostream &stream) const {
			::serialize(stream, this->path);
			::serialize(stream, this->hash);
			::serialize(stream, this->children);
		}
		virtual void deserialize(std::istream &stream) {
			::deserialize(stream, this->path);
			::deserialize(stream, this->hash);
			::deserialize(stream, this->children);
		}
	};
}

#endif
