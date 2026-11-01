#include "core/fileops.hpp"

#include <gio/gio.h>
#include <unistd.h>

namespace qimgv {

std::string baseName(const std::string& path) {
    gchar* b = g_path_get_basename(path.c_str());
    std::string r = b;
    g_free(b);
    return r;
}

std::string dirName(const std::string& path) {
    gchar* d = g_path_get_dirname(path.c_str());
    std::string r = d;
    g_free(d);
    return r;
}

std::string joinPath(const std::string& dir, const std::string& name) {
    gchar* p = g_build_filename(dir.c_str(), name.c_str(), nullptr);
    std::string r = p;
    g_free(p);
    return r;
}

bool isDirectory(const std::string& path) { return g_file_test(path.c_str(), G_FILE_TEST_IS_DIR); }

bool isWritableDir(const std::string& path) { return isDirectory(path) && access(path.c_str(), W_OK) == 0; }

std::string humanSize(long long bytes) {
    gchar* s = g_format_size_full((guint64)bytes, G_FORMAT_SIZE_IEC_UNITS);
    std::string r = s;
    g_free(s);
    return r;
}

static FileOpResult transfer(const std::string& src, const std::string& dstDir, bool overwrite, bool move,
                             std::string& err) {
    std::string dst = joinPath(dstDir, baseName(src));
    if(dst == src) {
        err = "Source and destination are the same";
        return FileOpResult::Error;
    }
    if(!overwrite && g_file_test(dst.c_str(), G_FILE_TEST_EXISTS)) return FileOpResult::Exists;
    GFile* s = g_file_new_for_path(src.c_str());
    GFile* d = g_file_new_for_path(dst.c_str());
    GError* e = nullptr;
    GFileCopyFlags flags = (GFileCopyFlags)(G_FILE_COPY_ALL_METADATA | (overwrite ? G_FILE_COPY_OVERWRITE : 0));
    bool ok = move ? g_file_move(s, d, flags, nullptr, nullptr, nullptr, &e)
                   : g_file_copy(s, d, flags, nullptr, nullptr, nullptr, &e);
    if(!ok) {
        err = e ? e->message : "Unknown error";
        if(e) g_error_free(e);
    }
    g_object_unref(s);
    g_object_unref(d);
    return ok ? FileOpResult::Ok : FileOpResult::Error;
}

FileOpResult copyToDir(const std::string& src, const std::string& dstDir, bool overwrite, std::string& err) {
    return transfer(src, dstDir, overwrite, false, err);
}

FileOpResult moveToDir(const std::string& src, const std::string& dstDir, bool overwrite, std::string& err) {
    return transfer(src, dstDir, overwrite, true, err);
}

bool moveToTrash(const std::string& path, std::string& err) {
    GFile* f = g_file_new_for_path(path.c_str());
    GError* e = nullptr;
    bool ok = g_file_trash(f, nullptr, &e);
    if(!ok) {
        err = e ? e->message : "Unknown error";
        if(e) g_error_free(e);
    }
    g_object_unref(f);
    return ok;
}

bool deleteFile(const std::string& path, std::string& err) {
    GFile* f = g_file_new_for_path(path.c_str());
    GError* e = nullptr;
    bool ok = g_file_delete(f, nullptr, &e);
    if(!ok) {
        err = e ? e->message : "Unknown error";
        if(e) g_error_free(e);
    }
    g_object_unref(f);
    return ok;
}

FileOpResult renameFile(const std::string& path, const std::string& newName, bool overwrite, std::string& newPath,
                        std::string& err) {
    if(newName.empty() || newName.find('/') != std::string::npos) {
        err = "Invalid file name";
        return FileOpResult::Error;
    }
    newPath = joinPath(dirName(path), newName);
    if(newPath == path) return FileOpResult::Ok;
    if(!overwrite && g_file_test(newPath.c_str(), G_FILE_TEST_EXISTS)) return FileOpResult::Exists;
    GFile* s = g_file_new_for_path(path.c_str());
    GFile* d = g_file_new_for_path(newPath.c_str());
    GError* e = nullptr;
    bool ok = g_file_move(s, d, overwrite ? G_FILE_COPY_OVERWRITE : G_FILE_COPY_NONE, nullptr, nullptr, nullptr, &e);
    if(!ok) {
        err = e ? e->message : "Unknown error";
        if(e) g_error_free(e);
    }
    g_object_unref(s);
    g_object_unref(d);
    return ok ? FileOpResult::Ok : FileOpResult::Error;
}

}  // namespace qimgv
