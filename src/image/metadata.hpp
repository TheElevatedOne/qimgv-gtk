// EXIF / XMP metadata via exiv2.
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace qimgv {

using InfoList = std::vector<std::pair<std::string, std::string>>;

// EXIF orientation (1..8), 1 when missing.
int readOrientation(const std::string& path);
// Human readable list of the most useful EXIF tags (camera, lens, exposure, date, GPS...).
InfoList readExifSummary(const std::string& path);
// Copy metadata from `src` to `dst` (after saving an edited image); resets the orientation.
void copyMetadata(const std::string& src, const std::string& dst);
// Silence exiv2's console output. Called once at startup.
void initMetadata();

}  // namespace qimgv
