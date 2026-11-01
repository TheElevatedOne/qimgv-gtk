#include "core/system.hpp"

#include <gio/gio.h>

#include <cstring>
#include <vector>

namespace qimgv {

static bool processRunning(const char* name) {
    GDir* d = g_dir_open("/proc", 0, nullptr);
    if(!d) return false;
    bool found = false;
    while(const gchar* e = g_dir_read_name(d)) {
        if(!g_ascii_isdigit(e[0])) continue;
        gchar* path = g_build_filename("/proc", e, "comm", nullptr);
        gchar* comm = nullptr;
        if(g_file_get_contents(path, &comm, nullptr, nullptr)) {
            g_strstrip(comm);
            if(!strcmp(comm, name)) found = true;
        }
        g_free(comm);
        g_free(path);
        if(found) break;
    }
    g_dir_close(d);
    return found;
}

static bool have(const char* prog) {
    gchar* p = g_find_program_in_path(prog);
    bool r = p != nullptr;
    g_free(p);
    return r;
}

static bool spawnSync(std::vector<std::string> args) {
    std::vector<gchar*> argv;
    for(auto& a : args) argv.push_back(&a[0]);
    argv.push_back(nullptr);
    gint status = 0;
    GError* e = nullptr;
    bool ok = g_spawn_sync(nullptr, argv.data(), nullptr,
                           (GSpawnFlags)(G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL),
                           nullptr, nullptr, nullptr, nullptr, &status, &e);
    if(e) g_error_free(e);
    return ok && g_spawn_check_wait_status(status, nullptr);
}

bool setWallpaper(const std::string& path, std::string& message) {
    const char* desktop = g_getenv("XDG_CURRENT_DESKTOP");
    std::string de = desktop ? desktop : "";
    if(g_getenv("HYPRLAND_INSTANCE_SIGNATURE") && processRunning("hyprpaper") && have("hyprctl")) {
        if(spawnSync({"hyprctl", "hyprpaper", "reload", "," + path})) {
            message = "Wallpaper set (hyprpaper)";
            return true;
        }
    }
    if((processRunning("swww-daemon") || processRunning("awww-daemon")) && (have("swww") || have("awww"))) {
        if(spawnSync({have("swww") ? "swww" : "awww", "img", path})) {
            message = "Wallpaper set (swww)";
            return true;
        }
    }
    if(de.find("KDE") != std::string::npos && have("plasma-apply-wallpaperimage")) {
        if(spawnSync({"plasma-apply-wallpaperimage", path})) {
            message = "Wallpaper set";
            return true;
        }
    }
    if(de.find("GNOME") != std::string::npos || de.find("Unity") != std::string::npos ||
       de.find("Budgie") != std::string::npos) {
        gchar* uri = g_filename_to_uri(path.c_str(), nullptr, nullptr);
        bool ok = uri && spawnSync({"gsettings", "set", "org.gnome.desktop.background", "picture-uri", uri});
        if(uri) spawnSync({"gsettings", "set", "org.gnome.desktop.background", "picture-uri-dark", uri});
        g_free(uri);
        if(ok) {
            message = "Wallpaper set";
            return true;
        }
    }
    if(de.find("XFCE") != std::string::npos && have("xfconf-query")) {
        if(spawnSync({"xfconf-query", "-c", "xfce4-desktop", "-p",
                      "/backdrop/screen0/monitor0/workspace0/last-image", "-s", path})) {
            message = "Wallpaper set";
            return true;
        }
    }
    if(g_getenv("DISPLAY") && !g_getenv("WAYLAND_DISPLAY") && have("feh")) {
        if(spawnSync({"feh", "--bg-fill", path})) {
            message = "Wallpaper set (feh)";
            return true;
        }
    }
    message = "No supported wallpaper tool found (hyprpaper, swww, GNOME, KDE, XFCE, feh)";
    return false;
}

void showInFileManager(const std::string& path) {
    gchar* uri = g_filename_to_uri(path.c_str(), nullptr, nullptr);
    if(!uri) return;
    GDBusConnection* bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
    if(bus) {
        const gchar* uris[] = {uri, nullptr};
        g_dbus_connection_call(
            bus, "org.freedesktop.FileManager1", "/org/freedesktop/FileManager1", "org.freedesktop.FileManager1",
            "ShowItems", g_variant_new("(^ass)", uris, ""), nullptr, G_DBUS_CALL_FLAGS_NONE, 3000, nullptr,
            [](GObject* src, GAsyncResult* res, gpointer data) {
                gchar* dir = static_cast<gchar*>(data);
                GError* e = nullptr;
                GVariant* r = g_dbus_connection_call_finish(G_DBUS_CONNECTION(src), res, &e);
                if(r) {
                    g_variant_unref(r);
                } else {
                    if(e) g_error_free(e);
                    gchar* duri = g_filename_to_uri(dir, nullptr, nullptr);
                    if(duri) g_app_info_launch_default_for_uri(duri, nullptr, nullptr);
                    g_free(duri);
                }
                g_free(dir);
            },
            g_path_get_dirname(path.c_str()));
        g_object_unref(bus);
    } else {
        gchar* dir = g_path_get_dirname(path.c_str());
        gchar* duri = g_filename_to_uri(dir, nullptr, nullptr);
        if(duri) g_app_info_launch_default_for_uri(duri, nullptr, nullptr);
        g_free(duri);
        g_free(dir);
    }
    g_free(uri);
}

bool runScript(const Script& script, const std::string& path, std::string& err) {
    std::vector<std::string> args;
    if(script.isFile) {
        args = {script.command, path};
    } else {
        gchar* quoted = g_shell_quote(path.c_str());
        std::string cmd = script.command;
        for(size_t pos; (pos = cmd.find("%file%")) != std::string::npos;) cmd.replace(pos, 6, quoted);
        g_free(quoted);
        args = {"/bin/sh", "-c", cmd};
    }
    std::vector<gchar*> argv;
    for(auto& a : args) argv.push_back(&a[0]);
    argv.push_back(nullptr);
    GError* e = nullptr;
    gchar* dir = g_path_get_dirname(path.c_str());
    bool ok = g_spawn_async(dir, argv.data(), nullptr, G_SPAWN_SEARCH_PATH, nullptr, nullptr, nullptr, &e);
    g_free(dir);
    if(!ok) {
        err = e ? e->message : "Could not start script";
        if(e) g_error_free(e);
    }
    return ok;
}

}  // namespace qimgv
