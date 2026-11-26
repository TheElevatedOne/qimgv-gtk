// Metadata front end. The real work is done by the optional qimgv-exiv2.so plugin (see metadata_plugin.h);
// without it, EXIF orientation of JPEG and WebP files is still read here so photos display upright.
#include "image/metadata.hpp"

#include <dlfcn.h>
#include <unistd.h>

#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

#include "config.hpp"
#include "image/metadata_plugin.h"

namespace qimgv {

namespace {

struct Plugin {
    decltype(&qimgv_metadata_library) library = nullptr;
    decltype(&qimgv_metadata_init) init = nullptr;
    decltype(&qimgv_metadata_orientation) orientation = nullptr;
    decltype(&qimgv_metadata_exif_summary) exifSummary = nullptr;
    decltype(&qimgv_metadata_copy) copy = nullptr;
};

Plugin gPlugin;
const Plugin* gPluginPtr = nullptr;
std::string gPluginStatus = "not found";

std::string exeDir() {
    char buf[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if(n <= 0) return {};
    buf[n] = 0;
    std::string p = buf;
    auto slash = p.find_last_of('/');
    return slash == std::string::npos ? std::string() : p.substr(0, slash);
}

std::vector<std::string> pluginDirs() {
    std::vector<std::string> dirs;
    if(const char* env = getenv("QIMGV_PLUGIN_DIR")) dirs.push_back(env);
    std::string exe = exeDir();
    if(!exe.empty()) {
        dirs.push_back(exe);  // build tree
        dirs.push_back(exe + "/../lib/qimgv");
        dirs.push_back(exe + "/../lib64/qimgv");
    }
    dirs.push_back(QIMGV_PLUGIN_DIR);
    return dirs;
}

void loadPlugin() {
    std::string errors;
    for(auto& dir : pluginDirs()) {
        std::string path = dir + "/qimgv-exiv2.so";
        if(access(path.c_str(), F_OK) != 0) continue;
        void* h = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if(!h) {
            // Typically exiv2 itself is missing or got a new soname.
            const char* e = dlerror();
            errors = e ? e : path;
            continue;
        }
        auto version = reinterpret_cast<decltype(&qimgv_metadata_api_version)>(dlsym(h, "qimgv_metadata_api_version"));
        Plugin p;
        p.library = reinterpret_cast<decltype(p.library)>(dlsym(h, "qimgv_metadata_library"));
        p.init = reinterpret_cast<decltype(p.init)>(dlsym(h, "qimgv_metadata_init"));
        p.orientation = reinterpret_cast<decltype(p.orientation)>(dlsym(h, "qimgv_metadata_orientation"));
        p.exifSummary = reinterpret_cast<decltype(p.exifSummary)>(dlsym(h, "qimgv_metadata_exif_summary"));
        p.copy = reinterpret_cast<decltype(p.copy)>(dlsym(h, "qimgv_metadata_copy"));
        if(!version || version() != QIMGV_METADATA_PLUGIN_API || !p.library || !p.init || !p.orientation ||
           !p.exifSummary || !p.copy) {
            errors = path + ": incompatible plugin";
            dlclose(h);
            continue;
        }
        p.init();
        gPlugin = p;
        gPluginPtr = &gPlugin;
        gPluginStatus = std::string(p.library()) + " (" + path + ")";
        return;
    }
    if(!errors.empty()) gPluginStatus = errors;
}

const Plugin* plugin() {
    static std::once_flag once;
    std::call_once(once, loadPlugin);
    return gPluginPtr;
}

// ---- built-in EXIF orientation reader (used without the plugin) ------------

int orientationFromTiff(const uint8_t* d, size_t n) {
    if(n < 8) return 1;
    bool le = d[0] == 'I' && d[1] == 'I';
    if(!le && !(d[0] == 'M' && d[1] == 'M')) return 1;
    auto u16 = [&](size_t o) -> uint32_t { return le ? d[o] | d[o + 1] << 8 : d[o] << 8 | d[o + 1]; };
    auto u32 = [&](size_t o) -> uint32_t {
        return le ? d[o] | d[o + 1] << 8 | d[o + 2] << 16 | (uint32_t)d[o + 3] << 24
                  : (uint32_t)d[o] << 24 | d[o + 1] << 16 | d[o + 2] << 8 | d[o + 3];
    };
    size_t ifd = u32(4);
    if(ifd + 2 > n) return 1;
    size_t count = u16(ifd);
    for(size_t i = 0; i < count; i++) {
        size_t e = ifd + 2 + i * 12;
        if(e + 12 > n) break;
        if(u16(e) == 0x0112 && u16(e + 2) == 3) {  // Orientation, SHORT
            uint32_t v = u16(e + 8);
            return v >= 1 && v <= 8 ? (int)v : 1;
        }
    }
    return 1;
}

int orientationFromExifBlock(const uint8_t* d, size_t n) {
    if(n >= 6 && !memcmp(d, "Exif\0\0", 6)) {
        d += 6;
        n -= 6;
    }
    return orientationFromTiff(d, n);
}

int builtinOrientation(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if(!f) return 1;
    int result = 1;
    uint8_t h[12];
    size_t n = fread(h, 1, sizeof(h), f);
    std::vector<uint8_t> buf;
    if(n >= 2 && h[0] == 0xFF && h[1] == 0xD8) {
        // JPEG: walk the markers up to the start of scan.
        long pos = 2;
        for(;;) {
            uint8_t m[4];
            if(fseek(f, pos, SEEK_SET) != 0 || fread(m, 1, 4, f) != 4 || m[0] != 0xFF) break;
            if(m[1] == 0xDA || m[1] == 0xD9) break;
            size_t len = (size_t)(m[2] << 8 | m[3]);
            if(len < 2) break;
            if(m[1] == 0xE1) {
                buf.resize(len - 2);
                if(fread(buf.data(), 1, buf.size(), f) != buf.size()) break;
                if(buf.size() >= 6 && !memcmp(buf.data(), "Exif\0\0", 6)) {
                    result = orientationFromExifBlock(buf.data(), buf.size());
                    break;
                }
            }
            pos += 2 + (long)len;
        }
    } else if(n == 12 && !memcmp(h, "RIFF", 4) && !memcmp(h + 8, "WEBP", 4)) {
        // WebP: look for the EXIF chunk.
        long pos = 12;
        uint8_t c[8];
        while(fseek(f, pos, SEEK_SET) == 0 && fread(c, 1, 8, f) == 8) {
            size_t len = c[4] | c[5] << 8 | c[6] << 16 | (size_t)c[7] << 24;
            if(!memcmp(c, "EXIF", 4)) {
                if(len > (64u << 20)) break;
                buf.resize(len);
                if(fread(buf.data(), 1, len, f) == len) result = orientationFromExifBlock(buf.data(), len);
                break;
            }
            pos += 8 + (long)len + (long)(len & 1);
        }
    }
    fclose(f);
    return result;
}

}  // namespace

void initMetadata() { plugin(); }

bool haveMetadataPlugin() { return plugin() != nullptr; }

std::string metadataPluginReport() {
    plugin();
    return gPluginStatus;
}

int readOrientation(const std::string& path) {
    if(const Plugin* p = plugin()) return p->orientation(path.c_str());
    return builtinOrientation(path);
}

InfoList readExifSummary(const std::string& path) {
    InfoList out;
    if(const Plugin* p = plugin())
        p->exifSummary(
            path.c_str(),
            [](void* user, const char* k, const char* v) { static_cast<InfoList*>(user)->emplace_back(k, v); },
            &out);
    return out;
}

void copyMetadata(const std::string& src, const std::string& dst) {
    if(const Plugin* p = plugin()) p->copy(src.c_str(), dst.c_str());
}

}  // namespace qimgv
