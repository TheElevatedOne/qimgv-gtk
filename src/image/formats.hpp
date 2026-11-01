// Supported file formats and content sniffing.
#pragma once

#include <string>
#include <vector>

namespace qimgv {

enum class Format {
    Unknown,
    JPEG,
    PNG,   // includes APNG
    GIF,
    WebP,
    JXL,
    AVIF,
    HEIF,
    Raw,
    Pixbuf,  // anything else gdk-pixbuf can read (BMP, TIFF, SVG, ICO, TGA, QOI, ...)
};

std::string lowerExtension(const std::string& path);
// True for extensions of image formats qimgv opens.
bool isSupportedExtension(const std::string& ext);
// True for video containers. Videos are never listed or opened.
bool isVideoExtension(const std::string& ext);
bool isRawExtension(const std::string& ext);
// True if the file should appear when browsing a directory.
bool isSupportedImageFile(const std::string& path);
// Detect the decoder to use from the file header (falls back to the extension).
Format detectFormat(const std::string& path);
// All supported extensions (for file chooser filters).
const std::vector<std::string>& supportedExtensions();
// Extensions qimgv can save to.
bool canSaveExtension(const std::string& ext);

}  // namespace qimgv
