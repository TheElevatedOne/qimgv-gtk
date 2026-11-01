#include "gui/folderview.hpp"

#include <algorithm>

#include "core/fileops.hpp"
#include "gui/icons.hpp"
#include "gui/style.hpp"

namespace qimgv {

enum { COL_NAME, COL_PATH, N_COLS };

FolderView::FolderView(Thumbnailer& thumbs) {
    mRoot = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    mGrid = std::make_unique<ThumbGrid>(ThumbGrid::Mode::Grid, thumbs);
    mGrid->setThumbSize(settings().folderViewIconSize);
    buildTopBar();
    mPaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    buildPlaces();
    gtk_paned_pack1(GTK_PANED(mPaned), mPlaces, FALSE, FALSE);
    gtk_paned_pack2(GTK_PANED(mPaned), mGrid->widget(), TRUE, FALSE);
    gtk_paned_set_position(GTK_PANED(mPaned), settings().placesPanelWidth);
    g_signal_connect(mPaned, "notify::position", gcb(+[](GObject* o, GParamSpec*, gpointer) {
                         int pos = gtk_paned_get_position(GTK_PANED(o));
                         if(pos > 50) settings().placesPanelWidth = pos;
                     }),
                     nullptr);
    gtk_box_pack_start(GTK_BOX(mRoot), mPaned, TRUE, TRUE, 0);
    gtk_widget_show_all(mRoot);
    setPlacesVisible(settings().placesPanel);

    mGrid->onActivate = [this](int i) {
        if(onActivate) onActivate(i);
    };
    mGrid->onSelect = [this](int i) {
        if(onSelect) onSelect(i);
    };
    mGrid->onRightClick = [this](int i, GdkEventButton* ev) {
        if(onRightClick) onRightClick(i, ev);
    };
}

void FolderView::buildTopBar() {
    GtkWidget* bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    addClass(bar, "qimgv-topbar");
    auto actionButton = [&](const char* icon, const char* tip, const char* action) {
        GtkWidget* b = iconButton(icon, tip);
        g_object_set_data_full(G_OBJECT(b), "action", g_strdup(action), g_free);
        g_signal_connect(b, "clicked", gcb(+[](GtkButton* btn, gpointer self) {
                             auto* f = static_cast<FolderView*>(self);
                             if(f->onAction) f->onAction((const char*)g_object_get_data(G_OBJECT(btn), "action"));
                         }),
                         this);
        return b;
    };
    gtk_box_pack_start(GTK_BOX(bar), actionButton("buttons/panel/document-view20", "Document view", "documentView"),
                       FALSE, FALSE, 0);
    GtkWidget* up = iconButton("buttons/panel/up16", "Parent directory");
    g_signal_connect(up, "clicked", gcb(+[](GtkButton*, gpointer self) {
                         auto* f = static_cast<FolderView*>(self);
                         if(!f->mDir.empty() && f->mDir != "/" && f->onDirectory) f->onDirectory(dirName(f->mDir));
                     }),
                     this);
    gtk_box_pack_start(GTK_BOX(bar), up, FALSE, FALSE, 0);
    mPathLabel = gtk_label_new("");
    addClass(mPathLabel, "qimgv-pathlabel");
    gtk_label_set_xalign(GTK_LABEL(mPathLabel), 0);
    gtk_label_set_ellipsize(GTK_LABEL(mPathLabel), PANGO_ELLIPSIZE_START);
    gtk_label_set_selectable(GTK_LABEL(mPathLabel), FALSE);
    gtk_box_pack_start(GTK_BOX(bar), mPathLabel, TRUE, TRUE, 0);

    mSlider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 80, 400, 10);
    gtk_scale_set_draw_value(GTK_SCALE(mSlider), FALSE);
    gtk_widget_set_size_request(mSlider, 130, -1);
    gtk_widget_set_can_focus(mSlider, FALSE);
    gtk_widget_set_tooltip_text(mSlider, "Thumbnail size");
    gtk_range_set_value(GTK_RANGE(mSlider), settings().folderViewIconSize);
    g_signal_connect(mSlider, "value-changed", gcb(+[](GtkRange* r, gpointer self) {
                         auto* f = static_cast<FolderView*>(self);
                         int v = (int)gtk_range_get_value(r);
                         settings().folderViewIconSize = v;
                         f->mGrid->setThumbSize(v);
                     }),
                     this);
    gtk_box_pack_start(GTK_BOX(bar), mSlider, FALSE, FALSE, 6);

    mSort = gtk_combo_box_text_new();
    for(const char* s : {"A - Z", "Z - A", "Size", "Size (desc)", "Oldest", "Newest"})
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(mSort), s);
    gtk_combo_box_set_active(GTK_COMBO_BOX(mSort), (int)settings().sortMode);
    gtk_widget_set_can_focus(mSort, FALSE);
    gtk_widget_set_valign(mSort, GTK_ALIGN_CENTER);
    g_signal_connect(mSort, "changed", gcb(+[](GtkComboBox* c, gpointer self) {
                         auto* f = static_cast<FolderView*>(self);
                         if(!f->mBlock && f->onSortChanged) f->onSortChanged((SortMode)gtk_combo_box_get_active(c));
                     }),
                     this);
    gtk_box_pack_start(GTK_BOX(bar), mSort, FALSE, FALSE, 4);

    mPlacesToggle = iconToggleButton("buttons/panel/toggle-panel20", "Show places panel");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mPlacesToggle), settings().placesPanel);
    g_signal_connect(mPlacesToggle, "toggled", gcb(+[](GtkToggleButton* b, gpointer self) {
                         auto* f = static_cast<FolderView*>(self);
                         if(f->mBlock) return;
                         settings().placesPanel = gtk_toggle_button_get_active(b);
                         f->setPlacesVisible(settings().placesPanel);
                     }),
                     this);
    gtk_box_pack_start(GTK_BOX(bar), mPlacesToggle, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar), actionButton("buttons/panel/open20", "Open", "open"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar), actionButton("buttons/panel/settings20", "Settings", "openSettings"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar), actionButton("buttons/panel/close16", "Exit", "exit"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mRoot), bar, FALSE, FALSE, 0);
}

static GtkWidget* placesHeader(const char* title, GtkWidget** buttons, int n) {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget* l = gtk_label_new(title);
    addClass(l, "places-header");
    addClass(l, "dim-label");
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_box_pack_start(GTK_BOX(box), l, TRUE, TRUE, 0);
    for(int i = 0; i < n; i++) gtk_box_pack_start(GTK_BOX(box), buttons[i], FALSE, FALSE, 0);
    return box;
}

void FolderView::buildPlaces() {
    mPlaces = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    addClass(mPlaces, "qimgv-places");
    gtk_widget_set_size_request(mPlaces, 120, -1);

    GtkWidget* add = iconButton("buttons/panel-small/add-new12", "Bookmark current folder");
    GtkWidget* remove = iconButton("buttons/panel-small/remove12", "Remove bookmark");
    g_signal_connect(add, "clicked", gcb(+[](GtkButton*, gpointer self) {
                         auto* f = static_cast<FolderView*>(self);
                         auto& b = settings().bookmarks;
                         if(!f->mDir.empty() && std::find(b.begin(), b.end(), f->mDir) == b.end()) {
                             b.push_back(f->mDir);
                             f->refreshBookmarks();
                         }
                     }),
                     this);
    g_signal_connect(remove, "clicked", gcb(+[](GtkButton*, gpointer self) {
                         auto* f = static_cast<FolderView*>(self);
                         GtkListBoxRow* row = gtk_list_box_get_selected_row(GTK_LIST_BOX(f->mBookmarks));
                         if(!row) return;
                         const char* p = (const char*)g_object_get_data(G_OBJECT(row), "path");
                         auto& b = settings().bookmarks;
                         b.erase(std::remove(b.begin(), b.end(), std::string(p)), b.end());
                         f->refreshBookmarks();
                     }),
                     this);
    GtkWidget* bb[] = {add, remove};
    gtk_box_pack_start(GTK_BOX(mPlaces), placesHeader("BOOKMARKS", bb, 2), FALSE, FALSE, 0);
    mBookmarks = gtk_list_box_new();
    gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(mBookmarks), TRUE);
    g_signal_connect(mBookmarks, "row-activated", gcb(+[](GtkListBox*, GtkListBoxRow* row, gpointer self) {
                         auto* f = static_cast<FolderView*>(self);
                         const char* p = (const char*)g_object_get_data(G_OBJECT(row), "path");
                         if(p && f->onDirectory) f->onDirectory(p);
                     }),
                     this);
    gtk_box_pack_start(GTK_BOX(mPlaces), mBookmarks, FALSE, FALSE, 0);
    refreshBookmarks();

    GtkWidget* home = iconButton("buttons/panel-small/home12", "Home folder");
    GtkWidget* root = iconButton("buttons/panel-small/root12", "Root folder");
    g_signal_connect(home, "clicked", gcb(+[](GtkButton*, gpointer self) {
                         static_cast<FolderView*>(self)->setTreeRoot(g_get_home_dir());
                     }),
                     this);
    g_signal_connect(root, "clicked", gcb(+[](GtkButton*, gpointer self) {
                         static_cast<FolderView*>(self)->setTreeRoot("/");
                     }),
                     this);
    GtkWidget* fb[] = {home, root};
    gtk_box_pack_start(GTK_BOX(mPlaces), placesHeader("FILESYSTEM", fb, 2), FALSE, FALSE, 0);

    mStore = gtk_tree_store_new(N_COLS, G_TYPE_STRING, G_TYPE_STRING);
    mTree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(mStore));
    g_object_unref(mStore);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(mTree), FALSE);
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(mTree), FALSE);
    gtk_tree_view_set_activate_on_single_click(GTK_TREE_VIEW(mTree), TRUE);
    GtkCellRenderer* r = gtk_cell_renderer_text_new();
    g_object_set(r, "ellipsize", PANGO_ELLIPSIZE_END, nullptr);
    gtk_tree_view_append_column(GTK_TREE_VIEW(mTree),
                                gtk_tree_view_column_new_with_attributes("", r, "text", COL_NAME, nullptr));
    g_signal_connect(mTree, "test-expand-row",
                     gcb(+[](GtkTreeView* tv, GtkTreeIter* it, GtkTreePath*, gpointer self) -> gboolean {
                         auto* f = static_cast<FolderView*>(self);
                         GtkTreeModel* m = gtk_tree_view_get_model(tv);
                         // Replace the placeholder child with the real subdirectories.
                         GtkTreeIter child;
                         if(gtk_tree_model_iter_children(m, &child, it)) {
                             gchar* p = nullptr;
                             gtk_tree_model_get(m, &child, COL_PATH, &p, -1);
                             bool placeholder = p && !*p;
                             g_free(p);
                             if(!placeholder) return FALSE;
                             gtk_tree_store_remove(f->mStore, &child);
                         }
                         gchar* dir = nullptr;
                         gtk_tree_model_get(m, it, COL_PATH, &dir, -1);
                         if(dir) f->populateTree(it, dir);
                         g_free(dir);
                         return FALSE;
                     }),
                     this);
    g_signal_connect(mTree, "row-activated",
                     gcb(+[](GtkTreeView* tv, GtkTreePath* path, GtkTreeViewColumn*, gpointer self) {
                         auto* f = static_cast<FolderView*>(self);
                         GtkTreeModel* m = gtk_tree_view_get_model(tv);
                         GtkTreeIter it;
                         if(!gtk_tree_model_get_iter(m, &it, path)) return;
                         gchar* dir = nullptr;
                         gtk_tree_model_get(m, &it, COL_PATH, &dir, -1);
                         if(dir && *dir && f->onDirectory) f->onDirectory(dir);
                         g_free(dir);
                     }),
                     this);
    GtkWidget* sw = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(sw), mTree);
    gtk_box_pack_start(GTK_BOX(mPlaces), sw, TRUE, TRUE, 0);
    setTreeRoot(g_get_home_dir());
}

void FolderView::populateTree(GtkTreeIter* parent, const std::string& dir) {
    GDir* d = g_dir_open(dir.c_str(), 0, nullptr);
    if(!d) return;
    std::vector<std::string> names;
    while(const gchar* n = g_dir_read_name(d)) {
        if(n[0] == '.') continue;
        std::string full = joinPath(dir, n);
        if(isDirectory(full)) names.emplace_back(n);
    }
    g_dir_close(d);
    std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) {
        return g_utf8_collate(a.c_str(), b.c_str()) < 0;
    });
    for(auto& n : names) {
        std::string full = joinPath(dir, n);
        GtkTreeIter it, ph;
        gtk_tree_store_append(mStore, &it, parent);
        gtk_tree_store_set(mStore, &it, COL_NAME, n.c_str(), COL_PATH, full.c_str(), -1);
        gtk_tree_store_append(mStore, &ph, &it);  // placeholder so the expander shows
        gtk_tree_store_set(mStore, &ph, COL_NAME, "", COL_PATH, "", -1);
    }
}

void FolderView::setTreeRoot(const std::string& root) {
    gtk_tree_store_clear(mStore);
    GtkTreeIter it, ph;
    gtk_tree_store_append(mStore, &it, nullptr);
    std::string name = root == "/" ? "/" : baseName(root);
    gtk_tree_store_set(mStore, &it, COL_NAME, name.c_str(), COL_PATH, root.c_str(), -1);
    gtk_tree_store_append(mStore, &ph, &it);
    gtk_tree_store_set(mStore, &ph, COL_NAME, "", COL_PATH, "", -1);
    GtkTreePath* p = gtk_tree_model_get_path(GTK_TREE_MODEL(mStore), &it);
    gtk_tree_view_expand_row(GTK_TREE_VIEW(mTree), p, FALSE);
    gtk_tree_path_free(p);
}

void FolderView::refreshBookmarks() {
    GList* children = gtk_container_get_children(GTK_CONTAINER(mBookmarks));
    for(GList* l = children; l; l = l->next) gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);
    for(auto& b : settings().bookmarks) {
        GtkWidget* row = gtk_list_box_row_new();
        g_object_set_data_full(G_OBJECT(row), "path", g_strdup(b.c_str()), g_free);
        GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_set_margin_start(box, 8);
        gtk_widget_set_margin_top(box, 3);
        gtk_widget_set_margin_bottom(box, 3);
        gtk_box_pack_start(GTK_BOX(box), iconWidget("menuitem/folder16"), FALSE, FALSE, 0);
        GtkWidget* l = gtk_label_new(b == "/" ? "/" : baseName(b).c_str());
        gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
        gtk_label_set_xalign(GTK_LABEL(l), 0);
        gtk_box_pack_start(GTK_BOX(box), l, TRUE, TRUE, 0);
        gtk_widget_set_tooltip_text(row, b.c_str());
        gtk_container_add(GTK_CONTAINER(row), box);
        gtk_container_add(GTK_CONTAINER(mBookmarks), row);
    }
    gtk_widget_show_all(mBookmarks);
}

void FolderView::setDirectory(const std::string& dir, std::vector<ThumbGrid::Item> items, int selected) {
    bool changed = dir != mDir;
    mDir = dir;
    gtk_label_set_text(GTK_LABEL(mPathLabel), dir.c_str());
    mGrid->setItems(std::move(items));
    mGrid->setCurrent(selected, true);
    if(changed && selected <= 0) {
        GtkAdjustment* a = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(mGrid->widget()));
        gtk_adjustment_set_value(a, 0);
    }
}

void FolderView::setSortMode(SortMode m) {
    mBlock = true;
    gtk_combo_box_set_active(GTK_COMBO_BOX(mSort), (int)m);
    mBlock = false;
}

void FolderView::setPlacesVisible(bool on) {
    gtk_widget_set_visible(mPlaces, on);
    mBlock = true;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mPlacesToggle), on);
    mBlock = false;
}

void FolderView::focusGrid() { gtk_widget_grab_focus(mGrid->canvas()); }

}  // namespace qimgv
