#include "settings.hpp"

#include <glib.h>
#include <glib/gstdio.h>

#include <algorithm>

#include "actions.hpp"

namespace qimgv {

Settings& settings() {
    static Settings s;
    return s;
}

std::string Settings::configDir() {
    gchar* d = g_build_filename(g_get_user_config_dir(), "qimgv", nullptr);
    std::string r = d;
    g_free(d);
    return r;
}

std::string Settings::cacheDir() {
    gchar* d = g_build_filename(g_get_user_cache_dir(), "qimgv", nullptr);
    std::string r = d;
    g_free(d);
    return r;
}

std::string Settings::thumbnailDir() { return cacheDir() + "/thumbnails"; }

static std::string configPath() { return Settings::configDir() + "/qimgv-gtk.conf"; }

namespace {
struct Reader {
    GKeyFile* kf;
    bool b(const char* g, const char* k, bool def) {
        GError* e = nullptr;
        bool v = g_key_file_get_boolean(kf, g, k, &e);
        if(e) {
            g_error_free(e);
            return def;
        }
        return v;
    }
    int i(const char* g, const char* k, int def, int lo = INT32_MIN, int hi = INT32_MAX) {
        GError* e = nullptr;
        int v = g_key_file_get_integer(kf, g, k, &e);
        if(e) {
            g_error_free(e);
            return def;
        }
        return std::clamp(v, lo, hi);
    }
    double d(const char* g, const char* k, double def) {
        GError* e = nullptr;
        double v = g_key_file_get_double(kf, g, k, &e);
        if(e) {
            g_error_free(e);
            return def;
        }
        return v;
    }
    std::vector<std::string> list(const char* g, const char* k) {
        std::vector<std::string> out;
        gsize n = 0;
        gchar** l = g_key_file_get_string_list(kf, g, k, &n, nullptr);
        for(gsize j = 0; l && j < n; j++) out.emplace_back(l[j]);
        g_strfreev(l);
        return out;
    }
};
}  // namespace

void Settings::load() {
    shortcuts = defaultShortcuts();
    savedPaths.assign(9, std::string());
    const char* home = g_get_home_dir();
    for(auto& p : savedPaths) p = home;
    GKeyFile* kf = g_key_file_new();
    if(!g_key_file_load_from_file(kf, configPath().c_str(), G_KEY_FILE_NONE, nullptr)) {
        g_key_file_free(kf);
        return;
    }
    Reader r{kf};
    defaultViewMode = (ViewMode)r.i("General", "defaultViewMode", 0, 0, 1);
    sortMode = (SortMode)r.i("General", "sortingMode", 0, 0, 5);
    folderEndAction = (FolderEndAction)r.i("General", "folderEndAction", 0, 0, 2);
    confirmDelete = r.b("General", "confirmDelete", confirmDelete);
    confirmTrash = r.b("General", "confirmTrash", confirmTrash);
    showSaveOverlay = r.b("General", "showSaveOverlay", showSaveOverlay);
    usePreloader = r.b("General", "usePreloader", usePreloader);
    slideshowInterval = r.i("General", "slideshowInterval", slideshowInterval, 100, 3600000);
    cursorAutohide = r.b("General", "cursorAutohide", cursorAutohide);
    fullscreenOnStart = r.b("General", "fullscreenOnStart", fullscreenOnStart);
    windowTitleExtendedInfo = r.b("General", "windowTitleExtendedInfo", windowTitleExtendedInfo);
    jpegQuality = r.i("General", "jpegQuality", jpegQuality, 1, 100);

    fitMode = (FitMode)r.i("View", "fitMode", 0, 0, 3);
    expandImage = r.b("View", "expandImage", expandImage);
    expandLimit = r.i("View", "expandLimit", expandLimit, 0, 8);
    zoomStep = std::clamp(r.d("View", "zoomStep", zoomStep), 0.01, 1.0);
    transparencyGrid = r.b("View", "transparencyGrid", transparencyGrid);
    zoomIndicator = (ZoomIndicator)r.i("View", "zoomIndicator", 2, 0, 2);
    focusPoint = (FocusPoint)r.i("View", "focusPoint", 2, 0, 2);
    infoBarWindowed = r.b("View", "infoBarWindowed", infoBarWindowed);
    infoBarFullscreen = r.b("View", "infoBarFullscreen", infoBarFullscreen);
    panelEnabled = r.b("View", "panelEnabled", panelEnabled);
    panelPinned = r.b("View", "panelPinned", panelPinned);
    panelFullscreenOnly = r.b("View", "panelFullscreenOnly", panelFullscreenOnly);
    panelPosition = (PanelPosition)r.i("View", "panelPosition", 0, 0, 1);
    panelThumbSize = r.i("View", "panelThumbSize", panelThumbSize, 60, 400);
    squareThumbnails = r.b("View", "squareThumbnails", squareThumbnails);
    wheelScrollsZoomed = r.b("View", "wheelScrollsZoomed", wheelScrollsZoomed);
    clickableEdges = r.b("View", "clickableEdges", clickableEdges);
    mapOverlay = r.b("View", "mapOverlay", mapOverlay);
    keepZoom = r.b("View", "keepZoom", keepZoom);

    scalingFilter = (ScalingFilter)r.i("Scaling", "filter", 2, 0, 2);
    smoothUpscaling = r.b("Scaling", "smoothUpscaling", smoothUpscaling);
    smoothAnimated = r.b("Scaling", "smoothAnimated", smoothAnimated);

    folderViewIconSize = r.i("FolderView", "iconSize", folderViewIconSize, 80, 400);
    placesPanel = r.b("FolderView", "placesPanel", placesPanel);
    placesPanelWidth = r.i("FolderView", "placesPanelWidth", placesPanelWidth, 100, 800);
    bookmarks = r.list("FolderView", "bookmarks");
    auto sp = r.list("General", "savedPaths");
    for(size_t j = 0; j < sp.size() && j < 9; j++) savedPaths[j] = sp[j];

    useThumbnailCache = r.b("Advanced", "useThumbnailCache", useThumbnailCache);
    thumbnailerThreads = r.i("Advanced", "thumbnailerThreads", thumbnailerThreads, 1, 32);
    rawEmbeddedPreview = r.b("Advanced", "rawEmbeddedPreview", rawEmbeddedPreview);
    animations = r.b("Advanced", "animations", animations);

    windowWidth = r.i("Window", "width", windowWidth, 200, 16384);
    windowHeight = r.i("Window", "height", windowHeight, 150, 16384);
    maximized = r.b("Window", "maximized", maximized);

    if(g_key_file_has_group(kf, "Shortcuts")) {
        shortcuts.clear();
        gsize n = 0;
        gchar** keys = g_key_file_get_keys(kf, "Shortcuts", &n, nullptr);
        for(gsize j = 0; j < n; j++) {
            gchar* v = g_key_file_get_string(kf, "Shortcuts", keys[j], nullptr);
            // Keys are stored escaped: the key file format cannot hold '=' or '[' in key names.
            gchar* seq = g_uri_unescape_string(keys[j], nullptr);
            if(v && seq) shortcuts[seq] = v;
            g_free(seq);
            g_free(v);
        }
        g_strfreev(keys);
    }
    if(g_key_file_has_group(kf, "Scripts")) {
        gsize n = 0;
        gchar** keys = g_key_file_get_keys(kf, "Scripts", &n, nullptr);
        for(gsize j = 0; j < n; j++) {
            gchar* v = g_key_file_get_string(kf, "Scripts", keys[j], nullptr);
            gchar* name = g_uri_unescape_string(keys[j], nullptr);
            if(v && name && strlen(v) >= 2) {
                Script s;
                s.isFile = v[0] == 'f';
                s.command = v + 2;
                scripts[name] = s;
            }
            g_free(name);
            g_free(v);
        }
        g_strfreev(keys);
    }
    g_key_file_free(kf);
}

void Settings::save() const {
    GKeyFile* kf = g_key_file_new();
    auto b = [&](const char* g, const char* k, bool v) { g_key_file_set_boolean(kf, g, k, v); };
    auto i = [&](const char* g, const char* k, int v) { g_key_file_set_integer(kf, g, k, v); };
    auto list = [&](const char* g, const char* k, const std::vector<std::string>& v) {
        std::vector<const gchar*> p;
        for(auto& s : v) p.push_back(s.c_str());
        g_key_file_set_string_list(kf, g, k, p.data(), p.size());
    };
    i("General", "defaultViewMode", (int)defaultViewMode);
    i("General", "sortingMode", (int)sortMode);
    i("General", "folderEndAction", (int)folderEndAction);
    b("General", "confirmDelete", confirmDelete);
    b("General", "confirmTrash", confirmTrash);
    b("General", "showSaveOverlay", showSaveOverlay);
    b("General", "usePreloader", usePreloader);
    i("General", "slideshowInterval", slideshowInterval);
    b("General", "cursorAutohide", cursorAutohide);
    b("General", "fullscreenOnStart", fullscreenOnStart);
    b("General", "windowTitleExtendedInfo", windowTitleExtendedInfo);
    i("General", "jpegQuality", jpegQuality);
    list("General", "savedPaths", savedPaths);

    i("View", "fitMode", (int)(fitMode == FitMode::Free ? FitMode::Window : fitMode));
    b("View", "expandImage", expandImage);
    i("View", "expandLimit", expandLimit);
    g_key_file_set_double(kf, "View", "zoomStep", zoomStep);
    b("View", "transparencyGrid", transparencyGrid);
    i("View", "zoomIndicator", (int)zoomIndicator);
    i("View", "focusPoint", (int)focusPoint);
    b("View", "infoBarWindowed", infoBarWindowed);
    b("View", "infoBarFullscreen", infoBarFullscreen);
    b("View", "panelEnabled", panelEnabled);
    b("View", "panelPinned", panelPinned);
    b("View", "panelFullscreenOnly", panelFullscreenOnly);
    i("View", "panelPosition", (int)panelPosition);
    i("View", "panelThumbSize", panelThumbSize);
    b("View", "squareThumbnails", squareThumbnails);
    b("View", "wheelScrollsZoomed", wheelScrollsZoomed);
    b("View", "clickableEdges", clickableEdges);
    b("View", "mapOverlay", mapOverlay);
    b("View", "keepZoom", keepZoom);

    i("Scaling", "filter", (int)scalingFilter);
    b("Scaling", "smoothUpscaling", smoothUpscaling);
    b("Scaling", "smoothAnimated", smoothAnimated);

    i("FolderView", "iconSize", folderViewIconSize);
    b("FolderView", "placesPanel", placesPanel);
    i("FolderView", "placesPanelWidth", placesPanelWidth);
    list("FolderView", "bookmarks", bookmarks);

    b("Advanced", "useThumbnailCache", useThumbnailCache);
    i("Advanced", "thumbnailerThreads", thumbnailerThreads);
    b("Advanced", "rawEmbeddedPreview", rawEmbeddedPreview);
    b("Advanced", "animations", animations);

    i("Window", "width", windowWidth);
    i("Window", "height", windowHeight);
    b("Window", "maximized", maximized);

    for(auto& [seq, action] : shortcuts) {
        gchar* k = g_uri_escape_string(seq.c_str(), "+_", FALSE);
        g_key_file_set_string(kf, "Shortcuts", k, action.c_str());
        g_free(k);
    }
    for(auto& [name, s] : scripts) {
        gchar* k = g_uri_escape_string(name.c_str(), nullptr, FALSE);
        std::string v = std::string(s.isFile ? "f:" : "c:") + s.command;
        g_key_file_set_string(kf, "Scripts", k, v.c_str());
        g_free(k);
    }
    g_mkdir_with_parents(configDir().c_str(), 0755);
    GError* e = nullptr;
    if(!g_key_file_save_to_file(kf, configPath().c_str(), &e)) {
        g_printerr("qimgv: could not save settings: %s\n", e->message);
        g_error_free(e);
    }
    g_key_file_free(kf);
}

}  // namespace qimgv
