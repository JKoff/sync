#ifndef UTIL_SERIALIZE_H
#define UTIL_SERIALIZE_H

#include <istream>
#include <iostream>
#include <map>
#include <ostream>
#include <string>
#include <thread>
#include <vector>
#include "max-size-buffer.h"

namespace MSG {
	enum class Type : uint8_t;
}


/////////////////////
// Primitive types //
/////////////////////

void serialize(std::ostream &stream, const uint8_t &val);
void deserialize(std::istream &stream, uint8_t &val);

void serialize(std::ostream &stream, const uint16_t &val);
void deserialize(std::istream &stream, uint16_t &val);

void serialize(std::ostream &stream, const uint32_t &val);
void deserialize(std::istream &stream, uint32_t &val);

void serialize(std::ostream &stream, const uint64_t &val);
void deserialize(std::istream &stream, uint64_t &val);

void serialize(std::ostream &stream, const int64_t &val);
void deserialize(std::istream &stream, int64_t &val);

void serialize(std::ostream &stream, const bool &val);
void deserialize(std::istream &stream, bool &val);


///////////
// Enums //
///////////

void serialize(std::ostream &stream, const MSG::Type &type);
void deserialize(std::istream &stream, MSG::Type &type);


/////////////////////
// Composite types //
/////////////////////

void serialize(std::ostream &stream, const std::string &str);
void deserialize(std::istream &stream, std::string &str);

void serialize(std::ostream &stream, const std::vector<uint8_t> &vec);
void deserialize(std::istream &stream, std::vector<uint8_t> &vec);


///////////////////
// Generic types //
///////////////////

template <size_t Size>
void serialize(std::ostream &stream, const MaxSizeBuffer<Size> &vec) {
    uint64_t size = vec.size();
    serialize(stream, size);

    stream.write((char*)vec.data(), size);
}

template <size_t Size>
void deserialize(std::istream &stream, MaxSizeBuffer<Size> &vec) {
    uint64_t size;
    deserialize(stream, size);

    vec.resize(size);
    stream.read((char*)vec.data(), size);
}

// Classes/structs with serialize/deserialize methods

template <typename T>
void serialize(std::ostream &stream, T *obj) {
    obj->serialize(stream);
}

template <typename T>
void deserialize(std::istream &stream, T *obj) {
    obj->deserialize(stream);
}


template <typename T>
void serialize(std::ostream &stream, const T &obj) {
	obj.serialize(stream);
}

template <typename T>
void deserialize(std::istream &stream, T &obj) {
    obj.deserialize(stream);
}


template <typename T>
void serialize(std::ostream &stream, const std::vector<T> &vec) {	
    uint64_t sz = vec.size();
    serialize(stream, sz);

    for (const T &el : vec) {
    	serialize(stream, el);
    }
}

template <typename T>
void deserialize(std::istream &stream, std::vector<T> &vec) {
    uint64_t sz;
    deserialize(stream, sz);
    vec.resize(sz);

    for (int i=0; i < sz; i++) {
    	deserialize(stream, vec[i]);
    }
}


template <typename T, typename U>
void serialize(std::ostream &stream, const std::pair<T, U> &p) {   
    serialize(stream, p.first);
    serialize(stream, p.second);
}

template <typename T, typename U>
void deserialize(std::istream &stream, std::pair<T, U> &p) {
    deserialize(stream, p.first);
    deserialize(stream, p.second);
}


template <typename T, typename U>
void serialize(std::ostream &stream, const std::map<T, U> &m) {   
    uint64_t sz = m.size();
    serialize(stream, sz);

    for (const auto &el : m) {
        serialize(stream, el);
    }
}

template <typename T, typename U>
void deserialize(std::istream &stream, std::map<T, U> &m) {
    uint64_t sz;
    deserialize(stream, sz);

    for (int i=0; i < sz; i++) {
        std::pair<T, U> p;
        deserialize(stream, p);
        m.emplace(p);
    }
}

#endif
