#ifndef FS_SCANNER_H
#define FS_SCANNER_H

#include <filesystem>
#include <string>

#include "types.h"

////////////////
// Public API //
////////////////

void performFullScan(
	const std::filesystem::path &path,
	std::function<void (const FileRecord&)> callback,
	std::function<bool (const std::filesystem::path &)> filterFn
);
void scanSingle(const std::filesystem::path &path, std::function<void (const FileRecord&)> callback);

#endif
