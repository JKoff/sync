#ifndef NET_PROTOCOL_INTERFACE_H
#define NET_PROTOCOL_INTERFACE_H

#include <cassert>
#include <functional>
#include <istream>
#include <map>
#include <ostream>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <vector>
#include <stdio.h>

#include "../util/log.h"

namespace MSG {
	/**
	 * Every message has base class Base and memory layout:
	 * Header | [Compressed/InfoReq/InfoResp/...]
	 */

	enum class Type : uint8_t {
		UNSET              = 0,
		COMPRESSED         = 1,
		INFO_REQ           = 2,
		INFO_RESP          = 3,
		DIFF_REQ           = 4,
		DIFF_RESP          = 5,
		DIFF_COMMIT        = 6,
		XFR_ESTABLISH_REQ  = 7,
		XFR_BLOCK          = 8,
		SYNC_ESTABLISH_REQ = 9,
		FULLSYNC_CMD       = 10,
		FLUSH_CMD          = 11,
		INSPECT_REQ        = 12,
		INSPECT_RESP       = 13,
		LOG_REQ            = 14,
		LOG_RESP           = 15
	};

	struct Base {
		virtual ~Base() = default;

		// Serialize to stream
		virtual void serialize(std::ostream &stream) const = 0;
		// Deserialize from stream
		virtual void deserialize(std::istream &stream) = 0;

		Type type;
	};

	struct Factory {
		typedef std::function<std::unique_ptr<Base> ()> Constructor;

		template <typename T>
		static void Register(Type t) {
			Factory::constructors[t] = [] () {
				return std::unique_ptr<Base>(new T);
			};
			Factory::types[std::type_index(typeid(T))] = t;
		}
		
		static std::unique_ptr<Base> Create(Type t) {
			const auto &result = Factory::constructors.find(t);
			assert(result != Factory::constructors.end());
			return result->second();
		}

		template <typename T>
		static Type EnumType() {
			return Factory::types.at(std::type_index(typeid(T)));
		}

	private:
		static std::map<Type,Constructor> constructors;
		static std::map<std::type_index,Type> types;
	};

	template <typename T>
	struct FactoryRecord {
		FactoryRecord(Type t) {
			Factory::Register<T>(t);
		}
	};
}

std::ostream& operator<<(std::ostream &stream, const MSG::Type &type);

#endif
