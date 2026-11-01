// Desktop integration: wallpaper, file manager, user scripts.
#pragma once

#include <string>

#include "settings.hpp"

namespace qimgv {

// Returns a short status message for the user.
bool setWallpaper(const std::string& path, std::string& message);
void showInFileManager(const std::string& path);
bool runScript(const Script& script, const std::string& path, std::string& err);

}  // namespace qimgv
