// File operations (GIO based).
#pragma once

#include <string>

namespace qimgv {

enum class FileOpResult { Ok, Exists, Error };

// Copy / move `src` into directory `dstDir`. With overwrite=false, Exists is returned if the target exists.
FileOpResult copyToDir(const std::string& src, const std::string& dstDir, bool overwrite, std::string& err);
FileOpResult moveToDir(const std::string& src, const std::string& dstDir, bool overwrite, std::string& err);
bool moveToTrash(const std::string& path, std::string& err);
bool deleteFile(const std::string& path, std::string& err);
// Rename within the same directory. `newName` is a file name, not a path.
FileOpResult renameFile(const std::string& path, const std::string& newName, bool overwrite, std::string& newPath,
                        std::string& err);
std::string baseName(const std::string& path);
std::string dirName(const std::string& path);
std::string joinPath(const std::string& dir, const std::string& name);
bool isDirectory(const std::string& path);
bool isWritableDir(const std::string& path);
std::string humanSize(long long bytes);

}  // namespace qimgv
