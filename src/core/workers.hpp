// Small thread pool and a helper to run code on the GTK main loop.
#pragma once

#include <glib.h>

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace qimgv {

// Run `fn` on the main loop (safe to call from any thread).
inline void runOnMain(std::function<void()> fn) {
    auto* f = new std::function<void()>(std::move(fn));
    g_idle_add_full(
        G_PRIORITY_DEFAULT,
        [](gpointer p) -> gboolean {
            (*static_cast<std::function<void()>*>(p))();
            return G_SOURCE_REMOVE;
        },
        f, [](gpointer p) { delete static_cast<std::function<void()>*>(p); });
}

class ThreadPool {
public:
    explicit ThreadPool(int threads) { resize(threads); }
    ~ThreadPool() { stop(); }

    // `front` = run before everything queued so far (LIFO use for visible thumbnails).
    void submit(std::function<void()> job, bool front = false) {
        {
            std::lock_guard<std::mutex> l(mMutex);
            if(front)
                mQueue.push_front(std::move(job));
            else
                mQueue.push_back(std::move(job));
        }
        mCond.notify_one();
    }
    void clear() {
        std::lock_guard<std::mutex> l(mMutex);
        mQueue.clear();
    }
    void resize(int threads) {
        stop();
        mStop = false;
        for(int i = 0; i < std::max(1, threads); i++) mThreads.emplace_back([this] { run(); });
    }
    void stop() {
        {
            std::lock_guard<std::mutex> l(mMutex);
            mStop = true;
            mQueue.clear();
        }
        mCond.notify_all();
        for(auto& t : mThreads) t.join();
        mThreads.clear();
    }

private:
    void run() {
        for(;;) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> l(mMutex);
                mCond.wait(l, [this] { return mStop || !mQueue.empty(); });
                if(mStop) return;
                job = std::move(mQueue.front());
                mQueue.pop_front();
            }
            job();
        }
    }
    std::mutex mMutex;
    std::condition_variable mCond;
    std::deque<std::function<void()>> mQueue;
    std::vector<std::thread> mThreads;
    bool mStop = false;
};

}  // namespace qimgv
