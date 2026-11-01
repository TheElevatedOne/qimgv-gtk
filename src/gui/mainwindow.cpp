#include "gui/mainwindow.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "actions.hpp"
#include "core/fileops.hpp"
#include "gui/icons.hpp"
#include "gui/style.hpp"
#include "image/formats.hpp"

namespace qimgv {

MainWindow::MainWindow() {
    mDir.setSortMode(settings().sortMode);
    mDir.onChanged = [this] { onDirectoryChanged(); };
    mDir.onFileModified = [this](const std::string& path) {
        mLoader.invalidate(path);
        mThumbs.forget(path);
        if(path == mCurrentPath && !mEdited) loadIndex(mIndex);
        mPanel->strip().refreshItem(path);
        mFolder->grid().refreshItem(path);
    };
    buildUi();
}

MainWindow::~MainWindow() = default;

// ---- UI ---------------------------------------------------------------------

void MainWindow::buildUi() {
    mWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(mWindow), "qimgv");
    gtk_window_set_decorated(GTK_WINDOW(mWindow), FALSE);  // no CSD title bar
    gtk_window_set_default_size(GTK_WINDOW(mWindow), settings().windowWidth, settings().windowHeight);
    if(settings().maximized) gtk_window_maximize(GTK_WINDOW(mWindow));
    addClass(mWindow, "qimgv");
    if(GdkPixbuf* icon = appIcon(128)) {
        gtk_window_set_icon(GTK_WINDOW(mWindow), icon);
        g_object_unref(icon);
    }

    mViewer = std::make_unique<Viewer>();
    mPanel = std::make_unique<MainPanel>(mThumbs);
    mFolder = std::make_unique<FolderView>(mThumbs);
    mMenu = std::make_unique<ContextMenu>();
    mCrop = std::make_unique<CropPanel>();
    mMessage = std::make_unique<FloatingMessage>();
    mCopy = std::make_unique<CopyOverlay>();
    mRename = std::make_unique<RenameOverlay>();
    mSaveConfirm = std::make_unique<SaveConfirmOverlay>();
    mInfo = std::make_unique<ImageInfoOverlay>();
    mFsInfo = std::make_unique<FullscreenInfo>();
    mControls = std::make_unique<ControlsOverlay>();

    mOverlay = gtk_overlay_new();
    gtk_container_add(GTK_CONTAINER(mOverlay), mViewer->widget());
    for(GtkWidget* w : {mFsInfo->widget(), mInfo->widget(), mCopy->widget(), mSaveConfirm->widget(),
                        mMessage->widget(), mRename->widget(), mControls->widget(), mPanel->widget(),
                        mMenu->widget()})
        gtk_overlay_add_overlay(GTK_OVERLAY(mOverlay), w);
    gtk_widget_set_margin_start(mFsInfo->widget(), 0);

    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(hbox), mOverlay, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), mCrop->widget(), FALSE, FALSE, 0);
    gtk_widget_set_hexpand(mCrop->widget(), FALSE);  // stop hexpand propagating up from the spin buttons

    mInfoBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    addClass(mInfoBar, "qimgv-infobar");
    mInfoIndex = gtk_label_new("");
    mInfoName = gtk_label_new("No file opened.");
    mInfoDetails = gtk_label_new("");
    addClass(mInfoIndex, "dim-label");
    addClass(mInfoDetails, "dim-label");
    gtk_label_set_xalign(GTK_LABEL(mInfoName), 0);
    gtk_label_set_ellipsize(GTK_LABEL(mInfoName), PANGO_ELLIPSIZE_MIDDLE);
    gtk_box_pack_start(GTK_BOX(mInfoBar), mInfoIndex, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mInfoBar), mInfoName, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(mInfoBar), mInfoDetails, FALSE, FALSE, 0);
    gtk_widget_set_no_show_all(mInfoBar, TRUE);
    gtk_widget_show(mInfoIndex);
    gtk_widget_show(mInfoName);
    gtk_widget_show(mInfoDetails);

    mDocBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    mTopSlot = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    mBottomSlot = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(mDocBox), mTopSlot, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mDocBox), hbox, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(mDocBox), mBottomSlot, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mDocBox), mInfoBar, FALSE, FALSE, 0);

    mStack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(mStack), GTK_STACK_TRANSITION_TYPE_NONE);
    gtk_stack_add_named(GTK_STACK(mStack), mDocBox, "document");
    gtk_stack_add_named(GTK_STACK(mStack), mFolder->widget(), "folder");
    gtk_container_add(GTK_CONTAINER(mWindow), mStack);

    // ---- wiring ----
    auto act = [this](const std::string& a) { action(a); };
    mPanel->onAction = act;
    mPanel->onPinToggled = [this](bool pinned) {
        settings().panelPinned = pinned;
        placePanel();
    };
    mPanel->strip().onActivate = [this](int i) { loadIndex(i); };
    mFolder->onAction = act;
    mFolder->onDirectory = [this](const std::string& dir) {
        if(setDirectory(dir)) {
            mIndex = -1;
            refreshLists();
            updateInfo();
        }
    };
    mFolder->onActivate = [this](int i) { folderActivate(i); };
    mFolder->onSelect = [this](int i) { folderSelect(i); };
    mFolder->onSortChanged = [this](SortMode m) { setSort(m); };
    mControls->onAction = act;
    mMenu->onAction = act;
    mSaveConfirm->onAction = act;
    mCopy->onPick = [this](int slot) { quickCopyMove(slot); };
    mCopy->onChangePath = [this](int slot) { chooseSlotPath(slot); };
    mRename->onRename = [this](const std::string& n) { renameCurrent(n); };
    mRename->onClosed = [this] { gtk_widget_grab_focus(mViewer->widget()); };
    mCrop->onRectChanged = [this](const CropRect& r) { mViewer->setCropRect(r); };
    mCrop->onAspectChanged = [this](double r) { mViewer->setCropAspect(r); };
    mCrop->onCrop = [this](bool save) { finishCrop(save); };
    mCrop->onCancel = [this] { cancelCrop(); };
    mViewer->onCropChanged = [this](const CropRect& r) { mCrop->setRect(r); };
    mViewer->onInput = [this](const std::string& seq, double x, double y) { return handleSequence(seq, x, y); };
    mViewer->onScaleChanged = [this] { updateInfo(); };
    mViewer->onEdgeClick = [this](int dir) { navigate(dir); };
    mViewer->onPressHook = [this] {
        if(mMenu->visible()) {
            mMenu->hide();
            return true;
        }
        return false;
    };
    mViewer->onMotion = [this](double x, double y) {
        mPointerX = x;
        mPointerY = y;
        updatePanelVisibility(x, y);
    };

    gtk_widget_add_events(mWindow, GDK_KEY_PRESS_MASK);
    g_signal_connect(mWindow, "key-press-event", G_CALLBACK(keyCb), this);
    g_signal_connect(mWindow, "window-state-event", G_CALLBACK(stateCb), this);
    g_signal_connect(mWindow, "delete-event", gcb(+[](GtkWidget*, GdkEvent*, gpointer self) -> gboolean {
                         static_cast<MainWindow*>(self)->quit();
                         return TRUE;
                     }),
                     this);
    g_signal_connect(mWindow, "size-allocate", gcb(+[](GtkWidget* w, GdkRectangle*, gpointer self) {
                         auto* mw = static_cast<MainWindow*>(self);
                         if(mw->mFullscreen || settings().maximized) return;
                         int ww, hh;
                         gtk_window_get_size(GTK_WINDOW(w), &ww, &hh);
                         settings().windowWidth = ww;
                         settings().windowHeight = hh;
                     }),
                     this);

    // Drag and drop files onto the window.
    static GtkTargetEntry targets[] = {{(gchar*)"text/uri-list", 0, 0}};
    gtk_drag_dest_set(mWindow, GTK_DEST_DEFAULT_ALL, targets, 1, GDK_ACTION_COPY);
    g_signal_connect(mWindow, "drag-data-received", G_CALLBACK(dropCb), this);

    gtk_widget_show_all(mStack);
    gtk_stack_set_visible_child_name(GTK_STACK(mStack), "document");
    mPanel->strip().setThumbSize(settings().panelThumbSize);
    mPanel->strip().setSquare(settings().squareThumbnails);
    mFolder->grid().setSquare(false);
    mPanel->setPinned(settings().panelPinned);
    placePanel();
    updateChrome();
    std::map<std::string, std::string> scripts;
    mMenu->update(settings().shortcuts, scripts);
    applySettings();
}

void MainWindow::show() {
    gtk_widget_show(mWindow);
    if(settings().fullscreenOnStart) toggleFullscreen();
    gtk_widget_grab_focus(mViewer->widget());
}

void MainWindow::scheduleSnapshot(const std::string& png, const std::string& actions) {
    struct Job {
        MainWindow* mw;
        std::string png;
        std::vector<std::string> actions;
        size_t next = 0;
    };
    auto* job = new Job{this, png, {}};
    gchar** parts = g_strsplit(actions.c_str(), ",", -1);
    for(int i = 0; parts[i]; i++)
        if(*parts[i]) job->actions.emplace_back(parts[i]);
    g_strfreev(parts);
    g_timeout_add(1500, [](gpointer p) -> gboolean {
        auto* job = static_cast<Job*>(p);
        if(job->next < job->actions.size()) {
            const std::string& a = job->actions[job->next++];
            if(a.rfind("move:", 0) == 0) {
                double x = 0, y = 0;
                sscanf(a.c_str() + 5, "%lf:%lf", &x, &y);
                job->mw->mPointerX = x;
                job->mw->mPointerY = y;
                job->mw->updatePanelVisibility(x, y);
            } else if(a.rfind("key:", 0) == 0) {
                // Synthesize a key press (e.g. key:Right, key:ctrl+r) through the normal event path.
                std::string k = a.substr(4);
                guint state = 0;
                for(auto [prefix, mask] : {std::pair<const char*, guint>{"ctrl+", GDK_CONTROL_MASK},
                                           {"shift+", GDK_SHIFT_MASK}, {"alt+", GDK_MOD1_MASK}}) {
                    if(k.rfind(prefix, 0) == 0) {
                        state |= mask;
                        k = k.substr(strlen(prefix));
                    }
                }
                GdkEvent* ev = gdk_event_new(GDK_KEY_PRESS);
                ev->key.window = GDK_WINDOW(g_object_ref(gtk_widget_get_window(job->mw->mWindow)));
                ev->key.keyval = gdk_keyval_from_name(k.c_str());
                ev->key.state = state;
                ev->key.time = GDK_CURRENT_TIME;
                GdkKeymapKey* keys = nullptr;
                gint n = 0;
                if(gdk_keymap_get_entries_for_keyval(gdk_keymap_get_for_display(gdk_display_get_default()),
                                                     ev->key.keyval, &keys, &n) && n > 0)
                    ev->key.hardware_keycode = (guint16)keys[0].keycode;
                g_free(keys);
                gdk_event_set_device(ev, gdk_seat_get_keyboard(gdk_display_get_default_seat(gdk_display_get_default())));
                gtk_main_do_event(ev);
                gdk_event_free(ev);
            } else if(a.rfind("menu:", 0) == 0) {
                double x = 0, y = 0;
                sscanf(a.c_str() + 5, "%lf:%lf", &x, &y);
                job->mw->action("contextMenu", x, y);
            } else {
                job->mw->action(a);
            }
            return G_SOURCE_CONTINUE;
        }
        // Main window first, then any other visible toplevel (dialogs) as <name>-1.png, ...
        GList* tops = gtk_window_list_toplevels();
        int n = 0;
        std::vector<GtkWidget*> wins = {job->mw->mWindow};
        for(GList* l = tops; l; l = l->next)
            if(l->data != job->mw->mWindow && gtk_widget_get_visible(GTK_WIDGET(l->data)) &&
               gtk_window_get_window_type(GTK_WINDOW(l->data)) == GTK_WINDOW_TOPLEVEL)
                wins.push_back(GTK_WIDGET(l->data));
        g_list_free(tops);
        for(GtkWidget* w : wins) {
            int W = gtk_widget_get_allocated_width(w), H = gtk_widget_get_allocated_height(w);
            cairo_surface_t* s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
            cairo_t* cr = cairo_create(s);
            gtk_widget_draw(w, cr);
            cairo_destroy(cr);
            std::string out = job->png;
            if(n > 0) out = out.substr(0, out.size() - 4) + "-" + std::to_string(n) + ".png";
            cairo_surface_write_to_png(s, out.c_str());
            cairo_surface_destroy(s);
            n++;
        }
        delete job;
        gtk_main_quit();
        return G_SOURCE_REMOVE;
    }, job);
}

void MainWindow::placePanel() {
    GtkWidget* pw = mPanel->widget();
    bool wantLayout = settings().panelPinned && settings().panelEnabled &&
                      (!settings().panelFullscreenOnly || mFullscreen);
    GtkWidget* parent = gtk_widget_get_parent(pw);
    g_object_ref(pw);
    if(parent) gtk_container_remove(GTK_CONTAINER(parent), pw);
    mPanel->setPosition(settings().panelPosition);
    if(wantLayout) {
        GtkWidget* slot = settings().panelPosition == PanelPosition::Bottom ? mBottomSlot : mTopSlot;
        gtk_box_pack_start(GTK_BOX(slot), pw, FALSE, FALSE, 0);
        gtk_revealer_set_transition_duration(GTK_REVEALER(pw), 0);
        mPanel->reveal(true);
    } else {
        gtk_overlay_add_overlay(GTK_OVERLAY(mOverlay), pw);
        // Keep the context menu above the panel.
        g_object_ref(mMenu->widget());
        gtk_container_remove(GTK_CONTAINER(mOverlay), mMenu->widget());
        gtk_overlay_add_overlay(GTK_OVERLAY(mOverlay), mMenu->widget());
        g_object_unref(mMenu->widget());
        gtk_revealer_set_transition_duration(GTK_REVEALER(pw), 120);
        mPanel->reveal(false);
    }
    g_object_unref(pw);
    gtk_widget_show(pw);
    mPanelInLayout = wantLayout;
    if(mIndex >= 0) mPanel->strip().scrollToCurrent();
}

void MainWindow::updatePanelVisibility(double x, double y) {
    // Fullscreen controls in the top right corner.
    double W = gtk_widget_get_allocated_width(mOverlay), H = gtk_widget_get_allocated_height(mOverlay);
    bool controls = mFullscreen && x > W - 220 && y < 70 && !mPanel->revealed();
    gtk_widget_set_visible(mControls->widget(), controls);

    if(mPanelInLayout || !settings().panelEnabled || (settings().panelFullscreenOnly && !mFullscreen) ||
       mViewer->cropping() || mDir.count() == 0)
        return;
    bool bottom = settings().panelPosition == PanelPosition::Bottom;
    int ph = mPanel->height();
    if(!mPanel->revealed()) {
        if((!bottom && y >= 0 && y < 6) || (bottom && y > H - 6)) {
            mPanel->reveal(true);
            mPanel->strip().scrollToCurrent();
        }
    } else if((!bottom && y > ph + 30) || (bottom && y < H - ph - 30)) {
        mPanel->reveal(false);
    }
}

void MainWindow::updateChrome() {
    bool doc = mMode == ViewMode::Document;
    bool infoBar = doc && (mFullscreen ? settings().infoBarFullscreen : settings().infoBarWindowed);
    gtk_widget_set_visible(mInfoBar, infoBar);
    bool fsInfo = doc && mFullscreen && !settings().infoBarFullscreen && mIndex >= 0;
    gtk_widget_set_visible(mFsInfo->widget(), fsInfo);
    if(!mFullscreen) gtk_widget_hide(mControls->widget());
    gtk_widget_set_margin_top(mFsInfo->widget(), 0);
}

void MainWindow::showMessage(const std::string& text, const char* icon, bool error) {
    mMessage->show(text, icon, error ? 3500 : 1600, error);
}

void MainWindow::applySettings() {
    Settings& s = settings();
    mDir.setSortMode(s.sortMode);
    mFolder->setSortMode(s.sortMode);
    mFolder->setPlacesVisible(s.placesPanel);
    mPanel->strip().setThumbSize(s.panelThumbSize);
    mPanel->strip().setSquare(s.squareThumbnails);
    mThumbs.setThreads(s.thumbnailerThreads);
    mViewer->settingsChanged();
    std::map<std::string, std::string> scripts;
    for(auto& [name, sc] : s.scripts) {
        std::string key;
        for(auto& [seq, a] : s.shortcuts)
            if(a == "s:" + name) key = seq;
        scripts[name] = key;
    }
    mMenu->update(s.shortcuts, scripts);
    placePanel();
    refreshLists();
    updateChrome();
    updateInfo();
}

void MainWindow::saveWindowState() { settings().save(); }

// ---- info -------------------------------------------------------------------

void MainWindow::updateInfo() {
    const FileEntry* e = currentEntry();
    if(!e) {
        gtk_label_set_text(GTK_LABEL(mInfoIndex), "");
        gtk_label_set_text(GTK_LABEL(mInfoName), mDir.directory().empty() ? "No file opened." : mDir.directory().c_str());
        gtk_label_set_text(GTK_LABEL(mInfoDetails), "");
        mFsInfo->setText("", "");
        updateTitle();
        return;
    }
    char idx[64];
    snprintf(idx, sizeof(idx), "%d / %d", mIndex + 1, mDir.count());
    std::string details;
    if(mImage) {
        details = std::to_string(mImage->width) + " x " + std::to_string(mImage->height);
        if(!mImage->format.empty()) details += "  " + mImage->format;
        if(mImage->animated()) details += " (" + std::to_string(mImage->frames.size()) + " frames)";
    }
    details += (details.empty() ? "" : "  ") + humanSize(e->size);
    if(mImage) details += "  " + std::to_string((int)std::lround(mViewer->scale() * 100)) + "%";
    if(mEdited) details += "  [edited]";
    if(mSlideshow) details += "  [slideshow]";
    if(mShuffle) details += "  [shuffle]";
    gtk_label_set_text(GTK_LABEL(mInfoIndex), idx);
    gtk_label_set_text(GTK_LABEL(mInfoName), e->name.c_str());
    gtk_label_set_text(GTK_LABEL(mInfoDetails), details.c_str());
    mFsInfo->setText(std::string("[") + idx + "]  " + e->name, details);
    updateTitle();
    updateChrome();
}

void MainWindow::updateTitle() {
    const FileEntry* e = currentEntry();
    std::string title;
    if(mMode == ViewMode::Folder) {
        title = mDir.directory().empty() ? "qimgv" : mDir.directory() + " — qimgv";
    } else if(!e) {
        title = "qimgv";
    } else {
        title = e->name;
        if(settings().windowTitleExtendedInfo && mImage)
            title += " — " + std::to_string(mImage->width) + " x " + std::to_string(mImage->height) + " — " +
                     humanSize(e->size);
        if(mEdited) title = "* " + title;
        title += " — qimgv";
    }
    gtk_window_set_title(GTK_WINDOW(mWindow), title.c_str());
}

// ---- input --------------------------------------------------------------------

gboolean MainWindow::keyCb(GtkWidget*, GdkEventKey* ev, gpointer self) {
    return static_cast<MainWindow*>(self)->handleKey(ev);
}

bool MainWindow::handleKey(GdkEventKey* ev) {
    GtkWidget* focus = gtk_window_get_focus(GTK_WINDOW(mWindow));
    // Text fields (rename, crop numbers, ...) get keys first.
    if(focus && GTK_IS_EDITABLE(focus)) {
        if(gtk_window_propagate_key_event(GTK_WINDOW(mWindow), ev)) return true;
        if(ev->keyval != GDK_KEY_Escape) return false;
    }
    if(isModifierKey(ev)) return false;
    guint mods = ev->state & gtk_accelerator_get_default_mod_mask();

    if(mMenu->visible() && ev->keyval == GDK_KEY_Escape) {
        mMenu->hide();
        return true;
    }
    if(mViewer->cropping()) {
        if(ev->keyval == GDK_KEY_Escape) {
            cancelCrop();
            return true;
        }
        if(ev->keyval == GDK_KEY_Return || ev->keyval == GDK_KEY_KP_Enter) {
            finishCrop(false);
            return true;
        }
    }
    if(mCopy->visible()) {
        int digit = -1;
        if(ev->keyval >= GDK_KEY_1 && ev->keyval <= GDK_KEY_9) digit = ev->keyval - GDK_KEY_1;
        if(ev->keyval >= GDK_KEY_KP_1 && ev->keyval <= GDK_KEY_KP_9) digit = ev->keyval - GDK_KEY_KP_1;
        if(digit < 0 && !mods) {
            // Layouts where the digit row needs Shift (e.g. Czech): use the hardware key.
            std::string s = keySequence(ev, true);
            if(s.size() == 1 && s[0] >= '1' && s[0] <= '9') digit = s[0] - '1';
        }
        if(digit >= 0) {
            quickCopyMove(digit);
            return true;
        }
        if(ev->keyval == GDK_KEY_Escape) {
            mCopy->close();
            return true;
        }
    }
    if(mMode == ViewMode::Folder) {
        // Let the grid handle navigation keys.
        if(gtk_window_propagate_key_event(GTK_WINDOW(mWindow), ev)) return true;
        if(ev->keyval == GDK_KEY_BackSpace && !mods) {
            action("goUp");
            return true;
        }
    }
    std::string seq = keySequence(ev);
    auto& sc = settings().shortcuts;
    auto it = sc.find(seq);
    if(it == sc.end()) {
        std::string alt = keySequence(ev, true);
        it = sc.find(alt);
    }
    if(it != sc.end()) {
        action(it->second);
        return true;
    }
    return false;
}

bool MainWindow::handleSequence(const std::string& seq, double x, double y) {
    auto& sc = settings().shortcuts;
    auto it = sc.find(seq);
    if(it == sc.end()) return false;
    action(it->second, x, y);
    return true;
}

gboolean MainWindow::stateCb(GtkWidget*, GdkEventWindowState* ev, gpointer self) {
    auto* mw = static_cast<MainWindow*>(self);
    if(ev->changed_mask & GDK_WINDOW_STATE_MAXIMIZED)
        settings().maximized = (ev->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) != 0;
    if(ev->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) {
        mw->mFullscreen = (ev->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) != 0;
        mw->placePanel();
        mw->updateChrome();
    }
    return FALSE;
}

void MainWindow::dropCb(GtkWidget*, GdkDragContext* ctx, gint, gint, GtkSelectionData* data, guint, guint time,
                        gpointer self) {
    auto* mw = static_cast<MainWindow*>(self);
    gchar** uris = gtk_selection_data_get_uris(data);
    if(uris && uris[0]) {
        gchar* path = g_filename_from_uri(uris[0], nullptr, nullptr);
        if(path) mw->open(path);
        g_free(path);
    }
    g_strfreev(uris);
    gtk_drag_finish(ctx, TRUE, FALSE, time);
}

// ---- directory & navigation -------------------------------------------------

bool MainWindow::setDirectory(const std::string& dir) {
    if(!mDir.setDirectory(dir)) {
        showMessage("Could not open folder " + dir, nullptr, true);
        return false;
    }
    mShuffleOrder.clear();
    mThumbs.cancelPending();
    mLoader.clear();
    return true;
}

void MainWindow::refreshLists() {
    std::vector<ThumbGrid::Item> strip, folder;
    for(auto& d : mDir.subdirs()) folder.push_back({d, baseName(d), true});
    for(auto& f : mDir.files()) {
        strip.push_back({f.path, f.name, false});
        folder.push_back({f.path, f.name, false});
    }
    mPanel->strip().setItems(std::move(strip));
    mPanel->strip().setCurrent(mIndex, true);
    int sel = mIndex >= 0 ? (int)mDir.subdirs().size() + mIndex : (folder.empty() ? -1 : 0);
    mFolder->setDirectory(mDir.directory(), std::move(folder), sel);
}

void MainWindow::onDirectoryChanged() {
    int idx = mDir.indexOf(mCurrentPath);
    if(idx < 0 && !mCurrentPath.empty() && mIndex >= 0) {
        // Current file disappeared.
        refreshLists();
        int n = std::min(mIndex, mDir.count() - 1);
        mIndex = -1;
        if(n >= 0)
            loadIndex(n);
        else {
            mCurrentPath.clear();
            mImage.reset();
            mViewer->clear();
            updateInfo();
        }
        return;
    }
    mIndex = idx;
    mShuffleOrder.clear();
    refreshLists();
    updateInfo();
}

void MainWindow::open(const std::string& rawPath) {
    if(rawPath.empty()) {
        if(settings().defaultViewMode == ViewMode::Folder) {
            gchar* cwd = g_get_current_dir();
            if(setDirectory(cwd)) {
                refreshLists();
                setMode(ViewMode::Folder);
            }
            g_free(cwd);
        }
        if(mMode == ViewMode::Document) mViewer->showHint("Open an image with Ctrl+O, or drop one here");
        updateInfo();
        return;
    }
    gchar* canon = g_canonicalize_filename(rawPath.c_str(), nullptr);
    std::string path = canon;
    g_free(canon);
    if(isDirectory(path)) {
        if(!setDirectory(path)) return;
        mIndex = -1;
        mCurrentPath.clear();
        refreshLists();
        if(settings().defaultViewMode == ViewMode::Folder || mDir.count() == 0) {
            setMode(ViewMode::Folder);
        } else {
            setMode(ViewMode::Document);
            loadIndex(0);
        }
        updateInfo();
        return;
    }
    if(!g_file_test(path.c_str(), G_FILE_TEST_EXISTS)) {
        showMessage("File not found: " + path, nullptr, true);
        return;
    }
    std::string dir = dirName(path);
    if(!setDirectory(dir)) return;
    setMode(ViewMode::Document);
    int idx = mDir.indexOf(path);
    if(idx < 0) {
        // Not an image we can show (e.g. a video): show the folder's images instead.
        mIndex = -1;
        mCurrentPath.clear();
        refreshLists();
        std::string ext = lowerExtension(path);
        mViewer->showError(isVideoExtension(ext) ? "Video files are not supported" : "Unsupported file type");
        updateInfo();
        return;
    }
    mIndex = -1;
    refreshLists();
    loadIndex(idx);
}

void MainWindow::loadIndex(int index) {
    const FileEntry* e = mDir.at(index);
    if(!e) return;
    if(mViewer->cropping()) cancelCrop();
    mEdited = false;
    mSaveConfirm->setVisible(false);
    mIndex = index;
    mCurrentPath = e->path;
    mPanel->strip().setCurrent(index, true);
    mFolder->grid().setCurrent((int)mDir.subdirs().size() + index, mMode == ViewMode::Folder);
    std::string path = e->path;
    mViewer->showLoading(true);
    bool cached = mLoader.load(path, [this, path](ImagePtr img, const std::string& err) {
        showLoaded(path, img, err);
    });
    (void)cached;
    updateInfo();
    preloadNeighbours();
    if(mSlideshow) {
        // Restart the interval for the new image.
        toggleSlideshow();
        toggleSlideshow();
    }
}

void MainWindow::showLoaded(const std::string& path, ImagePtr img, const std::string& error) {
    if(path != mCurrentPath) return;
    mImage = img;
    if(img) {
        mViewer->setImage(img);
        mLoadError.clear();
    } else {
        mLoadError = error;
        mViewer->showError(error.empty() ? "Could not load image" : error);
    }
    updateInfo();
    if(mInfo->visible()) updateImageInfo();
}

void MainWindow::preloadNeighbours() {
    std::set<std::string> keep;
    if(!mCurrentPath.empty()) keep.insert(mCurrentPath);
    if(settings().usePreloader && mDir.count() > 1) {
        for(int step : {1, -1}) {
            int n = mIndex + step;
            if(mShuffle && !mShuffleOrder.empty()) {
                auto it = std::find(mShuffleOrder.begin(), mShuffleOrder.end(), mIndex);
                if(it != mShuffleOrder.end()) {
                    long pos = (it - mShuffleOrder.begin() + step + (long)mShuffleOrder.size()) % (long)mShuffleOrder.size();
                    n = mShuffleOrder[pos];
                }
            }
            if(const FileEntry* e = mDir.at(n)) {
                keep.insert(e->path);
                mLoader.preload(e->path);
            }
        }
    }
    mLoader.retain(keep);
}

void MainWindow::navigate(int step) {
    int n = mDir.count();
    if(n == 0) return;
    if(mIndex < 0) {
        loadIndex(step > 0 ? 0 : n - 1);
        return;
    }
    if(mShuffle) {
        if((int)mShuffleOrder.size() != n) {
            mShuffleOrder.resize(n);
            for(int i = 0; i < n; i++) mShuffleOrder[i] = i;
            for(int i = n - 1; i > 0; i--) std::swap(mShuffleOrder[i], mShuffleOrder[g_random_int_range(0, i + 1)]);
        }
        auto it = std::find(mShuffleOrder.begin(), mShuffleOrder.end(), mIndex);
        long pos = it == mShuffleOrder.end() ? 0 : it - mShuffleOrder.begin();
        pos = (pos + step + n) % n;
        loadIndex(mShuffleOrder[pos]);
        return;
    }
    int target = mIndex + step;
    if(target >= 0 && target < n) {
        loadIndex(target);
        return;
    }
    switch(settings().folderEndAction) {
        case FolderEndAction::Loop: loadIndex(target < 0 ? n - 1 : 0); break;
        case FolderEndAction::NextFolder: changeDirectory(step); break;
        case FolderEndAction::Nothing:
            if(mSlideshow)
                loadIndex(0);  // slideshows always loop
            else
                showMessage("", target < 0 ? "notifications/dir_start20" : "notifications/dir_end20");
            break;
    }
}

void MainWindow::jumpTo(int index) {
    if(mDir.count() > 0) loadIndex(std::clamp(index, 0, mDir.count() - 1));
}

void MainWindow::changeDirectory(int step) {
    if(mDir.directory().empty()) return;
    auto siblings = mDir.siblingDirectories();
    auto it = std::find(siblings.begin(), siblings.end(), mDir.directory());
    if(it == siblings.end()) return;
    long pos = it - siblings.begin();
    for(long p = pos + step; p >= 0 && p < (long)siblings.size(); p += step) {
        if(!DirectoryModel::directoryHasImages(siblings[p])) continue;
        if(!setDirectory(siblings[p])) return;
        mIndex = -1;
        mCurrentPath.clear();
        refreshLists();
        loadIndex(step > 0 ? 0 : mDir.count() - 1);
        showMessage(baseName(siblings[p]), "menuitem/folder16");
        return;
    }
    showMessage(step > 0 ? "No next folder with images" : "No previous folder with images",
                step > 0 ? "notifications/dir_end20" : "notifications/dir_start20");
}

void MainWindow::setMode(ViewMode m) {
    if(m == mMode && gtk_stack_get_visible_child(GTK_STACK(mStack))) {
        gtk_stack_set_visible_child_name(GTK_STACK(mStack), m == ViewMode::Folder ? "folder" : "document");
    }
    mMode = m;
    if(m == ViewMode::Folder) {
        if(mViewer->cropping()) cancelCrop();
        mMenu->hide();
        mCopy->close();
        gtk_stack_set_visible_child_name(GTK_STACK(mStack), "folder");
        mFolder->grid().setCurrent(mIndex >= 0 ? (int)mDir.subdirs().size() + mIndex : mFolder->grid().current(), true);
        mFolder->focusGrid();
    } else {
        gtk_stack_set_visible_child_name(GTK_STACK(mStack), "document");
        gtk_widget_grab_focus(mViewer->widget());
    }
    updateChrome();
    updateTitle();
}

void MainWindow::folderSelect(int itemIndex) {
    int dirs = (int)mDir.subdirs().size();
    if(itemIndex >= dirs) {
        // Selecting in the folder view moves the current file without decoding it yet.
        int fi = itemIndex - dirs;
        if(const FileEntry* e = mDir.at(fi)) {
            mIndex = fi;
            if(mCurrentPath != e->path) {
                mCurrentPath = e->path;
                mImage.reset();
                mViewer->clear();
            }
            mPanel->strip().setCurrent(fi, false);
            updateInfo();
        }
    }
}

void MainWindow::folderActivate(int itemIndex) {
    int dirs = (int)mDir.subdirs().size();
    if(itemIndex < dirs) {
        std::string dir = mDir.subdirs()[itemIndex];
        if(setDirectory(dir)) {
            mIndex = -1;
            mCurrentPath.clear();
            mImage.reset();
            mViewer->clear();
            refreshLists();
            updateInfo();
        }
        return;
    }
    setMode(ViewMode::Document);
    loadIndex(itemIndex - dirs);
}

}  // namespace qimgv
