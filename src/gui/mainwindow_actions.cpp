// Action dispatch and the operations behind it (editing, saving, file management, ...).
#include <glib/gstdio.h>

#include <algorithm>
#include <cmath>

#include "core/fileops.hpp"
#include "core/system.hpp"
#include "gui/dialogs.hpp"
#include "gui/mainwindow.hpp"
#include "gui/settingsdialog.hpp"
#include "gui/style.hpp"
#include "image/encoder.hpp"
#include "image/formats.hpp"
#include "image/metadata.hpp"

namespace qimgv {

void MainWindow::action(const std::string& a, double x, double y) {
    if(a.rfind("s:", 0) == 0) return runUserScript(a.substr(2));
    bool doc = mMode == ViewMode::Document;

    // Navigation
    if(a == "nextImage") return doc ? navigate(1) : (void)0;
    if(a == "prevImage") return doc ? navigate(-1) : (void)0;
    if(a == "jumpToFirst") return doc ? jumpTo(0) : (void)0;
    if(a == "jumpToLast") return doc ? jumpTo(mDir.count() - 1) : (void)0;
    if(a == "nextDirectory") return changeDirectory(1);
    if(a == "prevDirectory") return changeDirectory(-1);

    // View
    if(a == "toggleFullscreen") return toggleFullscreen();
    if(a == "closeFullScreenOrExit") return closeFullscreenOrExit();
    if(a == "exit") return quit();
    if(a == "fitWindow") return mViewer->setFitMode(FitMode::Window);
    if(a == "fitWidth") return mViewer->setFitMode(FitMode::Width);
    if(a == "fitNormal") return mViewer->setFitMode(FitMode::Normal);
    if(a == "fitWindowStretch") return mViewer->setFitMode(FitMode::Stretch);
    if(a == "toggleFitMode") {
        FitMode m = mViewer->fitMode();
        FitMode next = m == FitMode::Window ? FitMode::Width : m == FitMode::Width ? FitMode::Normal : FitMode::Window;
        return mViewer->setFitMode(next);
    }
    if(a == "zoomIn") return mViewer->zoomIn(false);
    if(a == "zoomOut") return mViewer->zoomOut(false);
    if(a == "zoomInCursor") return mViewer->zoomIn(true);
    if(a == "zoomOutCursor") return mViewer->zoomOut(true);
    if(a == "lockZoom") {
        settings().keepZoom = !settings().keepZoom;
        mViewer->setKeepZoom(settings().keepZoom);
        return showMessage(settings().keepZoom ? "Zoom level is kept between images" : "Zoom level is reset per image");
    }
    if(a == "scrollUp") return mViewer->scrollBy(0, -120);
    if(a == "scrollDown") return mViewer->scrollBy(0, 120);
    if(a == "scrollLeft") return mViewer->scrollBy(-120, 0);
    if(a == "scrollRight") return mViewer->scrollBy(120, 0);
    if(a == "toggleTransparencyGrid") {
        settings().transparencyGrid = !settings().transparencyGrid;
        gtk_widget_queue_draw(mViewer->widget());
        return;
    }
    if(a == "toggleScalingFilter") {
        int f = ((int)settings().scalingFilter + 1) % 3;
        settings().scalingFilter = (ScalingFilter)f;
        mViewer->settingsChanged();
        static const char* names[] = {"Scaling: nearest", "Scaling: bilinear", "Scaling: high quality"};
        return showMessage(names[f]);
    }
    if(a == "toggleFullscreenInfoBar") {
        settings().infoBarFullscreen = !settings().infoBarFullscreen;
        return updateChrome();
    }
    if(a == "toggleImageInfo") return toggleImageInfo();
    if(a == "toggleSlideshow") return toggleSlideshow();
    if(a == "toggleShuffle") return toggleShuffle();
    if(a == "contextMenu") {
        if(!doc) return;
        if(x < 0) {
            x = mPointerX >= 0 ? mPointerX : gtk_widget_get_allocated_width(mOverlay) / 2.0;
            y = mPointerY >= 0 ? mPointerY : gtk_widget_get_allocated_height(mOverlay) / 2.0;
        }
        return showContextMenu(x, y);
    }

    // Modes
    if(a == "folderView") return setMode(doc ? ViewMode::Folder : ViewMode::Document);
    if(a == "toggleFolderView") return setMode(doc ? ViewMode::Folder : ViewMode::Document);
    if(a == "documentView") {
        setMode(ViewMode::Document);
        if(mIndex >= 0 && !mImage) loadIndex(mIndex);
        return;
    }
    if(a == "goUp") {
        std::string dir = mDir.directory();
        if(dir.empty() || dir == "/") return;
        std::string up = dirName(dir);
        if(setDirectory(up)) {
            mIndex = -1;
            mCurrentPath.clear();
            mImage.reset();
            mViewer->clear();
            refreshLists();
            // Select the folder we came from.
            auto& subs = mDir.subdirs();
            auto it = std::find(subs.begin(), subs.end(), dir);
            if(it != subs.end()) mFolder->grid().setCurrent((int)(it - subs.begin()), true);
            updateInfo();
        }
        return;
    }
    if(a == "openSettings") {
        return SettingsDialog::open(GTK_WINDOW(mWindow), [this] { applySettings(); });
    }
    if(a == "open") return openFileDialog();
    if(a == "sortByName") return setSort(settings().sortMode == SortMode::Name ? SortMode::NameDesc : SortMode::Name);
    if(a == "sortByTime")
        return setSort(settings().sortMode == SortMode::TimeNewest ? SortMode::TimeOldest : SortMode::TimeNewest);
    if(a == "sortBySize") return setSort(settings().sortMode == SortMode::SizeDesc ? SortMode::Size : SortMode::SizeDesc);

    // File operations (work on the selected file in folder view too)
    if(a == "moveToTrash") return removeCurrent(true);
    if(a == "removeFile") return removeCurrent(false);
    if(a == "copyFile" || a == "moveFile") {
        if(!currentEntry()) return;
        bool move = a == "moveFile";
        if(mCopy->visible() && mCopy->moveMode() == move) return mCopy->close();
        return mCopy->open(move);
    }
    if(a == "renameFile") {
        if(const FileEntry* e = currentEntry()) {
            if(!doc) setMode(ViewMode::Document);
            mRename->open(e->name);
        }
        return;
    }
    if(a == "copyFileClipboard") return copyFileToClipboard();
    if(a == "copyPathClipboard") return copyPathToClipboard();
    if(a == "pasteFile") return pasteFiles();
    if(a == "showInDirectory") {
        if(const FileEntry* e = currentEntry()) showInFileManager(e->path);
        return;
    }
    if(a == "reloadImage") {
        if(mIndex < 0) return;
        mLoader.invalidate(mCurrentPath);
        mThumbs.forget(mCurrentPath);
        return loadIndex(mIndex);
    }
    if(a == "setWallpaper") {
        if(const FileEntry* e = currentEntry()) {
            std::string msg;
            bool ok = setWallpaper(e->path, msg);
            showMessage(msg, ok ? "notifications/success16" : nullptr, !ok);
        }
        return;
    }
    if(a == "print") return print();

    // Editing (document view only)
    if(!doc) return;
    if(a == "rotateRight") return rotate(90);
    if(a == "rotateLeft") return rotate(270);
    if(a == "flipH") return flip(true);
    if(a == "flipV") return flip(false);
    if(a == "crop") return mViewer->cropping() ? cancelCrop() : startCrop();
    if(a == "resize") return resize();
    if(a == "save") {
        if(mImage) saveImage(mCurrentPath);
        return;
    }
    if(a == "saveAs") return saveAs();
    if(a == "discardEdits") return discardEdits();
}

// ---- editing ----------------------------------------------------------------

bool MainWindow::ensureEditable() {
    if(!mImage) return false;
    if(!mEdited) {
        // Edit a private copy: the original may be shared with the cache.
        mImage = mImage->clone();
        mViewer->setImage(mImage);
    }
    return true;
}

void MainWindow::markEdited() {
    mEdited = true;
    mViewer->imageEdited();
    mSaveConfirm->setVisible(settings().showSaveOverlay);
    updateInfo();
}

void MainWindow::rotate(int degrees) {
    if(!ensureEditable()) return;
    std::vector<Frame> frames;
    for(auto& f : mImage->frames) frames.push_back({rotateSurface(f.surface, degrees), f.delayMs});
    mImage->setFrames(std::move(frames));
    markEdited();
}

void MainWindow::flip(bool horizontal) {
    if(!ensureEditable()) return;
    std::vector<Frame> frames;
    for(auto& f : mImage->frames) frames.push_back({flipSurface(f.surface, horizontal), f.delayMs});
    mImage->setFrames(std::move(frames));
    markEdited();
}

void MainWindow::startCrop() {
    if(!mImage) return;
    if(mImage->animated()) return showMessage("Cropping animated images is not supported", nullptr, true);
    mMenu->hide();
    mPanel->reveal(false);
    mCrop->open(mImage->width, mImage->height);
    mViewer->startCrop();
}

void MainWindow::cancelCrop() {
    mViewer->endCrop();
    mCrop->close();
    mViewer->setFitMode(settings().fitMode);
}

void MainWindow::finishCrop(bool save) {
    CropRect r = mViewer->cropRect();
    mViewer->endCrop();
    mCrop->close();
    if(!mImage || r.w <= 0 || r.h <= 0) return;
    if(r.x == 0 && r.y == 0 && r.w == mImage->width && r.h == mImage->height) {
        mViewer->setFitMode(settings().fitMode);
        return;
    }
    if(!ensureEditable()) return;
    std::vector<Frame> frames;
    for(auto& f : mImage->frames) frames.push_back({cropSurface(f.surface, r.x, r.y, r.w, r.h), f.delayMs});
    mImage->setFrames(std::move(frames));
    markEdited();
    if(save) saveImage(mCurrentPath);
}

void MainWindow::resize() {
    if(!mImage) return;
    if(mImage->animated()) return showMessage("Resizing animated images is not supported", nullptr, true);
    auto r = askResize(GTK_WINDOW(mWindow), mImage->width, mImage->height);
    if(!r || (r->first == mImage->width && r->second == mImage->height)) return;
    if(!ensureEditable()) return;
    std::vector<Frame> frames;
    for(auto& f : mImage->frames) frames.push_back({scaleSurface(f.surface, r->first, r->second, true), f.delayMs});
    mImage->setFrames(std::move(frames));
    markEdited();
}

bool MainWindow::saveImage(const std::string& path) {
    if(!mImage) return false;
    if(mImage->animated()) {
        showMessage("Saving animated images is not supported", nullptr, true);
        return false;
    }
    std::string ext = lowerExtension(path);
    if(!canSaveExtension(ext)) {
        showMessage("Can't save ." + ext + " files. Use Save as with jpg, png, webp, jxl or avif.", nullptr, true);
        return false;
    }
    std::string err;
    std::string source = mCurrentPath;
    // Write to a temporary file first, then copy the metadata and replace the target.
    std::string tmp = joinPath(dirName(path), "." + baseName(path) + ".qimgv-tmp." + ext);
    if(!saveSurface(mImage->surface(0), tmp, settings().jpegQuality, err)) {
        g_unlink(tmp.c_str());
        showMessage("Save failed: " + err, nullptr, true);
        return false;
    }
    if(!source.empty() && (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "webp" || ext == "jxl" ||
                           ext == "avif" || ext == "jpe" || ext == "jfif"))
        copyMetadata(source, tmp);
    if(g_rename(tmp.c_str(), path.c_str()) != 0) {
        g_unlink(tmp.c_str());
        showMessage("Save failed: could not replace " + path, nullptr, true);
        return false;
    }
    mLoader.invalidate(path);
    mThumbs.forget(path);
    if(path == mCurrentPath) {
        mEdited = false;
        mSaveConfirm->setVisible(false);
    }
    mPanel->strip().refreshItem(path);
    mFolder->grid().refreshItem(path);
    showMessage("Saved " + baseName(path), "notifications/success16");
    updateInfo();
    return true;
}

void MainWindow::saveAs() {
    if(!mImage) return;
    std::string target = chooseSaveFile(GTK_WINDOW(mWindow), mCurrentPath);
    if(target.empty()) return;
    if(lowerExtension(target).empty()) target += ".png";
    bool wasEdited = mEdited;
    if(saveImage(target) && target != mCurrentPath) {
        mEdited = wasEdited;
        mSaveConfirm->setVisible(false);
        mEdited = false;
        // Reload the listing so the new file shows up immediately if it's in this folder.
        if(dirName(target) == mDir.directory()) {
            mDir.reload();
            int idx = mDir.indexOf(target);
            refreshLists();
            if(idx >= 0) loadIndex(idx);
        }
    }
}

void MainWindow::discardEdits() {
    if(!mEdited) return;
    mEdited = false;
    mSaveConfirm->setVisible(false);
    if(ImagePtr orig = mLoader.cached(mCurrentPath)) {
        mImage = orig;
        mViewer->setImage(orig);
        updateInfo();
    } else if(mIndex >= 0) {
        loadIndex(mIndex);
    }
}

// ---- file management --------------------------------------------------------

void MainWindow::afterFileRemoved(const std::string& path) {
    int old = mIndex;
    mDir.removePath(path);
    mLoader.invalidate(path);
    mThumbs.forget(path);
    mShuffleOrder.clear();
    if(path == mCurrentPath) {
        mCurrentPath.clear();
        mImage.reset();
        mEdited = false;
        mSaveConfirm->setVisible(false);
        mIndex = -1;
        refreshLists();
        int n = std::min(old, mDir.count() - 1);
        if(n >= 0) {
            if(mMode == ViewMode::Document)
                loadIndex(n);
            else
                folderSelect((int)mDir.subdirs().size() + n), mFolder->grid().setCurrent((int)mDir.subdirs().size() + n);
        } else {
            mViewer->clear();
        }
    } else {
        mIndex = mDir.indexOf(mCurrentPath);
        refreshLists();
    }
    updateInfo();
}

void MainWindow::removeCurrent(bool trash) {
    const FileEntry* e = currentEntry();
    if(!e) return;
    std::string path = e->path;
    std::string name = e->name;
    if(trash ? settings().confirmTrash : settings().confirmDelete) {
        std::string msg = trash ? "Move \"" + name + "\" to trash?" : "Delete \"" + name + "\" permanently?";
        if(!askConfirm(GTK_WINDOW(mWindow), trash ? "Move to trash" : "Delete file", msg, trash ? "Move to trash" : "Delete"))
            return;
    }
    std::string err;
    bool ok = trash ? moveToTrash(path, err) : deleteFile(path, err);
    if(!ok) return showMessage((trash ? "Could not move to trash: " : "Could not delete: ") + err, nullptr, true);
    afterFileRemoved(path);
    showMessage(trash ? "Moved to trash: " + name : "Deleted: " + name, "menuitem/trash16");
}

void MainWindow::quickCopyMove(int slot) {
    const FileEntry* e = currentEntry();
    if(!e || slot < 0 || slot >= (int)settings().savedPaths.size()) return;
    std::string dest = settings().savedPaths[slot];
    if(!isDirectory(dest)) return showMessage("Folder does not exist: " + dest, nullptr, true);
    bool move = mCopy->moveMode();
    std::string path = e->path, name = e->name, err;
    auto run = [&](bool overwrite) {
        return move ? moveToDir(path, dest, overwrite, err) : copyToDir(path, dest, overwrite, err);
    };
    FileOpResult r = run(false);
    if(r == FileOpResult::Exists) {
        ReplaceChoice c = askReplace(GTK_WINDOW(mWindow), name);
        if(c != ReplaceChoice::Replace) return;
        r = run(true);
    }
    if(r != FileOpResult::Ok) return showMessage((move ? "Move failed: " : "Copy failed: ") + err, nullptr, true);
    std::string shortDest = baseName(dest);
    if(move) {
        afterFileRemoved(path);
        showMessage("Moved to " + shortDest, "overlay/move16");
    } else {
        showMessage("Copied to " + shortDest, "overlay/copy16");
    }
}

void MainWindow::chooseSlotPath(int slot) {
    auto& paths = settings().savedPaths;
    if(slot < 0 || slot >= (int)paths.size()) return;
    std::string dir = chooseFolder(GTK_WINDOW(mWindow), "Choose destination " + std::to_string(slot + 1), paths[slot]);
    if(dir.empty()) return;
    paths[slot] = dir;
    mCopy->refresh();
    settings().save();
}

void MainWindow::renameCurrent(const std::string& newName) {
    const FileEntry* e = currentEntry();
    if(!e || newName.empty() || newName == e->name) return;
    std::string path = e->path, newPath, err;
    FileOpResult r = renameFile(path, newName, false, newPath, err);
    if(r == FileOpResult::Exists) {
        if(!askConfirm(GTK_WINDOW(mWindow), "File already exists", "\"" + newName + "\" already exists. Replace it?",
                       "Replace"))
            return;
        r = renameFile(path, newName, true, newPath, err);
    }
    if(r != FileOpResult::Ok) return showMessage("Rename failed: " + err, nullptr, true);
    ImagePtr img = mLoader.cached(path);
    mLoader.invalidate(path);
    mThumbs.forget(path);
    if(!isSupportedImageFile(newPath)) {
        afterFileRemoved(path);
        return;
    }
    mDir.renamePath(path, newPath);
    mCurrentPath = newPath;
    mIndex = mDir.indexOf(newPath);
    if(mImage) mImage->path = newPath;
    refreshLists();
    updateInfo();
}

void MainWindow::openFileDialog() {
    std::string start = mDir.directory().empty() ? g_get_home_dir() : mDir.directory();
    std::string f = chooseOpenFile(GTK_WINDOW(mWindow), start);
    if(!f.empty()) open(f);
}

// ---- clipboard ----------------------------------------------------------------

void MainWindow::copyFileToClipboard() {
    const FileEntry* e = currentEntry();
    if(!e) return;
    gchar* uri = g_filename_to_uri(e->path.c_str(), nullptr, nullptr);
    if(!uri) return;
    static GtkTargetEntry targets[] = {
        {(gchar*)"x-special/gnome-copied-files", 0, 0},
        {(gchar*)"text/uri-list", 0, 1},
        {(gchar*)"UTF8_STRING", 0, 2},
        {(gchar*)"text/plain;charset=utf-8", 0, 2},
    };
    struct Data {
        std::string uri, path;
    };
    auto* d = new Data{uri, e->path};
    g_free(uri);
    GtkClipboard* cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_with_data(
        cb, targets, G_N_ELEMENTS(targets),
        [](GtkClipboard*, GtkSelectionData* sel, guint info, gpointer p) {
            auto* d = static_cast<Data*>(p);
            std::string s;
            if(info == 0)
                s = "copy\n" + d->uri;
            else if(info == 1)
                s = d->uri + "\r\n";
            else
                s = d->path;
            gtk_selection_data_set(sel, gtk_selection_data_get_target(sel), 8,
                                   reinterpret_cast<const guchar*>(s.data()), (gint)s.size());
        },
        [](GtkClipboard*, gpointer p) { delete static_cast<Data*>(p); }, d);
    showMessage("File copied to clipboard", "overlay/copy16");
}

void MainWindow::copyPathToClipboard() {
    const FileEntry* e = currentEntry();
    if(!e) return;
    gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), e->path.c_str(), -1);
    showMessage("Path copied to clipboard", "overlay/copy16");
}

void MainWindow::pasteFiles() {
    if(mDir.directory().empty()) return;
    gtk_clipboard_request_uris(
        gtk_clipboard_get(GDK_SELECTION_CLIPBOARD),
        [](GtkClipboard*, gchar** uris, gpointer self) {
            auto* mw = static_cast<MainWindow*>(self);
            if(!uris) return mw->showMessage("Nothing to paste");
            int copied = 0;
            for(int i = 0; uris[i]; i++) {
                gchar* path = g_filename_from_uri(uris[i], nullptr, nullptr);
                if(!path) continue;
                std::string err;
                FileOpResult r = copyToDir(path, mw->mDir.directory(), false, err);
                if(r == FileOpResult::Exists &&
                   askReplace(GTK_WINDOW(mw->mWindow), baseName(path)) == ReplaceChoice::Replace)
                    r = copyToDir(path, mw->mDir.directory(), true, err);
                if(r == FileOpResult::Ok) copied++;
                g_free(path);
            }
            if(copied) mw->showMessage("Pasted " + std::to_string(copied) + " file(s)", "notifications/success16");
        },
        this);
}

// ---- print --------------------------------------------------------------------

void MainWindow::print() {
    if(!mImage) return;
    GtkPrintOperation* op = gtk_print_operation_new();
    gtk_print_operation_set_n_pages(op, 1);
    gtk_print_operation_set_job_name(op, baseName(mCurrentPath).c_str());
    gtk_print_operation_set_embed_page_setup(op, TRUE);
    GtkPageSetup* setup = gtk_page_setup_new();
    gtk_page_setup_set_orientation(setup, mImage->width > mImage->height ? GTK_PAGE_ORIENTATION_LANDSCAPE
                                                                         : GTK_PAGE_ORIENTATION_PORTRAIT);
    gtk_print_operation_set_default_page_setup(op, setup);
    g_object_unref(setup);
    g_signal_connect(op, "draw-page", gcb(+[](GtkPrintOperation*, GtkPrintContext* ctx, gint, gpointer self) {
                         auto* mw = static_cast<MainWindow*>(self);
                         if(!mw->mImage) return;
                         cairo_t* cr = gtk_print_context_get_cairo_context(ctx);
                         double pw = gtk_print_context_get_width(ctx), ph = gtk_print_context_get_height(ctx);
                         double k = std::min(pw / mw->mImage->width, ph / mw->mImage->height);
                         cairo_translate(cr, (pw - mw->mImage->width * k) / 2, (ph - mw->mImage->height * k) / 2);
                         cairo_scale(cr, k, k);
                         cairo_set_source_surface(cr, mw->mImage->surface(0), 0, 0);
                         cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
                         cairo_paint(cr);
                     }),
                     this);
    GError* err = nullptr;
    gtk_print_operation_run(op, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG, GTK_WINDOW(mWindow), &err);
    if(err) {
        showMessage(std::string("Print failed: ") + err->message, nullptr, true);
        g_error_free(err);
    }
    g_object_unref(op);
}

// ---- toggles ------------------------------------------------------------------

void MainWindow::toggleFullscreen() {
    if(mFullscreen)
        gtk_window_unfullscreen(GTK_WINDOW(mWindow));
    else
        gtk_window_fullscreen(GTK_WINDOW(mWindow));
}

void MainWindow::toggleSlideshow() {
    if(mSlideshow) {
        g_source_remove(mSlideshow);
        mSlideshow = 0;
        updateInfo();
        return;
    }
    mSlideshow = g_timeout_add(
        (guint)settings().slideshowInterval,
        [](gpointer p) -> gboolean {
            auto* mw = static_cast<MainWindow*>(p);
            mw->mSlideshow = 0;
            int before = mw->mIndex;
            mw->navigate(1);
            // navigate() restarts the timer through loadIndex(); restart it here if it didn't move.
            if(mw->mIndex == before && !mw->mSlideshow) mw->toggleSlideshow();
            return G_SOURCE_REMOVE;
        },
        this);
    updateInfo();
}

void MainWindow::toggleShuffle() {
    mShuffle = !mShuffle;
    mShuffleOrder.clear();
    showMessage(mShuffle ? "Shuffle mode on" : "Shuffle mode off");
    preloadNeighbours();
    updateInfo();
}

void MainWindow::updateImageInfo() {
    const FileEntry* e = currentEntry();
    InfoList file, exif;
    if(e) {
        file.emplace_back("File", e->name);
        file.emplace_back("Folder", dirName(e->path));
        file.emplace_back("Size", humanSize(e->size));
        GDateTime* dt = g_date_time_new_from_unix_local(e->mtime / 1000000);
        if(dt) {
            gchar* s = g_date_time_format(dt, "%Y-%m-%d %H:%M:%S");
            file.emplace_back("Modified", s);
            g_free(s);
            g_date_time_unref(dt);
        }
        if(mImage) {
            file.emplace_back("Format", mImage->format);
            file.emplace_back("Resolution", std::to_string(mImage->width) + " x " + std::to_string(mImage->height));
            file.emplace_back("Bit depth", std::to_string(mImage->bitDepth));
            if(mImage->animated()) file.emplace_back("Frames", std::to_string(mImage->frames.size()));
            file.emplace_back("Alpha", mImage->hasAlpha ? "yes" : "no");
        }
        exif = readExifSummary(e->path);
    }
    mInfo->setInfo(file, exif);
}

void MainWindow::toggleImageInfo() {
    if(!mInfo->visible()) updateImageInfo();
    mInfo->toggle();
}

void MainWindow::setSort(SortMode m) {
    settings().sortMode = m;
    mDir.setSortMode(m);
    mFolder->setSortMode(m);
    mIndex = mDir.indexOf(mCurrentPath);
    mShuffleOrder.clear();
    refreshLists();
    preloadNeighbours();
    updateInfo();
    static const char* names[] = {"Sort: name", "Sort: name (desc)", "Sort: size", "Sort: size (desc)",
                                  "Sort: oldest first", "Sort: newest first"};
    if(mMode == ViewMode::Document) showMessage(names[(int)m], "other/sorting-mode16");
}

void MainWindow::showContextMenu(double x, double y) {
    mPanel->reveal(false);
    mMenu->popup(x, y, gtk_widget_get_allocated_width(mOverlay), gtk_widget_get_allocated_height(mOverlay));
}

void MainWindow::runUserScript(const std::string& name) {
    auto it = settings().scripts.find(name);
    const FileEntry* e = currentEntry();
    if(it == settings().scripts.end() || !e) return;
    std::string err;
    if(!runScript(it->second, e->path, err)) showMessage("Script failed: " + err, nullptr, true);
}

void MainWindow::closeFullscreenOrExit() {
    if(mMenu->visible()) return mMenu->hide();
    if(mCopy->visible()) return mCopy->close();
    if(mInfo->visible()) return mInfo->close();
    if(mViewer->cropping()) return cancelCrop();
    if(mFullscreen) return toggleFullscreen();
    quit();
}

void MainWindow::quit() {
    if(mSlideshow) g_source_remove(mSlideshow);
    mSlideshow = 0;
    saveWindowState();
    gtk_widget_hide(mWindow);
    gtk_main_quit();
}

}  // namespace qimgv
