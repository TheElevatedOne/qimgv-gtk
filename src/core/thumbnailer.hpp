// Background thumbnail generation with a memory cache and an on-disk cache
// ($XDG_CACHE_HOME/qimgv/thumbnails).
#pragma once

#include <cairo.h>

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "core/workers.hpp"

namespace qimgv {

class Thumbnailer {
public:
    using Callback = std::function<void(const std::string& path, int size, cairo_surface_t* thumb)>;
    Thumbnailer();
    ~Thumbnailer();

    // Returns a cached thumbnail (borrowed reference) or nullptr and schedules generation.
    // `cb` runs on the main thread when a scheduled thumbnail is ready (thumb may be nullptr on error).
    cairo_surface_t* get(const std::string& path, int size, bool square, const Callback& cb);
    void cancelPending();
    void forget(const std::string& path);
    void setThreads(int n);
    void clearMemory();

    // Synchronous: generate and store thumbnails on disk (used by --gen-thumbs).
    static cairo_surface_t* loadOrGenerate(const std::string& path, int size, bool square, bool useDiskCache);
    static void clearDiskCache();

private:
    std::string key(const std::string& path, int size, bool square) const;
    void insert(const std::string& k, cairo_surface_t* s);
    ThreadPool mPool;
    std::map<std::string, std::pair<cairo_surface_t*, std::list<std::string>::iterator>> mCache;
    std::list<std::string> mLru;
    std::set<std::string> mPending;
    std::shared_ptr<int> mGeneration;
};

}  // namespace qimgv
