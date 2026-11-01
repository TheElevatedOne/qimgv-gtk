// Saving edited images. Supports JPEG, PNG, WebP, JPEG XL and AVIF (chosen by extension).
#pragma once

#include <cairo.h>

#include <string>

namespace qimgv {

bool saveSurface(cairo_surface_t* s, const std::string& path, int quality, std::string& err);

}  // namespace qimgv
