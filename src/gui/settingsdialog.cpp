#include "gui/settingsdialog.hpp"

#include <vector>

#include "actions.hpp"
#include "config.hpp"
#include "core/thumbnailer.hpp"
#include "gui/dialogs.hpp"
#include "gui/icons.hpp"
#include "gui/style.hpp"
#include "image/formats.hpp"
#include "settings.hpp"

namespace qimgv {

namespace {

struct State {
    GtkWidget* window = nullptr;
    Settings tmp;
    std::function<void()> onApply;
    GtkListStore* shortcuts = nullptr;
    GtkListStore* scripts = nullptr;
    GtkWidget* shortcutView = nullptr;
    GtkWidget* scriptView = nullptr;
};

State* gState = nullptr;  // only one settings window at a time

// ---- binding helpers --------------------------------------------------------

GtkWidget* page() {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    addClass(box, "qimgv-settings-page");
    return box;
}

GtkWidget* scrolled(GtkWidget* child) {
    GtkWidget* sw = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(sw), child);
    return sw;
}

void section(GtkWidget* box, const char* title) {
    GtkWidget* l = gtk_label_new(title);
    addClass(l, "section");
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_box_pack_start(GTK_BOX(box), l, FALSE, FALSE, 0);
}

void check(GtkWidget* box, const char* label, bool* field) {
    GtkWidget* c = gtk_check_button_new_with_label(label);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(c), *field);
    g_signal_connect(c, "toggled", gcb(+[](GtkToggleButton* b, gpointer p) {
                         *static_cast<bool*>(p) = gtk_toggle_button_get_active(b);
                     }),
                     field);
    gtk_box_pack_start(GTK_BOX(box), c, FALSE, FALSE, 0);
}

GtkWidget* row(GtkWidget* box, const char* label, GtkWidget* w) {
    GtkWidget* r = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget* l = gtk_label_new(label);
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_box_pack_start(GTK_BOX(r), l, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(r), w, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), r, FALSE, FALSE, 0);
    return r;
}

void spin(GtkWidget* box, const char* label, int* field, int lo, int hi, int step) {
    GtkWidget* s = gtk_spin_button_new_with_range(lo, hi, step);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(s), *field);
    g_signal_connect(s, "value-changed", gcb(+[](GtkSpinButton* b, gpointer p) {
                         *static_cast<int*>(p) = gtk_spin_button_get_value_as_int(b);
                     }),
                     field);
    row(box, label, s);
}

template <typename E>
void combo(GtkWidget* box, const char* label, E* field, std::initializer_list<const char*> options) {
    static_assert(sizeof(E) == sizeof(int), "enum must be int sized");
    GtkWidget* c = gtk_combo_box_text_new();
    for(auto* o : options) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(c), o);
    gtk_combo_box_set_active(GTK_COMBO_BOX(c), (int)*field);
    g_signal_connect(c, "changed", gcb(+[](GtkComboBox* cb, gpointer p) {
                         int v = gtk_combo_box_get_active(cb);
                         if(v >= 0) *static_cast<int*>(p) = v;
                     }),
                     reinterpret_cast<int*>(field));
    row(box, label, c);
}

// ---- pages ------------------------------------------------------------------

GtkWidget* generalPage(Settings& s) {
    GtkWidget* p = page();
    section(p, "Behaviour");
    combo(p, "Default view", &s.defaultViewMode, {"Image", "Folder view"});
    combo(p, "Sorting", &s.sortMode, {"Name (A - Z)", "Name (Z - A)", "Size", "Size (desc)", "Oldest first", "Newest first"});
    combo(p, "At the end of a folder", &s.folderEndAction, {"Do nothing", "Loop", "Go to next folder"});
    spin(p, "Slideshow interval (ms)", &s.slideshowInterval, 100, 600000, 100);
    check(p, "Preload adjacent images", &s.usePreloader);
    check(p, "Start in fullscreen", &s.fullscreenOnStart);
    check(p, "Hide mouse cursor when idle", &s.cursorAutohide);
    check(p, "Show image details in the window title", &s.windowTitleExtendedInfo);
    section(p, "Files");
    check(p, "Ask before deleting files", &s.confirmDelete);
    check(p, "Ask before moving files to trash", &s.confirmTrash);
    check(p, "Ask to save edited images", &s.showSaveOverlay);
    spin(p, "JPEG / WebP / AVIF / JXL save quality", &s.jpegQuality, 1, 100, 1);
    return scrolled(p);
}

GtkWidget* viewPage(Settings& s) {
    GtkWidget* p = page();
    section(p, "Image");
    combo(p, "Default fit mode", &s.fitMode, {"Fit window", "Fit width", "Original size (1:1)", "Fit window (stretch)"});
    check(p, "Expand images smaller than the window", &s.expandImage);
    spin(p, "Expand limit (x, 0 = no limit)", &s.expandLimit, 0, 8, 1);
    GtkWidget* zs = gtk_spin_button_new_with_range(1, 100, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(zs), s.zoomStep * 100);
    g_signal_connect(zs, "value-changed", gcb(+[](GtkSpinButton* b, gpointer p) {
                         *static_cast<double*>(p) = gtk_spin_button_get_value(b) / 100.0;
                     }),
                     &s.zoomStep);
    row(p, "Zoom step (%)", zs);
    combo(p, "1:1 zoom focus point", &s.focusPoint, {"Top", "Center", "Cursor"});
    combo(p, "Zoom indicator", &s.zoomIndicator, {"Off", "Always", "When zoomed"});
    check(p, "Transparency grid", &s.transparencyGrid);
    check(p, "Mini-map while panning zoomed images", &s.mapOverlay);
    check(p, "Mouse wheel scrolls zoomed images", &s.wheelScrollsZoomed);
    check(p, "Keep zoom level when changing images", &s.keepZoom);
    check(p, "Click window edges to go to previous / next image", &s.clickableEdges);
    section(p, "Panels");
    check(p, "Thumbnail panel", &s.panelEnabled);
    combo(p, "Panel position", &s.panelPosition, {"Top", "Bottom"});
    check(p, "Panel only in fullscreen", &s.panelFullscreenOnly);
    spin(p, "Panel thumbnail size", &s.panelThumbSize, 60, 400, 10);
    check(p, "Square thumbnails", &s.squareThumbnails);
    check(p, "Info bar (windowed)", &s.infoBarWindowed);
    check(p, "Info bar (fullscreen)", &s.infoBarFullscreen);
    check(p, "Folder view: places panel", &s.placesPanel);
    return scrolled(p);
}

GtkWidget* scalingPage(Settings& s) {
    GtkWidget* p = page();
    section(p, "Scaling");
    combo(p, "Scaling filter", &s.scalingFilter, {"Nearest", "Bilinear", "High quality"});
    check(p, "Smooth upscaling", &s.smoothUpscaling);
    check(p, "Smooth animated images", &s.smoothAnimated);
    GtkWidget* hint = gtk_label_new(
        "High quality uses a box filter when downscaling (computed in the background).\n"
        "Toggle filters quickly with the \"Toggle scaling filter\" action.");
    addClass(hint, "dim-label");
    gtk_label_set_xalign(GTK_LABEL(hint), 0);
    gtk_box_pack_start(GTK_BOX(p), hint, FALSE, FALSE, 6);
    return scrolled(p);
}

void fillShortcuts(State* st) {
    gtk_list_store_clear(st->shortcuts);
    for(auto& [seq, action] : st->tmp.shortcuts) {
        GtkTreeIter it;
        gtk_list_store_append(st->shortcuts, &it);
        gtk_list_store_set(st->shortcuts, &it, 0, actionLabel(action).c_str(), 1, seq.c_str(), 2, action.c_str(), -1);
    }
}

bool selectedRow(GtkWidget* view, GtkTreeIter* it, GtkTreeModel** model) {
    GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    return gtk_tree_selection_get_selected(sel, model, it);
}

void editShortcut(State* st, bool add) {
    std::string seq, action = "nextImage";
    GtkTreeIter it;
    GtkTreeModel* m;
    if(!add) {
        if(!selectedRow(st->shortcutView, &it, &m)) return;
        gchar *s = nullptr, *a = nullptr;
        gtk_tree_model_get(m, &it, 1, &s, 2, &a, -1);
        seq = s ? s : "";
        action = a ? a : "";
        g_free(s);
        g_free(a);
    }
    auto r = askShortcut(GTK_WINDOW(st->window), seq, action);
    if(!r) return;
    if(!add) st->tmp.shortcuts.erase(seq);
    st->tmp.shortcuts[r->first] = r->second;
    fillShortcuts(st);
}

GtkWidget* listPage(GtkWidget** viewOut, GtkListStore* store, std::initializer_list<const char*> columns,
                    std::initializer_list<std::pair<const char*, GCallback>> buttons, State* st) {
    GtkWidget* p = page();
    GtkWidget* view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    int col = 0;
    for(auto* c : columns) {
        GtkCellRenderer* r = gtk_cell_renderer_text_new();
        GtkTreeViewColumn* tc = gtk_tree_view_column_new_with_attributes(c, r, "text", col, nullptr);
        gtk_tree_view_column_set_resizable(tc, TRUE);
        gtk_tree_view_column_set_expand(tc, col == 0);
        gtk_tree_view_column_set_sort_column_id(tc, col);
        gtk_tree_view_append_column(GTK_TREE_VIEW(view), tc);
        col++;
    }
    GtkWidget* sw = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);
    gtk_container_add(GTK_CONTAINER(sw), view);
    gtk_box_pack_start(GTK_BOX(p), sw, TRUE, TRUE, 0);
    GtkWidget* bb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    for(auto& [label, cb] : buttons) {
        GtkWidget* b = gtk_button_new_with_label(label);
        g_signal_connect(b, "clicked", cb, st);
        gtk_box_pack_start(GTK_BOX(bb), b, FALSE, FALSE, 0);
    }
    gtk_box_pack_start(GTK_BOX(p), bb, FALSE, FALSE, 0);
    *viewOut = view;
    return p;
}

GtkWidget* controlsPage(State* st) {
    st->shortcuts = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(st->shortcuts), 0, GTK_SORT_ASCENDING);
    fillShortcuts(st);
    GtkWidget* p = listPage(
        &st->shortcutView, st->shortcuts, {"Action", "Shortcut"},
        {{"Add", gcb(+[](GtkButton*, gpointer s) { editShortcut(static_cast<State*>(s), true); })},
         {"Edit", gcb(+[](GtkButton*, gpointer s) { editShortcut(static_cast<State*>(s), false); })},
         {"Remove", gcb(+[](GtkButton*, gpointer s) {
              auto* st = static_cast<State*>(s);
              GtkTreeIter it;
              GtkTreeModel* m;
              if(!selectedRow(st->shortcutView, &it, &m)) return;
              gchar* seq = nullptr;
              gtk_tree_model_get(m, &it, 1, &seq, -1);
              if(seq) st->tmp.shortcuts.erase(seq);
              g_free(seq);
              fillShortcuts(st);
          })},
         {"Reset to defaults", gcb(+[](GtkButton*, gpointer s) {
              auto* st = static_cast<State*>(s);
              st->tmp.shortcuts = defaultShortcuts();
              fillShortcuts(st);
          })}},
        st);
    g_object_unref(st->shortcuts);
    g_signal_connect(st->shortcutView, "row-activated",
                     gcb(+[](GtkTreeView*, GtkTreePath*, GtkTreeViewColumn*, gpointer s) {
                         editShortcut(static_cast<State*>(s), false);
                     }),
                     st);
    return p;
}

void fillScripts(State* st) {
    gtk_list_store_clear(st->scripts);
    for(auto& [name, s] : st->tmp.scripts) {
        GtkTreeIter it;
        gtk_list_store_append(st->scripts, &it);
        gtk_list_store_set(st->scripts, &it, 0, name.c_str(), 1, s.command.c_str(), -1);
    }
}

void editScript(State* st, bool add) {
    std::string name;
    Script script;
    if(!add) {
        GtkTreeIter it;
        GtkTreeModel* m;
        if(!selectedRow(st->scriptView, &it, &m)) return;
        gchar* n = nullptr;
        gtk_tree_model_get(m, &it, 0, &n, -1);
        name = n ? n : "";
        g_free(n);
        script = st->tmp.scripts[name];
    }
    auto r = askScript(GTK_WINDOW(st->window), name, script);
    if(!r) return;
    if(!add && r->first != name) {
        // Renamed: move shortcuts along.
        st->tmp.scripts.erase(name);
        for(auto& [seq, action] : st->tmp.shortcuts)
            if(action == "s:" + name) action = "s:" + r->first;
    }
    st->tmp.scripts[r->first] = r->second;
    fillScripts(st);
}

GtkWidget* scriptsPage(State* st) {
    st->scripts = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    fillScripts(st);
    GtkWidget* p = listPage(
        &st->scriptView, st->scripts, {"Name", "Command"},
        {{"Add", gcb(+[](GtkButton*, gpointer s) { editScript(static_cast<State*>(s), true); })},
         {"Edit", gcb(+[](GtkButton*, gpointer s) { editScript(static_cast<State*>(s), false); })},
         {"Remove", gcb(+[](GtkButton*, gpointer s) {
              auto* st = static_cast<State*>(s);
              GtkTreeIter it;
              GtkTreeModel* m;
              if(!selectedRow(st->scriptView, &it, &m)) return;
              gchar* n = nullptr;
              gtk_tree_model_get(m, &it, 0, &n, -1);
              if(n) {
                  st->tmp.scripts.erase(n);
                  std::string action = std::string("s:") + n;
                  for(auto i = st->tmp.shortcuts.begin(); i != st->tmp.shortcuts.end();)
                      i = i->second == action ? st->tmp.shortcuts.erase(i) : std::next(i);
              }
              g_free(n);
              fillScripts(st);
          })}},
        st);
    g_object_unref(st->scripts);
    g_signal_connect(st->scriptView, "row-activated",
                     gcb(+[](GtkTreeView*, GtkTreePath*, GtkTreeViewColumn*, gpointer s) {
                         editScript(static_cast<State*>(s), false);
                     }),
                     st);
    GtkWidget* hint = gtk_label_new(
        "Run scripts from the context menu, or bind them to a key in Controls.\n"
        "Example command: convert %file% %file%.pdf");
    addClass(hint, "dim-label");
    gtk_label_set_xalign(GTK_LABEL(hint), 0);
    gtk_box_pack_start(GTK_BOX(p), hint, FALSE, FALSE, 0);
    return p;
}

GtkWidget* advancedPage(Settings& s) {
    GtkWidget* p = page();
    section(p, "Thumbnails");
    check(p, "Cache thumbnails on disk", &s.useThumbnailCache);
    spin(p, "Thumbnailer threads", &s.thumbnailerThreads, 1, 32, 1);
    GtkWidget* clear = gtk_button_new_with_label("Clear thumbnail cache");
    g_signal_connect(clear, "clicked", gcb(+[](GtkButton* b, gpointer) {
                         Thumbnailer::clearDiskCache();
                         gtk_button_set_label(b, "Thumbnail cache cleared");
                     }),
                     nullptr);
    row(p, Settings::thumbnailDir().c_str(), clear);
    section(p, "Decoding");
    check(p, "RAW: show the embedded preview instead of developing the raw data (faster)", &s.rawEmbeddedPreview);
    check(p, "Play animations (GIF, APNG, WebP, JPEG XL, AVIF)", &s.animations);
    section(p, "Configuration");
    std::string cfg = Settings::configDir() + "/qimgv-gtk.conf";
    GtkWidget* l = gtk_label_new(cfg.c_str());
    gtk_label_set_selectable(GTK_LABEL(l), TRUE);
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_box_pack_start(GTK_BOX(p), l, FALSE, FALSE, 0);
    return scrolled(p);
}

GtkWidget* aboutPage() {
    GtkWidget* p = page();
    gtk_widget_set_valign(p, GTK_ALIGN_START);
    GdkPixbuf* logo = appIcon(96);
    if(logo) {
        GtkWidget* img = gtk_image_new_from_pixbuf(logo);
        g_object_unref(logo);
        gtk_box_pack_start(GTK_BOX(p), img, FALSE, FALSE, 8);
    }
    GtkWidget* t = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(t), "<big><b>qimgv " QIMGV_VERSION "</b></big>");
    gtk_box_pack_start(GTK_BOX(p), t, FALSE, FALSE, 0);
    GtkWidget* d = gtk_label_new(
        "Fast and easy to use image viewer.\nGTK 3 rewrite of qimgv by easymodo.\n\nLicense: GNU GPL v3 or later");
    gtk_label_set_justify(GTK_LABEL(d), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(p), d, FALSE, FALSE, 0);
    GtkWidget* link = gtk_link_button_new(QIMGV_HOMEPAGE);
    gtk_widget_set_halign(link, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(p), link, FALSE, FALSE, 0);
    std::string fmts = "Supported formats:\n";
    int n = 0;
    for(auto& e : supportedExtensions()) {
        fmts += e + (++n % 14 == 0 ? "\n" : " ");
    }
    GtkWidget* f = gtk_label_new(fmts.c_str());
    addClass(f, "dim-label");
    gtk_label_set_justify(GTK_LABEL(f), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(p), f, FALSE, FALSE, 12);
    return scrolled(p);
}

void apply(State* st) {
    Settings& s = settings();
    // Keep state the main window owns.
    st->tmp.windowWidth = s.windowWidth;
    st->tmp.windowHeight = s.windowHeight;
    st->tmp.maximized = s.maximized;
    st->tmp.bookmarks = s.bookmarks;
    st->tmp.savedPaths = s.savedPaths;
    st->tmp.panelPinned = s.panelPinned;
    st->tmp.folderViewIconSize = s.folderViewIconSize;
    st->tmp.placesPanelWidth = s.placesPanelWidth;
    s = st->tmp;
    s.save();
    if(st->onApply) st->onApply();
}

}  // namespace

void SettingsDialog::open(GtkWindow* parent, std::function<void()> onApply) {
    if(gState) {
        gtk_window_present(GTK_WINDOW(gState->window));
        return;
    }
    auto* st = new State();
    gState = st;
    st->tmp = settings();
    st->onApply = std::move(onApply);
    GtkWidget* w = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    st->window = w;
    gtk_window_set_title(GTK_WINDOW(w), "qimgv settings");
    gtk_window_set_transient_for(GTK_WINDOW(w), parent);
    gtk_window_set_modal(GTK_WINDOW(w), TRUE);
    gtk_window_set_decorated(GTK_WINDOW(w), FALSE);
    gtk_window_set_default_size(GTK_WINDOW(w), 780, 580);
    gtk_window_set_position(GTK_WINDOW(w), GTK_WIN_POS_CENTER_ON_PARENT);
    gtk_window_set_type_hint(GTK_WINDOW(w), GDK_WINDOW_TYPE_HINT_DIALOG);
    addClass(w, "qimgv");

    GtkWidget* outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget* main = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget* stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_NONE);
    GtkWidget* side = gtk_list_box_new();
    addClass(side, "qimgv-settings-sidebar");
    gtk_widget_set_size_request(side, 170, -1);
    struct PageDef {
        const char* id;
        const char* label;
        const char* icon;
        GtkWidget* widget;
    };
    std::vector<PageDef> pages = {
        {"general", "General", "settings/general32", generalPage(st->tmp)},
        {"view", "View", "settings/view32", viewPage(st->tmp)},
        {"scaling", "Scaling", "settings/scale32", scalingPage(st->tmp)},
        {"controls", "Controls", "settings/shortcuts32", controlsPage(st)},
        {"scripts", "Scripts", "settings/terminal32", scriptsPage(st)},
        {"advanced", "Advanced", "settings/advanced32", advancedPage(st->tmp)},
        {"about", "About", "settings/about32", aboutPage()},
    };
    for(auto& pd : pages) {
        gtk_stack_add_named(GTK_STACK(stack), pd.widget, pd.id);
        GtkWidget* r = gtk_list_box_row_new();
        g_object_set_data(G_OBJECT(r), "page", (gpointer)pd.id);
        GtkWidget* b = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_box_pack_start(GTK_BOX(b), iconWidget(pd.icon), FALSE, FALSE, 0);
        GtkWidget* l = gtk_label_new(pd.label);
        gtk_label_set_xalign(GTK_LABEL(l), 0);
        gtk_box_pack_start(GTK_BOX(b), l, TRUE, TRUE, 0);
        gtk_container_add(GTK_CONTAINER(r), b);
        gtk_container_add(GTK_CONTAINER(side), r);
    }
    g_signal_connect(side, "row-selected", gcb(+[](GtkListBox*, GtkListBoxRow* r, gpointer stack) {
                         if(r) gtk_stack_set_visible_child_name(GTK_STACK(stack), (const char*)g_object_get_data(G_OBJECT(r), "page"));
                     }),
                     stack);
    gtk_list_box_select_row(GTK_LIST_BOX(side), gtk_list_box_get_row_at_index(GTK_LIST_BOX(side), 0));
    gtk_box_pack_start(GTK_BOX(main), side, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main), stack, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(outer), main, TRUE, TRUE, 0);

    GtkWidget* buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    addClass(buttons, "qimgv-dialog-buttons");
    GtkWidget* cancel = gtk_button_new_with_label("Cancel");
    GtkWidget* applyB = gtk_button_new_with_label("Apply");
    GtkWidget* ok = gtk_button_new_with_label("OK");
    addClass(ok, "suggested-action");
    gtk_box_pack_end(GTK_BOX(buttons), ok, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(buttons), applyB, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(buttons), cancel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(outer), buttons, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(w), outer);

    g_signal_connect(cancel, "clicked", gcb(+[](GtkButton*, gpointer s) {
                         gtk_widget_destroy(static_cast<State*>(s)->window);
                     }),
                     st);
    g_signal_connect(applyB, "clicked", gcb(+[](GtkButton*, gpointer s) { apply(static_cast<State*>(s)); }), st);
    g_signal_connect(ok, "clicked", gcb(+[](GtkButton*, gpointer s) {
                         auto* st = static_cast<State*>(s);
                         apply(st);
                         gtk_widget_destroy(st->window);
                     }),
                     st);
    g_signal_connect(w, "key-press-event", gcb(+[](GtkWidget* win, GdkEventKey* e, gpointer) -> gboolean {
                         if(e->keyval == GDK_KEY_Escape) {
                             gtk_widget_destroy(win);
                             return TRUE;
                         }
                         return FALSE;
                     }),
                     nullptr);
    g_signal_connect(w, "destroy", gcb(+[](GtkWidget*, gpointer s) {
                         delete static_cast<State*>(s);
                         gState = nullptr;
                     }),
                     st);
    gtk_widget_show_all(w);
}

}  // namespace qimgv
