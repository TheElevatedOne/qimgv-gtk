// qimgv: fast and easy to use image viewer (GTK 3).
#include <gtk/gtk.h>

#include <cstdio>
#include <cstring>
#include <condition_variable>
#include <mutex>
#include <string>

#include "config.hpp"
#include "core/directorymodel.hpp"
#include "core/thumbnailer.hpp"
#include "core/workers.hpp"
#include "gui/mainwindow.hpp"
#include "gui/style.hpp"
#include "image/formats.hpp"
#include "image/metadata.hpp"
#include "settings.hpp"

using namespace qimgv;

// --gen-thumbs: fill the disk thumbnail cache for a directory (no display needed).
static int generateThumbnails(const std::string& dir, int size) {
    DirectoryModel model;
    if(!model.setDirectory(dir)) {
        fprintf(stderr, "qimgv: cannot open directory %s\n", dir.c_str());
        return 1;
    }
    int n = model.count(), done = 0, failed = 0;
    printf("Generating %d thumbnails (%d px) for %s\n", n, size, dir.c_str());
    ThreadPool pool(std::max(1, settings().thumbnailerThreads));
    std::mutex m;
    std::condition_variable cv;
    for(auto& f : model.files()) {
        std::string path = f.path;
        pool.submit([&, path] {
            cairo_surface_t* s = Thumbnailer::loadOrGenerate(path, size, false, true);
            std::lock_guard<std::mutex> l(m);
            if(s)
                cairo_surface_destroy(s);
            else
                failed++;
            done++;
            printf("\r%d / %d", done, n);
            fflush(stdout);
            cv.notify_one();
        });
    }
    std::unique_lock<std::mutex> l(m);
    cv.wait(l, [&] { return done == n; });
    printf("\nDone. %d failed.\n", failed);
    return failed ? 2 : 0;
}

int main(int argc, char** argv) {
    g_set_prgname("qimgv");  // Wayland app id / X11 WM_CLASS, matches qimgv.desktop
    g_set_application_name("qimgv");

    gboolean version = FALSE;
    gchar* genThumbs = nullptr;
    gint genThumbsSize = 0;
    gchar** files = nullptr;
    GOptionEntry entries[] = {
        {"version", 'v', 0, G_OPTION_ARG_NONE, &version, "Show version and exit", nullptr},
        {"gen-thumbs", 0, 0, G_OPTION_ARG_FILENAME, &genThumbs, "Generate all thumbnails for a directory", "DIR"},
        {"gen-thumbs-size", 0, 0, G_OPTION_ARG_INT, &genThumbsSize, "Thumbnail size for --gen-thumbs (default 128)",
         "SIZE"},
        {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &files, nullptr, "[FILE|DIRECTORY]"},
        {nullptr, 0, 0, G_OPTION_ARG_NONE, nullptr, nullptr, nullptr},
    };
    GOptionContext* ctx = g_option_context_new("- image viewer");
    g_option_context_set_summary(ctx, "Fast and easy to use image viewer.\n"
                                      "Supports JPEG, PNG, APNG, GIF, WebP, JPEG XL, AVIF, HEIF/HEIC, camera RAW, "
                                      "BMP, TIFF, SVG and more.");
    g_option_context_add_main_entries(ctx, entries, nullptr);
    g_option_context_add_group(ctx, gtk_get_option_group(FALSE));
    GError* err = nullptr;
    if(!g_option_context_parse(ctx, &argc, &argv, &err)) {
        fprintf(stderr, "qimgv: %s\nRun 'qimgv --help' for usage.\n", err->message);
        g_error_free(err);
        g_option_context_free(ctx);
        return 1;
    }
    g_option_context_free(ctx);

    if(version) {
        printf("qimgv %s (GTK %d.%d.%d)\n", QIMGV_VERSION, gtk_get_major_version(), gtk_get_minor_version(),
               gtk_get_micro_version());
        return 0;
    }

    initMetadata();
    settings().load();

    if(genThumbs) {
        int size = genThumbsSize > 0 ? genThumbsSize : 128;
        int r = generateThumbnails(genThumbs, size);
        g_free(genThumbs);
        g_strfreev(files);
        return r;
    }

    if(!gtk_init_check(nullptr, nullptr)) {
        fprintf(stderr, "qimgv: cannot open display\n");
        return 1;
    }
    installStyle();
    std::string path = files && files[0] ? files[0] : "";
    g_strfreev(files);

    {
        MainWindow window;
        window.show();
        window.open(path);
        if(const char* snap = g_getenv("QIMGV_SNAPSHOT")) {
            const char* acts = g_getenv("QIMGV_SNAPSHOT_ACTIONS");
            window.scheduleSnapshot(snap, acts ? acts : "");
        }
        gtk_main();
    }
    return 0;
}
