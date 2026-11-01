#include "core/directorymodel.hpp"

#include <glib/gstdio.h>
#include <sys/stat.h>

#include <algorithm>

#include "image/formats.hpp"

namespace qimgv {

static std::string collate(const std::string& name) {
    gchar* k = g_utf8_collate_key_for_filename(name.c_str(), -1);
    std::string r = k;
    g_free(k);
    return r;
}

bool statEntry(const std::string& path, FileEntry& e) {
    GStatBuf st;
    if(g_stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) return false;
    e.path = path;
    gchar* base = g_path_get_basename(path.c_str());
    e.name = base;
    g_free(base);
    e.collateKey = collate(e.name);
    e.size = st.st_size;
    e.mtime = (int64_t)st.st_mtim.tv_sec * 1000000 + st.st_mtim.tv_nsec / 1000;
    return true;
}

DirectoryModel::DirectoryModel() = default;

DirectoryModel::~DirectoryModel() {
    stopMonitor();
    if(mDebounce) g_source_remove(mDebounce);
}

bool DirectoryModel::setDirectory(const std::string& dir) {
    if(!listDirectory(dir)) return false;
    startMonitor();
    return true;
}

bool DirectoryModel::listDirectory(const std::string& dir) {
    GDir* d = g_dir_open(dir.c_str(), 0, nullptr);
    if(!d) return false;
    mDir = dir;
    mFiles.clear();
    mSubdirs.clear();
    while(const gchar* name = g_dir_read_name(d)) {
        if(name[0] == '.') continue;  // hidden files
        gchar* full = g_build_filename(dir.c_str(), name, nullptr);
        std::string path = full;
        g_free(full);
        if(g_file_test(path.c_str(), G_FILE_TEST_IS_DIR)) {
            mSubdirs.push_back(path);
            continue;
        }
        if(!isSupportedImageFile(path)) continue;  // skips videos and everything else
        FileEntry e;
        if(statEntry(path, e)) mFiles.push_back(std::move(e));
    }
    g_dir_close(d);
    std::sort(mSubdirs.begin(), mSubdirs.end(), [](const std::string& a, const std::string& b) {
        return collate(a) < collate(b);
    });
    sort();
    return true;
}

void DirectoryModel::reload() {
    if(!mDir.empty()) setDirectory(mDir);
}

int DirectoryModel::indexOf(const std::string& path) const {
    for(size_t i = 0; i < mFiles.size(); i++)
        if(mFiles[i].path == path) return (int)i;
    return -1;
}

void DirectoryModel::setSortMode(SortMode m) {
    mSort = m;
    sort();
}

void DirectoryModel::sort() {
    auto byName = [](const FileEntry& a, const FileEntry& b) { return a.collateKey < b.collateKey; };
    switch(mSort) {
        case SortMode::Name: std::sort(mFiles.begin(), mFiles.end(), byName); break;
        case SortMode::NameDesc:
            std::sort(mFiles.begin(), mFiles.end(), [&](auto& a, auto& b) { return byName(b, a); });
            break;
        case SortMode::Size:
            std::stable_sort(mFiles.begin(), mFiles.end(), [&](auto& a, auto& b) {
                return a.size != b.size ? a.size < b.size : byName(a, b);
            });
            break;
        case SortMode::SizeDesc:
            std::stable_sort(mFiles.begin(), mFiles.end(), [&](auto& a, auto& b) {
                return a.size != b.size ? a.size > b.size : byName(a, b);
            });
            break;
        case SortMode::TimeOldest:
            std::stable_sort(mFiles.begin(), mFiles.end(), [&](auto& a, auto& b) {
                return a.mtime != b.mtime ? a.mtime < b.mtime : byName(a, b);
            });
            break;
        case SortMode::TimeNewest:
            std::stable_sort(mFiles.begin(), mFiles.end(), [&](auto& a, auto& b) {
                return a.mtime != b.mtime ? a.mtime > b.mtime : byName(a, b);
            });
            break;
    }
}

void DirectoryModel::removePath(const std::string& path) {
    int i = indexOf(path);
    if(i >= 0) mFiles.erase(mFiles.begin() + i);
}

void DirectoryModel::renamePath(const std::string& from, const std::string& to) {
    int i = indexOf(from);
    if(i < 0) return;
    FileEntry e;
    if(statEntry(to, e)) {
        mFiles[i] = e;
        sort();
    } else {
        mFiles.erase(mFiles.begin() + i);
    }
}

void DirectoryModel::startMonitor() {
    stopMonitor();
    GFile* f = g_file_new_for_path(mDir.c_str());
    mMonitor = g_file_monitor_directory(f, G_FILE_MONITOR_WATCH_MOVES, nullptr, nullptr);
    g_object_unref(f);
    if(mMonitor) {
        g_file_monitor_set_rate_limit(mMonitor, 500);
        g_signal_connect(mMonitor, "changed", G_CALLBACK(monitorCb), this);
    }
}

void DirectoryModel::stopMonitor() {
    if(mMonitor) {
        g_signal_handlers_disconnect_by_data(mMonitor, this);
        g_file_monitor_cancel(mMonitor);
        g_object_unref(mMonitor);
        mMonitor = nullptr;
    }
}

void DirectoryModel::monitorCb(GFileMonitor*, GFile* file, GFile*, GFileMonitorEvent ev, gpointer self) {
    auto* m = static_cast<DirectoryModel*>(self);
    if(ev == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT) {
        gchar* p = g_file_get_path(file);
        if(p && m->onFileModified && m->indexOf(p) >= 0) m->onFileModified(p);
        g_free(p);
    }
    if(ev == G_FILE_MONITOR_EVENT_CREATED || ev == G_FILE_MONITOR_EVENT_DELETED ||
       ev == G_FILE_MONITOR_EVENT_MOVED_IN || ev == G_FILE_MONITOR_EVENT_MOVED_OUT ||
       ev == G_FILE_MONITOR_EVENT_RENAMED || ev == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT) {
        if(m->mDebounce) g_source_remove(m->mDebounce);
        m->mDebounce = g_timeout_add(300, debounceCb, m);
    }
}

gboolean DirectoryModel::debounceCb(gpointer self) {
    auto* m = static_cast<DirectoryModel*>(self);
    m->mDebounce = 0;
    std::string dir = m->mDir;
    m->listDirectory(dir);
    if(m->onChanged) m->onChanged();
    return G_SOURCE_REMOVE;
}

std::vector<std::string> DirectoryModel::siblingDirectories() const {
    std::vector<std::string> out;
    if(mDir.empty() || mDir == "/") return out;
    gchar* parent = g_path_get_dirname(mDir.c_str());
    GDir* d = g_dir_open(parent, 0, nullptr);
    if(d) {
        while(const gchar* name = g_dir_read_name(d)) {
            if(name[0] == '.') continue;
            gchar* full = g_build_filename(parent, name, nullptr);
            if(g_file_test(full, G_FILE_TEST_IS_DIR)) out.emplace_back(full);
            g_free(full);
        }
        g_dir_close(d);
    }
    g_free(parent);
    std::sort(out.begin(), out.end(), [](const std::string& a, const std::string& b) { return collate(a) < collate(b); });
    return out;
}

bool DirectoryModel::directoryHasImages(const std::string& dir) {
    GDir* d = g_dir_open(dir.c_str(), 0, nullptr);
    if(!d) return false;
    bool found = false;
    while(const gchar* name = g_dir_read_name(d)) {
        if(name[0] == '.') continue;
        if(isSupportedImageFile(name)) {
            found = true;
            break;
        }
    }
    g_dir_close(d);
    return found;
}

}  // namespace qimgv
