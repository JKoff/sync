#include "protocol-interface.h"

std::ostream& operator<<(std::ostream &stream, const MSG::Type &type) {
	switch (type) {
	case MSG::Type::UNSET:              return stream << "UNSET";
	case MSG::Type::COMPRESSED:         return stream << "COMPRESSED";
	case MSG::Type::INFO_REQ:           return stream << "INFO_REQ";
	case MSG::Type::INFO_RESP:          return stream << "INFO_RESP";
	case MSG::Type::DIFF_REQ:           return stream << "DIFF_REQ";
	case MSG::Type::DIFF_RESP:          return stream << "DIFF_RESP";
	case MSG::Type::DIFF_COMMIT:        return stream << "DIFF_COMMIT";
	case MSG::Type::XFR_ESTABLISH_REQ:  return stream << "XFR_ESTABLISH_REQ";
	case MSG::Type::XFR_BLOCK:          return stream << "XFR_BLOCK";
	case MSG::Type::SYNC_ESTABLISH_REQ: return stream << "SYNC_ESTABLISH_REQ";
	case MSG::Type::FULLSYNC_CMD:       return stream << "FULLSYNC_CMD";
	case MSG::Type::FLUSH_CMD:          return stream << "FLUSH_CMD";
	case MSG::Type::INSPECT_REQ:        return stream << "INSPECT_REQ";
	case MSG::Type::INSPECT_RESP:       return stream << "INSPECT_RESP";
	case MSG::Type::LOG_REQ:            return stream << "LOG_REQ";
	case MSG::Type::LOG_RESP:           return stream << "LOG_RESP";
	}
	return stream;
}

std::wostream& operator<<(std::wostream &stream, const MSG::Type &type) {
	switch (type) {
	case MSG::Type::UNSET:              return stream << "UNSET";
	case MSG::Type::COMPRESSED:         return stream << "COMPRESSED";
	case MSG::Type::INFO_REQ:           return stream << "INFO_REQ";
	case MSG::Type::INFO_RESP:          return stream << "INFO_RESP";
	case MSG::Type::DIFF_REQ:           return stream << "DIFF_REQ";
	case MSG::Type::DIFF_RESP:          return stream << "DIFF_RESP";
	case MSG::Type::DIFF_COMMIT:        return stream << "DIFF_COMMIT";
	case MSG::Type::XFR_ESTABLISH_REQ:  return stream << "XFR_ESTABLISH_REQ";
	case MSG::Type::XFR_BLOCK:          return stream << "XFR_BLOCK";
	case MSG::Type::SYNC_ESTABLISH_REQ: return stream << "SYNC_ESTABLISH_REQ";
	case MSG::Type::FULLSYNC_CMD:       return stream << "FULLSYNC_CMD";
	case MSG::Type::FLUSH_CMD:          return stream << "FLUSH_CMD";
	case MSG::Type::INSPECT_REQ:        return stream << "INSPECT_REQ";
	case MSG::Type::INSPECT_RESP:       return stream << "INSPECT_RESP";
	case MSG::Type::LOG_REQ:            return stream << "LOG_REQ";
	case MSG::Type::LOG_RESP:           return stream << "LOG_RESP";
	}
	return stream;
}
