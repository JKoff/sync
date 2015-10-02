#ifndef UTIL_ANY_H
#define UTIL_ANY_H

#include <memory>
#include <stdexcept>

/**
 * Only works on CopyConstructible types.
 */

class Any {
public:
	//////////////////
	// Needed types //
	//////////////////

	class TypeErased {
	public:
		virtual ~TypeErased() {}
	};

	template <typename T> class Holder : public TypeErased {
	public:
		Holder(const T &value) : held(value) {}
		T held;
	};


	/////////////
	// Members //
	/////////////

	Any() : content(nullptr) {}
	template <typename T> Any(const T &value) : content(new Holder<T>(value)) {}

	template <typename T> T cast() const {
		if (Holder<T> *holder = dynamic_cast<Holder<T>*>(this->content.get())) {
			return holder->held;
		} else {
			throw std::runtime_error("Could not cast Any.");
		}
	}

	std::unique_ptr<TypeErased> content;
};

#endif
