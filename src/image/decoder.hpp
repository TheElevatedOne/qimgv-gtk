// Image decoding front end. All functions are thread safe and may run on worker threads.
#pragma once

#include <string>

#include "image/formats.hpp"
#include "image/image.hpp"

namespace qimgv {

struct DecodeOptions {
    bool firstFrameOnly = false;  // thumbnails: skip animation frames
    int targetSize = 0;           // >0: decoder may return a smaller image (>= targetSize on the long side)
    bool rawUseEmbeddedPreview = false;
    bool animations = true;       // decode animated JXL/AVIF/WebP/GIF/APNG frames
};

ImagePtr decodeFile(const std::string& path, const DecodeOptions& opt, std::string* error = nullptr);

// Format specific decoders (dec_*.cpp). They fill `img` and return false + error on failure.
bool decodeJpeg(const std::string& path, const DecodeOptions& opt, ImageData& img, std::string& err);
bool decodePng(const std::string& path, const DecodeOptions& opt, ImageData& img, std::string& err);
bool decodeGif(const std::string& path, const DecodeOptions& opt, ImageData& img, std::string& err);
bool decodeWebp(const std::string& path, const DecodeOptions& opt, ImageData& img, std::string& err);
bool decodeJxl(const std::string& path, const DecodeOptions& opt, ImageData& img, std::string& err);
bool decodeAvif(const std::string& path, const DecodeOptions& opt, ImageData& img, std::string& err);
bool decodeHeif(const std::string& path, const DecodeOptions& opt, ImageData& img, std::string& err);
bool decodeRaw(const std::string& path, const DecodeOptions& opt, ImageData& img, std::string& err);
bool decodePixbuf(const std::string& path, const DecodeOptions& opt, ImageData& img, std::string& err);

// Helpers shared by decoders.
bool readWholeFile(const std::string& path, std::string& out);
// Decode a JPEG held in memory into a surface (used by the raw decoder for embedded previews).
cairo_surface_t* decodeJpegMemory(const unsigned char* data, size_t size, int targetSize, std::string& err);

}  // namespace qimgv
