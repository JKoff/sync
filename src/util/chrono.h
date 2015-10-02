#ifndef UTIL_CHRONO_H
#define UTIL_CHRONO_H

#include <chrono>
#include <sys/time.h>

template <typename T>
timeval chronoToTimeval(const std::chrono::duration<T> &d) {
	std::chrono::microseconds usec = std::chrono::duration_cast<std::chrono::microseconds>(d);
	timeval tv;
	tv.tv_sec = usec.count() / 1000000;
	tv.tv_usec = usec.count() % 1000000;
	return tv;
}

#endif
