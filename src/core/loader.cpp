#include "core/loader.hpp"

#include "image/decoder.hpp"
#include "settings.hpp"

namespace qimgv {

Loader::Loader() : mPool(2), mAlive(std::make_shared<bool>(true)) {}

Loader::~Loader() {
    *mAlive = false;
    mPool.stop();
}

ImagePtr Loader::cached(const std::string& path) const {
    auto it = mCache.find(path);
    return it == mCache.end() ? nullptr : it->second;
}

bool Loader::load(const std::string& path, Callback cb) {
    mWanted = path;
    mWantedCb = nullptr;
    if(auto img = cached(path)) {
        cb(img, {});
        return true;
    }
    mWantedCb = std::move(cb);
    if(!mInFlight.count(path)) decode(path);
    return false;
}

void Loader::preload(const std::string& path) {
    if(path.empty() || mCache.count(path) || mInFlight.count(path)) return;
    decode(path);
}

void Loader::decode(const std::string& path) {
    mInFlight.insert(path);
    DecodeOptions opt;
    opt.rawUseEmbeddedPreview = settings().rawEmbeddedPreview;
    opt.animations = settings().animations;
    std::weak_ptr<bool> alive = mAlive;
    mPool.submit([this, path, opt, alive] {
        std::string err;
        ImagePtr img = decodeFile(path, opt, &err);
        runOnMain([this, path, img, err, alive] {
            if(alive.expired() || !*alive.lock()) return;
            mInFlight.erase(path);
            if(img && (mRetain.empty() || mRetain.count(path) || path == mWanted)) mCache[path] = img;
            if(path == mWanted && mWantedCb) {
                Callback cb = std::move(mWantedCb);
                mWantedCb = nullptr;
                cb(img, err);
            }
        });
    });
}

void Loader::retain(const std::set<std::string>& paths) {
    mRetain = paths;
    for(auto it = mCache.begin(); it != mCache.end();) {
        if(!paths.count(it->first))
            it = mCache.erase(it);
        else
            ++it;
    }
}

void Loader::invalidate(const std::string& path) { mCache.erase(path); }

void Loader::clear() { mCache.clear(); }

}  // namespace qimgv
