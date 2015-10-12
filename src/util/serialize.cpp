#include "serialize.h"

#include <cassert>


template <typename T>
void serializeInteger(std::ostream &stream, const T &val) {
	size_t sz = sizeof val;
	// std::cout << "Serializing " << val << " of size " << sz << std::endl;
	for (int i=sz-1; i >= 0; i--) {
		// MSB to LSB
		int offset = i * 8;
		T mask = static_cast<T>(0xFF) << offset;
		uint8_t byte = (val & mask) >> (i * 8);
		stream.write((char*)&byte, 1);
		// std::cout << "Byte " << std::hex << std::showbase << static_cast<uint64_t>(byte) << "." << std::endl;
	}
}

template <typename T>
T deserializeInteger(std::istream &stream) {
	T val = 0;
	size_t sz = sizeof val;
	for (int i=0; i < sz; i++) {
		// MSB to LSB
		// First LSB will eventually become MSB
		uint8_t lsb;
		stream.read((char*)&lsb, 1);
		// LOG("Byte[in] " << std::hex << std::showbase << static_cast<uint64_t>(lsb) << ".");
		val = (val << 8) | lsb;
	}
	// LOG("Deserialized " << val << " of size " << sz);
	return val;
}


void serialize(std::ostream &stream, const uint8_t &val) {
	stream.write((char*)&val, 1);
}

void deserialize(std::istream &stream, uint8_t &val) {
	stream.read((char*)&val, 1);
}


void serialize(std::ostream &stream, const uint16_t &val) {
	serializeInteger(stream, val);
}

void deserialize(std::istream &stream, uint16_t &val) {
	val = deserializeInteger<uint16_t>(stream);
}


void serialize(std::ostream &stream, const uint32_t &val) {
	serializeInteger(stream, val);
}

void deserialize(std::istream &stream, uint32_t &val) {
	val = deserializeInteger<uint32_t>(stream);
}


void serialize(std::ostream &stream, const uint64_t &val) {
	serializeInteger(stream, val);
}

void deserialize(std::istream &stream, uint64_t &val) {
	val = deserializeInteger<uint64_t>(stream);
}


void serialize(std::ostream &stream, const int64_t &val) {
	serializeInteger(stream, static_cast<uint64_t>(val));
}

void deserialize(std::istream &stream, int64_t &val) {
	val = static_cast<int64_t>(deserializeInteger<uint64_t>(stream));
}


void serialize(std::ostream &stream, const bool &val) {
	serialize(stream, static_cast<uint8_t>(val));
}

void deserialize(std::istream &stream, bool &val) {
	uint8_t tmp;
	deserialize(stream, tmp);
	val = static_cast<bool>(tmp);
}


void serialize(std::ostream &stream, const std::string &str) {
    uint32_t sz = str.size();

    serialize(stream, sz);
    if (sz > 0) {
	    stream.write((char*)str.data(), sz);
	}
}

void deserialize(std::istream &stream, std::string &str) {
    uint32_t sz;
    deserialize(stream, sz);

    std::string s(sz, ' ');
    if (sz > 0) {
	    stream.read((char*)&(*s.begin()), sz);
	}
    swap(str, s);
}

void serialize(std::ostream &stream, const MSG::Type &type) {
	serialize(stream, static_cast<uint8_t>(type));
}

void deserialize(std::istream &stream, MSG::Type &type) {
	uint8_t tmp;
	deserialize(stream, tmp);
	type = static_cast<MSG::Type>(tmp);
}

void serialize(std::ostream &stream, const std::vector<uint8_t> &vec) {
	uint64_t size = vec.size();
	serialize(stream, size);

	stream.write((char*)vec.data(), size);
}

void deserialize(std::istream &stream, std::vector<uint8_t> &vec) {
	uint64_t size;
	deserialize(stream, size);

	vec.resize(size);
    stream.read((char*)vec.data(), size);
}
