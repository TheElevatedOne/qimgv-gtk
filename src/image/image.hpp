// Decoded image data shared between the loader, the cache and the viewer.
// Every frame is a cairo ARGB32 (premultiplied, native endian) image surface.
#pragma once

#include <cairo.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace qimgv {

struct Frame {
    cairo_surface_t* surface = nullptr;
    int delayMs = 0;  // only meaningful for animations
};

class ImageData {
public:
    ImageData() = default;
    ImageData(const ImageData&) = delete;
    ImageData& operator=(const ImageData&) = delete;
    ~ImageData();

    std::string path;
    std::string format;  // short format name, e.g. "JPEG", "JPEG XL", "APNG"
    int width = 0;
    int height = 0;
    int bitDepth = 8;     // bits per channel of the source
    bool hasAlpha = false;
    std::vector<Frame> frames;

    bool animated() const { return frames.size() > 1; }
    cairo_surface_t* surface(size_t i = 0) const { return i < frames.size() ? frames[i].surface : nullptr; }
    // Replace all frames (takes ownership of the new surfaces) and update the size.
    void setFrames(std::vector<Frame> f);
    std::shared_ptr<ImageData> clone() const;
};
using ImagePtr = std::shared_ptr<ImageData>;

// ---- pixel helpers --------------------------------------------------------
// Create a surface from straight-alpha RGBA8 pixels.
cairo_surface_t* surfaceFromRGBA(const uint8_t* rgba, int w, int h, size_t stride);
// Create a surface from RGB8 pixels.
cairo_surface_t* surfaceFromRGB(const uint8_t* rgb, int w, int h, size_t stride);
// Create a surface from straight-alpha BGRA8 pixels (premultiplies in place in the copy).
cairo_surface_t* surfaceFromBGRA(const uint8_t* bgra, int w, int h, size_t stride, bool premultiplied);
// Write the surface as straight-alpha RGBA8 / BGRA8 into a tightly packed buffer.
std::vector<uint8_t> surfaceToRGBA(cairo_surface_t* s, bool bgra = false);
bool surfaceHasAlpha(cairo_surface_t* s);

// Apply an EXIF orientation (1..8) and return a new surface (or a new reference for 1).
cairo_surface_t* applyOrientation(cairo_surface_t* s, int orientation);
// Geometric transforms (new surfaces).
cairo_surface_t* rotateSurface(cairo_surface_t* s, int degrees);  // 90, 180, 270
cairo_surface_t* flipSurface(cairo_surface_t* s, bool horizontal);
cairo_surface_t* cropSurface(cairo_surface_t* s, int x, int y, int w, int h);
// High quality scaling (box filter when downscaling).
cairo_surface_t* scaleSurface(cairo_surface_t* s, int w, int h, bool smooth = true);

}  // namespace qimgv
