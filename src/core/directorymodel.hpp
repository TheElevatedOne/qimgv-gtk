// The list of images in the current directory (sorted, watched for changes).
// Videos and unsupported files are skipped.
#pragma once

#include <gio/gio.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "settings.hpp"

namespace qimgv {

struct FileEntry {
    std::string path;
    std::string name;
    std::string collateKey;
    int64_t size = 0;
    int64_t mtime = 0;  // microseconds
};

class DirectoryModel {
public:
    DirectoryModel();
    ~DirectoryModel();

    // Load a directory. Returns false if it can't be read.
    bool setDirectory(const std::string& dir);
    void reload();
    const std::string& directory() const { return mDir; }

    const std::vector<FileEntry>& files() const { return mFiles; }
    const std::vector<std::string>& subdirs() const { return mSubdirs; }
    int count() const { return (int)mFiles.size(); }
    int indexOf(const std::string& path) const;
    const FileEntry* at(int i) const { return i >= 0 && i < count() ? &mFiles[i] : nullptr; }

    void setSortMode(SortMode m);
    SortMode sortMode() const { return mSort; }

    // Called (debounced, on the main thread) after files were added/removed/changed.
    std::function<void()> onChanged;
    // Called when a single file was modified in place (reload it).
    std::function<void(const std::string&)> onFileModified;

    // Apply local changes immediately (after our own rename/delete) without waiting for the monitor.
    void removePath(const std::string& path);
    void renamePath(const std::string& from, const std::string& to);

    // Sibling directories of the current one (sorted by name).
    std::vector<std::string> siblingDirectories() const;
    static bool directoryHasImages(const std::string& dir);

private:
    void sort();
    bool listDirectory(const std::string& dir);
    void startMonitor();
    void stopMonitor();
    static void monitorCb(GFileMonitor*, GFile* file, GFile* other, GFileMonitorEvent ev, gpointer self);
    static gboolean debounceCb(gpointer self);

    std::string mDir;
    std::vector<FileEntry> mFiles;
    std::vector<std::string> mSubdirs;
    SortMode mSort = SortMode::Name;
    GFileMonitor* mMonitor = nullptr;
    guint mDebounce = 0;
};

bool statEntry(const std::string& path, FileEntry& e);

}  // namespace qimgv
