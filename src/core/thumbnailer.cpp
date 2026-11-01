#include "core/thumbnailer.hpp"

#include <glib/gstdio.h>
#include <sys/stat.h>

#include "image/thumbnail.hpp"
#include "settings.hpp"

namespace qimgv {

static constexpr size_t kMemoryLimit = 600;

Thumbnailer::Thumbnailer() : mPool(settings().thumbnailerThreads), mGeneration(std::make_shared<int>(0)) {}

Thumbnailer::~Thumbnailer() {
    mPool.stop();
    clearMemory();
}

void Thumbnailer::setThreads(int n) {
    ++*mGeneration;
    mPending.clear();
    mPool.resize(n);
}

std::string Thumbnailer::key(const std::string& path, int size, bool square) const {
    return path + '\n' + std::to_string(size) + (square ? "s" : "");
}

void Thumbnailer::insert(const std::string& k, cairo_surface_t* s) {
    auto it = mCache.find(k);
    if(it != mCache.end()) {
        cairo_surface_destroy(it->second.first);
        mLru.erase(it->second.second);
        mCache.erase(it);
    }
    mLru.push_front(k);
    mCache[k] = {s, mLru.begin()};
    while(mCache.size() > kMemoryLimit) {
        auto last = mCache.find(mLru.back());
        if(last != mCache.end()) {
            cairo_surface_destroy(last->second.first);
            mCache.erase(last);
        }
        mLru.pop_back();
    }
}

cairo_surface_t* Thumbnailer::get(const std::string& path, int size, bool square, const Callback& cb) {
    std::string k = key(path, size, square);
    auto it = mCache.find(k);
    if(it != mCache.end()) {
        mLru.splice(mLru.begin(), mLru, it->second.second);
        return it->second.first;
    }
    if(mPending.count(k)) return nullptr;
    mPending.insert(k);
    bool disk = settings().useThumbnailCache;
    std::weak_ptr<int> gen = mGeneration;
    int myGen = *mGeneration;
    mPool.submit(
        [this, path, size, square, disk, k, cb, gen, myGen] {
            {
                auto g = gen.lock();
                if(!g || *g != myGen) return;  // cancelled
            }
            cairo_surface_t* s = loadOrGenerate(path, size, square, disk);
            runOnMain([this, path, size, k, cb, gen, myGen, s] {
                auto g = gen.lock();
                if(!g || *g != myGen) {
                    if(s) cairo_surface_destroy(s);
                    return;
                }
                mPending.erase(k);
                if(s) insert(k, s);
                if(cb) cb(path, size, s);
            });
        },
        true);
    return nullptr;
}

void Thumbnailer::cancelPending() {
    ++*mGeneration;
    mPending.clear();
    mPool.clear();
}

void Thumbnailer::forget(const std::string& path) {
    std::string prefix = path + '\n';
    for(auto it = mCache.begin(); it != mCache.end();) {
        if(it->first.compare(0, prefix.size(), prefix) == 0) {
            cairo_surface_destroy(it->second.first);
            mLru.erase(it->second.second);
            it = mCache.erase(it);
        } else {
            ++it;
        }
    }
}

void Thumbnailer::clearMemory() {
    for(auto& [k, v] : mCache) cairo_surface_destroy(v.first);
    mCache.clear();
    mLru.clear();
}

static std::string diskPath(const std::string& path, int size, bool square) {
    gchar* md5 = g_compute_checksum_for_string(G_CHECKSUM_MD5, path.c_str(), -1);
    std::string p = Settings::thumbnailDir() + "/" + md5 + "_" + std::to_string(size) + (square ? "s" : "") + ".png";
    g_free(md5);
    return p;
}

cairo_surface_t* Thumbnailer::loadOrGenerate(const std::string& path, int size, bool square, bool useDiskCache) {
    std::string cachePath;
    GStatBuf src;
    if(g_stat(path.c_str(), &src) != 0) return nullptr;
    if(useDiskCache) {
        cachePath = diskPath(path, size, square);
        GStatBuf cst;
        if(g_stat(cachePath.c_str(), &cst) == 0 && cst.st_mtime >= src.st_mtime) {
            cairo_surface_t* s = cairo_image_surface_create_from_png(cachePath.c_str());
            if(cairo_surface_status(s) == CAIRO_STATUS_SUCCESS) {
                // cairo loads opaque PNGs as RGB24; convert so all thumbnails share one format.
                if(cairo_image_surface_get_format(s) != CAIRO_FORMAT_ARGB32) {
                    int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
                    cairo_surface_t* a = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
                    cairo_t* cr = cairo_create(a);
                    cairo_set_source_surface(cr, s, 0, 0);
                    cairo_paint(cr);
                    cairo_destroy(cr);
                    cairo_surface_destroy(s);
                    s = a;
                }
                return s;
            }
            cairo_surface_destroy(s);
        }
    }
    cairo_surface_t* s = generateThumbnail(path, size, square);
    if(s && useDiskCache) {
        g_mkdir_with_parents(Settings::thumbnailDir().c_str(), 0700);
        std::string tmp = cachePath + ".tmp" + std::to_string((uintptr_t)g_thread_self());
        if(cairo_surface_write_to_png(s, tmp.c_str()) == CAIRO_STATUS_SUCCESS)
            g_rename(tmp.c_str(), cachePath.c_str());
        else
            g_unlink(tmp.c_str());
    }
    return s;
}

void Thumbnailer::clearDiskCache() {
    std::string dir = Settings::thumbnailDir();
    GDir* d = g_dir_open(dir.c_str(), 0, nullptr);
    if(!d) return;
    while(const gchar* name = g_dir_read_name(d)) {
        if(!g_str_has_suffix(name, ".png")) continue;
        gchar* full = g_build_filename(dir.c_str(), name, nullptr);
        g_unlink(full);
        g_free(full);
    }
    g_dir_close(d);
}

}  // namespace qimgv
