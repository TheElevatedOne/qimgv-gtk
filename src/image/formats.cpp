#include "image/formats.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>

namespace qimgv {

static const std::vector<std::string> kRaw = {
    "3fr", "ari", "arw", "bay", "cap", "cr2", "cr3", "crw", "dcr", "dcs", "dng", "drf", "eip", "erf", "fff",
    "iiq", "k25", "kdc", "mdc", "mef", "mos", "mrw", "nef", "nrw", "orf", "pef", "ptx", "pxn", "r3d",
    "raf", "raw", "rw2", "rwl", "rwz", "sr2", "srf", "srw", "x3f"};

static const std::vector<std::string> kImage = {
    "jpg", "jpeg", "jpe", "jfif", "png", "apng", "gif", "webp", "jxl", "avif", "avifs", "heif", "heic",
    "heics", "heifs", "hif", "bmp", "dib", "ico", "cur", "icns", "tif", "tiff", "svg", "svgz", "tga",
    "qoi", "pnm", "pbm", "pgm", "ppm", "pam", "xpm", "xbm", "dds", "exr"};

static const std::set<std::string> kVideo = {
    "webm", "mp4", "m4v", "mpg", "mpeg", "mpe", "mkv", "wmv", "avi", "mov", "flv", "f4v", "ogv", "ogm",
    "3gp", "3g2", "ts", "mts", "m2ts", "vob", "gifv", "divx", "asf", "rm", "rmvb", "mxf"};

std::string lowerExtension(const std::string& path) {
    auto slash = path.find_last_of('/');
    auto dot = path.find_last_of('.');
    if(dot == std::string::npos || (slash != std::string::npos && dot < slash)) return {};
    std::string ext = path.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
    return ext;
}

const std::vector<std::string>& supportedExtensions() {
    static std::vector<std::string> all = [] {
        std::vector<std::string> v = kImage;
        v.insert(v.end(), kRaw.begin(), kRaw.end());
        return v;
    }();
    return all;
}

bool isSupportedExtension(const std::string& ext) {
    static std::set<std::string> s(supportedExtensions().begin(), supportedExtensions().end());
    return s.count(ext) > 0;
}

bool isVideoExtension(const std::string& ext) { return kVideo.count(ext) > 0; }

bool isRawExtension(const std::string& ext) {
    static std::set<std::string> s(kRaw.begin(), kRaw.end());
    return s.count(ext) > 0;
}

bool isSupportedImageFile(const std::string& path) {
    std::string ext = lowerExtension(path);
    return !isVideoExtension(ext) && isSupportedExtension(ext);
}

bool canSaveExtension(const std::string& ext) {
    return ext == "jpg" || ext == "jpeg" || ext == "jpe" || ext == "jfif" || ext == "png" || ext == "webp" ||
           ext == "jxl" || ext == "avif";
}

static Format fromExtension(const std::string& ext) {
    if(ext == "jpg" || ext == "jpeg" || ext == "jpe" || ext == "jfif") return Format::JPEG;
    if(ext == "png" || ext == "apng") return Format::PNG;
    if(ext == "gif") return Format::GIF;
    if(ext == "webp") return Format::WebP;
    if(ext == "jxl") return Format::JXL;
    if(ext == "avif" || ext == "avifs") return Format::AVIF;
    if(ext == "heif" || ext == "heic" || ext == "heics" || ext == "heifs" || ext == "hif") return Format::HEIF;
    if(isRawExtension(ext)) return Format::Raw;
    if(isSupportedExtension(ext)) return Format::Pixbuf;
    return Format::Unknown;
}

Format detectFormat(const std::string& path) {
    std::string ext = lowerExtension(path);
    if(isVideoExtension(ext)) return Format::Unknown;
    unsigned char h[64] = {0};
    size_t n = 0;
    if(FILE* f = fopen(path.c_str(), "rb")) {
        n = fread(h, 1, sizeof(h), f);
        fclose(f);
    } else {
        return Format::Unknown;
    }
    // Raw files are often TIFF based, so trust the extension for them.
    if(isRawExtension(ext)) return Format::Raw;
    if(n >= 3 && h[0] == 0xFF && h[1] == 0xD8 && h[2] == 0xFF) return Format::JPEG;
    if(n >= 8 && !memcmp(h, "\x89PNG\r\n\x1a\n", 8)) return Format::PNG;
    if(n >= 6 && (!memcmp(h, "GIF87a", 6) || !memcmp(h, "GIF89a", 6))) return Format::GIF;
    if(n >= 12 && !memcmp(h, "RIFF", 4) && !memcmp(h + 8, "WEBP", 4)) return Format::WebP;
    if(n >= 2 && h[0] == 0xFF && h[1] == 0x0A) return Format::JXL;
    if(n >= 12 && !memcmp(h, "\0\0\0\x0cJXL \r\n\x87\n", 12)) return Format::JXL;
    if(n >= 12 && !memcmp(h + 4, "ftyp", 4)) {
        // ISO BMFF: check the major brand and the compatible brands.
        uint32_t boxSize = (h[0] << 24) | (h[1] << 16) | (h[2] << 8) | h[3];
        size_t end = std::min<size_t>(n, boxSize);
        bool avif = false, heif = false;
        auto brand = [&](size_t off) {
            const char* b = reinterpret_cast<const char*>(h + off);
            if(!memcmp(b, "avif", 4) || !memcmp(b, "avis", 4)) avif = true;
            if(!memcmp(b, "heic", 4) || !memcmp(b, "heix", 4) || !memcmp(b, "hevc", 4) || !memcmp(b, "hevx", 4) ||
               !memcmp(b, "heim", 4) || !memcmp(b, "heis", 4) || !memcmp(b, "mif1", 4) || !memcmp(b, "msf1", 4))
                heif = true;
        };
        brand(8);
        for(size_t off = 16; off + 4 <= end; off += 4) brand(off);
        if(avif) return Format::AVIF;
        if(heif) return Format::HEIF;
    }
    Format byExt = fromExtension(ext);
    if(byExt == Format::Unknown) return Format::Pixbuf;  // let gdk-pixbuf try
    return byExt;
}

}  // namespace qimgv
