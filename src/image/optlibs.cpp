#include "image/optlibs.hpp"

#include <dirent.h>
#include <dlfcn.h>
#include <string.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <vector>

namespace qimgv {

namespace {

std::vector<std::string> libraryDirs() {
    std::vector<std::string> dirs;
    auto add = [&](std::string d) {
        while(d.size() > 1 && d.back() == '/') d.pop_back();
        if(!d.empty() && std::find(dirs.begin(), dirs.end(), d) == dirs.end()) dirs.push_back(d);
    };
    if(const char* env = getenv("LD_LIBRARY_PATH")) {
        std::string s = env;
        size_t pos = 0;
        while(pos <= s.size()) {
            size_t end = s.find(':', pos);
            if(end == std::string::npos) end = s.size();
            add(s.substr(pos, end - pos));
            pos = end + 1;
        }
    }
    // The directory libc was loaded from covers multiarch layouts (/usr/lib/x86_64-linux-gnu).
    Dl_info info;
    if(dladdr(reinterpret_cast<void*>(&fopen), &info) && info.dli_fname) {
        std::string p = info.dli_fname;
        auto slash = p.find_last_of('/');
        if(slash != std::string::npos) add(p.substr(0, slash));
    }
    for(const char* d : {"/usr/local/lib", "/usr/local/lib64", "/usr/lib", "/usr/lib64", "/lib", "/lib64"}) add(d);
    return dirs;
}

std::string handlePath(void* h, const char* sym) {
    Dl_info info;
    void* addr = sym ? dlsym(h, sym) : nullptr;
    if(addr && dladdr(addr, &info) && info.dli_fname) return info.dli_fname;
    return {};
}

}  // namespace

void* openSharedLibrary(const char* base, std::string* loadedFrom) {
    std::string so = std::string(base) + ".so";
    if(void* h = dlopen(so.c_str(), RTLD_NOW | RTLD_LOCAL)) {
        if(loadedFrom) *loadedFrom = so;
        return h;
    }
    // Distributions that ship the unversioned symlink only in a -dev package: take the newest soname.
    std::string prefix = so + ".";
    std::vector<std::string> found;
    for(auto& dir : libraryDirs()) {
        DIR* d = opendir(dir.c_str());
        if(!d) continue;
        while(dirent* e = readdir(d))
            if(!strncmp(e->d_name, prefix.c_str(), prefix.size())) found.push_back(dir + "/" + e->d_name);
        closedir(d);
    }
    std::sort(found.begin(), found.end(),
              [](const std::string& a, const std::string& b) { return strverscmp(a.c_str(), b.c_str()) > 0; });
    for(auto& path : found) {
        if(void* h = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL)) {
            if(loadedFrom) *loadedFrom = path;
            return h;
        }
    }
    return nullptr;
}

// ---- loaders ---------------------------------------------------------------

namespace {

struct Status {
    const char* name;
    std::string text;
};

// Resolve every listed symbol; on failure `missing` names the first one that was not found.
#define QIMGV_LOAD_SYM(name)                                                    \
    if(ok) {                                                                    \
        api.name = reinterpret_cast<decltype(api.name)>(dlsym(handle, #name)); \
        if(!api.name) {                                                         \
            ok = false;                                                         \
            missing = #name;                                                    \
        }                                                                       \
    }

#ifdef QIMGV_WITH_JXL
JxlApi gJxl;
const JxlApi* gJxlPtr = nullptr;
Status gJxlStatus{"libjxl", "not loaded"};

void loadJxl() {
    std::string from;
    void* handle = openSharedLibrary("libjxl", &from);
    if(!handle) {
        gJxlStatus.text = "not installed";
        return;
    }
    JxlApi api;
    bool ok = true;
    std::string missing;
    QIMGV_JXL_FUNCS(QIMGV_LOAD_SYM)
    if(!ok) {
        gJxlStatus.text = from + ": missing " + missing;
        dlclose(handle);
        return;
    }
    uint32_t v = api.JxlDecoderVersion();
    char ver[32];
    snprintf(ver, sizeof(ver), "%u.%u.%u", v / 1000000, v / 1000 % 1000, v % 1000);
    gJxlStatus.text = std::string(ver) + " (" + handlePath(handle, "JxlDecoderCreate") + ")";
    // Threads are optional.
    std::string thFrom;
    if(void* th = openSharedLibrary("libjxl_threads", &thFrom)) {
        JxlApi t;
        bool tok = true;
        std::string tmissing;
        {
            JxlApi& api = t;
            void* handle = th;
            bool& ok = tok;
            std::string& missing = tmissing;
            QIMGV_JXL_THREAD_FUNCS(QIMGV_LOAD_SYM)
        }
        if(tok) {
#define QIMGV_COPY_SYM(name) api.name = t.name;
            QIMGV_JXL_THREAD_FUNCS(QIMGV_COPY_SYM)
#undef QIMGV_COPY_SYM
        } else {
            dlclose(th);
            gJxlStatus.text += ", single threaded";
        }
    } else {
        gJxlStatus.text += ", single threaded (no libjxl_threads)";
    }
    gJxl = api;
    gJxlPtr = &gJxl;
}
#endif

#ifdef QIMGV_WITH_AVIF
AvifApi gAvif;
const AvifApi* gAvifPtr = nullptr;
Status gAvifStatus{"libavif", "not loaded"};

void loadAvif() {
    std::string from;
    void* handle = openSharedLibrary("libavif", &from);
    if(!handle) {
        gAvifStatus.text = "not installed";
        return;
    }
    AvifApi api;
    bool ok = true;
    std::string missing;
    QIMGV_AVIF_FUNCS(QIMGV_LOAD_SYM)
    if(!ok) {
        gAvifStatus.text = from + ": missing " + missing;
        dlclose(handle);
        return;
    }
    // qimgv reads avifDecoder/avifRGBImage fields directly; their layout is only stable within a major version.
    const char* ver = api.avifVersion();
    if(atoi(ver) != AVIF_VERSION_MAJOR) {
        char buf[160];
        snprintf(buf, sizeof(buf), "%s is version %s, qimgv was built for %d.x (rebuild qimgv)", from.c_str(),
                 ver, AVIF_VERSION_MAJOR);
        gAvifStatus.text = buf;
        dlclose(handle);
        return;
    }
    gAvifStatus.text = std::string(ver) + " (" + handlePath(handle, "avifDecoderCreate") + ")";
    gAvif = api;
    gAvifPtr = &gAvif;
}
#endif

#ifdef QIMGV_WITH_HEIF
HeifApi gHeif;
const HeifApi* gHeifPtr = nullptr;
Status gHeifStatus{"libheif", "not loaded"};

void loadHeif() {
    std::string from;
    void* handle = openSharedLibrary("libheif", &from);
    if(!handle) {
        gHeifStatus.text = "not installed";
        return;
    }
    HeifApi api;
    bool ok = true;
    std::string missing;
    QIMGV_HEIF_FUNCS(QIMGV_LOAD_SYM)
    if(!ok) {
        gHeifStatus.text = from + ": missing " + missing;
        dlclose(handle);
        return;
    }
    api.heif_init(nullptr);  // never deinitialised: decoders may run until exit
    using VersionFn = const char* (*)();
    auto version = reinterpret_cast<VersionFn>(dlsym(handle, "heif_get_version"));
    gHeifStatus.text = std::string(version ? version() : "?") + " (" + handlePath(handle, "heif_init") + ")";
    gHeif = api;
    gHeifPtr = &gHeif;
}
#endif

#ifdef QIMGV_WITH_RAW
RawApi gRaw;
const RawApi* gRawPtr = nullptr;
Status gRawStatus{"libraw_r", "not loaded"};

void loadRaw() {
    std::string from;
    void* handle = openSharedLibrary("libraw_r", &from);
    if(!handle) {
        gRawStatus.text = "not installed";
        return;
    }
    RawApi api;
    bool ok = true;
    std::string missing;
    QIMGV_RAW_FUNCS(QIMGV_LOAD_SYM)
    if(!ok) {
        gRawStatus.text = from + ": missing " + missing;
        dlclose(handle);
        return;
    }
    int v = api.libraw_versionNumber();
    api.sameAbi = (v & 0xffff00) == (LIBRAW_VERSION & 0xffff00);
    char buf[64];
    snprintf(buf, sizeof(buf), "%d.%d.%d", v >> 16, (v >> 8) & 0xff, v & 0xff);
    gRawStatus.text = std::string(buf) + " (" + handlePath(handle, "libraw_init") + ")";
    if(!api.sameAbi) gRawStatus.text += ", built for a different version: basic mode";
    gRaw = api;
    gRawPtr = &gRaw;
}
#endif

#undef QIMGV_LOAD_SYM

void loadAll() {
    static std::once_flag once;
    std::call_once(once, [] {
#ifdef QIMGV_WITH_JXL
        loadJxl();
#endif
#ifdef QIMGV_WITH_AVIF
        loadAvif();
#endif
#ifdef QIMGV_WITH_HEIF
        loadHeif();
#endif
#ifdef QIMGV_WITH_RAW
        loadRaw();
#endif
    });
}

}  // namespace

#ifdef QIMGV_WITH_JXL
const JxlApi* jxlApi() {
    loadAll();
    return gJxlPtr;
}
#endif
#ifdef QIMGV_WITH_AVIF
const AvifApi* avifApi() {
    loadAll();
    return gAvifPtr;
}
#endif
#ifdef QIMGV_WITH_HEIF
const HeifApi* heifApi() {
    loadAll();
    return gHeifPtr;
}
#endif
#ifdef QIMGV_WITH_RAW
const RawApi* rawApi() {
    loadAll();
    return gRawPtr;
}
#endif

bool haveJxl() {
#ifdef QIMGV_WITH_JXL
    return jxlApi() != nullptr;
#else
    return false;
#endif
}
bool haveAvif() {
#ifdef QIMGV_WITH_AVIF
    return avifApi() != nullptr;
#else
    return false;
#endif
}
bool haveHeif() {
#ifdef QIMGV_WITH_HEIF
    return heifApi() != nullptr;
#else
    return false;
#endif
}
bool haveRaw() {
#ifdef QIMGV_WITH_RAW
    return rawApi() != nullptr;
#else
    return false;
#endif
}

std::string optionalLibrariesReport() {
    loadAll();
    std::string out;
    auto line = [&](const char* name, const std::string& text) { out += std::string("  ") + name + ": " + text + "\n"; };
#ifdef QIMGV_WITH_JXL
    line(gJxlStatus.name, gJxlStatus.text);
#else
    line("libjxl", "disabled at build time");
#endif
#ifdef QIMGV_WITH_AVIF
    line(gAvifStatus.name, gAvifStatus.text);
#else
    line("libavif", "disabled at build time");
#endif
#ifdef QIMGV_WITH_HEIF
    line(gHeifStatus.name, gHeifStatus.text);
#else
    line("libheif", "disabled at build time");
#endif
#ifdef QIMGV_WITH_RAW
    line(gRawStatus.name, gRawStatus.text);
#else
    line("libraw_r", "disabled at build time");
#endif
    return out;
}

}  // namespace qimgv
