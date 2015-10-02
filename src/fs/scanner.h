#ifndef FS_SCANNER_H
#define FS_SCANNER_H

#include <string>

#include "types.h"

////////////////
// Public API //
////////////////

void performFullScan(
	const std::string &path,
	std::function<void (const FileRecord&)> callback,
	std::function<bool (const std::string &)> filterFn
);
void scanSingle(const std::string &path, std::function<void (const FileRecord&)> callback);

#endif
