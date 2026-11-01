// Thumbnail generation (decoding + high quality downscaling).
#pragma once

#include <cairo.h>

#include <string>

namespace qimgv {

// Returns a new surface whose long side is `size` (or a size x size square crop), or nullptr.
cairo_surface_t* generateThumbnail(const std::string& path, int size, bool square, std::string* err = nullptr);
// Scale an existing surface the same way.
cairo_surface_t* thumbnailFromSurface(cairo_surface_t* src, int size, bool square);

}  // namespace qimgv
