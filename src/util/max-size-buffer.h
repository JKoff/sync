#ifndef UTIL_MAX_SIZE_BUFFER_H
#define UTIL_MAX_SIZE_BUFFER_H

#include <cassert>
#include <memory>

// Like a vector, except always allocates the max given amount of memory.
// Resize only affects size variable——no allocations or constructors.

template <size_t Size>
class MaxSizeBuffer {
public:
	MaxSizeBuffer() : _buf(new uint8_t[Size]), _allocated(Size), _size(0) {}
	MaxSizeBuffer(const MaxSizeBuffer&) = delete;
	MaxSizeBuffer& operator=(const MaxSizeBuffer&) = delete;

	uint8_t *data() const { return _buf.get(); }
	size_t size() const { return _size; }
	void resize(size_t size) {
		assert(size <= _allocated);
		_size = size;
	}

private:
	std::unique_ptr<uint8_t> _buf;
	size_t _allocated;
	size_t _size;
};

#endif
