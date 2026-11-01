// Asynchronous image loading with a small cache for preloaded neighbours.
#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "core/workers.hpp"
#include "image/image.hpp"

namespace qimgv {

class Loader {
public:
    using Callback = std::function<void(ImagePtr img, const std::string& error)>;
    Loader();
    ~Loader();

    // Load `path`. The callback runs on the main thread, unless a newer load() superseded it.
    // Returns true if the image came straight from the cache (callback already called).
    bool load(const std::string& path, Callback cb);
    // Decode in the background and keep the result in the cache.
    void preload(const std::string& path);
    // Keep only these paths in the cache.
    void retain(const std::set<std::string>& paths);
    void invalidate(const std::string& path);
    void clear();
    ImagePtr cached(const std::string& path) const;

private:
    void decode(const std::string& path);
    ThreadPool mPool;
    std::map<std::string, ImagePtr> mCache;
    std::set<std::string> mInFlight;
    std::set<std::string> mRetain;
    std::string mWanted;
    Callback mWantedCb;
    std::shared_ptr<bool> mAlive;
};

}  // namespace qimgv
