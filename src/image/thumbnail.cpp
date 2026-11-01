#include "image/thumbnail.hpp"

#include <algorithm>
#include <cmath>

#include "image/decoder.hpp"

namespace qimgv {

cairo_surface_t* thumbnailFromSurface(cairo_surface_t* src, int size, bool square) {
    int w = cairo_image_surface_get_width(src), h = cairo_image_surface_get_height(src);
    if(w <= 0 || h <= 0) return nullptr;
    if(square) {
        int side = std::min(w, h);
        cairo_surface_t* crop = cropSurface(src, (w - side) / 2, (h - side) / 2, side, side);
        if(!crop) return nullptr;
        cairo_surface_t* r = side > size ? scaleSurface(crop, size, size) : cairo_surface_reference(crop);
        cairo_surface_destroy(crop);
        return r;
    }
    if(std::max(w, h) <= size) return cropSurface(src, 0, 0, w, h);
    double k = (double)size / std::max(w, h);
    return scaleSurface(src, std::max(1, (int)std::lround(w * k)), std::max(1, (int)std::lround(h * k)));
}

cairo_surface_t* generateThumbnail(const std::string& path, int size, bool square, std::string* err) {
    DecodeOptions opt;
    opt.firstFrameOnly = true;
    opt.targetSize = size;
    ImagePtr img = decodeFile(path, opt, err);
    if(!img) return nullptr;
    return thumbnailFromSurface(img->surface(0), size, square);
}

}  // namespace qimgv
