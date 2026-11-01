#include "gui/overlays.hpp"

#include <cstring>

#include "gui/icons.hpp"
#include "gui/style.hpp"
#include "settings.hpp"

namespace qimgv {

static std::string shortPath(const std::string& p) {
    const char* home = g_get_home_dir();
    size_t n = strlen(home);
    if(p.compare(0, n, home) == 0 && (p.size() == n || p[n] == '/')) return "~" + p.substr(n);
    return p;
}

// ---- FloatingMessage --------------------------------------------------------

FloatingMessage::FloatingMessage() {
    mBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    addClass(mBox, "qimgv-message");
    gtk_widget_set_halign(mBox, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(mBox, GTK_ALIGN_START);
    gtk_widget_set_margin_top(mBox, 36);
    gtk_widget_set_no_show_all(mBox, TRUE);
    mIconHolder = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    mLabel = gtk_label_new("");
    gtk_label_set_line_wrap(GTK_LABEL(mLabel), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(mLabel), 80);
    gtk_box_pack_start(GTK_BOX(mBox), mIconHolder, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mBox), mLabel, FALSE, FALSE, 0);
}

void FloatingMessage::show(const std::string& text, const char* icon, int ms, bool error) {
    GList* children = gtk_container_get_children(GTK_CONTAINER(mIconHolder));
    for(GList* l = children; l; l = l->next) gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);
    if(icon) {
        GtkWidget* ic = iconWidget(icon);
        gtk_widget_set_valign(ic, GTK_ALIGN_CENTER);
        gtk_container_add(GTK_CONTAINER(mIconHolder), ic);
        gtk_widget_show(ic);
    }
    gtk_widget_set_visible(mIconHolder, icon != nullptr);
    gtk_widget_set_visible(mLabel, !text.empty());
    gtk_label_set_text(GTK_LABEL(mLabel), text.c_str());
    GtkStyleContext* ctx = gtk_widget_get_style_context(mBox);
    if(error)
        gtk_style_context_add_class(ctx, "error");
    else
        gtk_style_context_remove_class(ctx, "error");
    gtk_widget_show(mBox);
    if(mTimer) g_source_remove(mTimer);
    mTimer = g_timeout_add(
        ms,
        [](gpointer p) -> gboolean {
            auto* m = static_cast<FloatingMessage*>(p);
            m->mTimer = 0;
            gtk_widget_hide(m->mBox);
            return G_SOURCE_REMOVE;
        },
        this);
}

void FloatingMessage::hide() {
    if(mTimer) g_source_remove(mTimer);
    mTimer = 0;
    gtk_widget_hide(mBox);
}

// ---- CopyOverlay ------------------------------------------------------------

CopyOverlay::CopyOverlay() {
    mBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    addClass(mBox, "qimgv-overlay");
    gtk_widget_set_halign(mBox, GTK_ALIGN_START);
    gtk_widget_set_valign(mBox, GTK_ALIGN_END);
    gtk_widget_set_margin_start(mBox, 12);
    gtk_widget_set_margin_bottom(mBox, 12);
    gtk_widget_set_size_request(mBox, 300, -1);
    gtk_widget_set_no_show_all(mBox, TRUE);

    GtkWidget* head = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    mHeader = gtk_label_new("Copy to...");
    addClass(mHeader, "header");
    gtk_label_set_xalign(GTK_LABEL(mHeader), 0);
    gtk_box_pack_start(GTK_BOX(head), mHeader, TRUE, TRUE, 4);
    GtkWidget* close = iconButton("overlay/close16", "Close");
    g_signal_connect(close, "clicked", gcb(+[](GtkButton*, gpointer self) {
                         static_cast<CopyOverlay*>(self)->close();
                     }),
                     this);
    gtk_box_pack_end(GTK_BOX(head), close, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mBox), head, FALSE, FALSE, 0);

    for(int i = 0; i < 9; i++) {
        GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        GtkWidget* pick = gtk_button_new();
        addClass(pick, "qimgv-flat");
        gtk_button_set_relief(GTK_BUTTON(pick), GTK_RELIEF_NONE);
        gtk_widget_set_can_focus(pick, FALSE);
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        char num[4];
        snprintf(num, sizeof(num), "%d", i + 1);
        GtkWidget* n = gtk_label_new(num);
        addClass(n, "dim-label");
        mLabels[i] = gtk_label_new("");
        gtk_label_set_xalign(GTK_LABEL(mLabels[i]), 0);
        gtk_label_set_ellipsize(GTK_LABEL(mLabels[i]), PANGO_ELLIPSIZE_START);
        gtk_label_set_max_width_chars(GTK_LABEL(mLabels[i]), 36);
        gtk_box_pack_start(GTK_BOX(inner), n, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(inner), mLabels[i], TRUE, TRUE, 0);
        gtk_container_add(GTK_CONTAINER(pick), inner);
        g_object_set_data(G_OBJECT(pick), "slot", GINT_TO_POINTER(i));
        g_signal_connect(pick, "clicked", gcb(+[](GtkButton* b, gpointer self) {
                             auto* o = static_cast<CopyOverlay*>(self);
                             if(o->onPick) o->onPick(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "slot")));
                         }),
                         this);
        GtkWidget* change = iconButton("buttons/panel/folder16", "Choose folder");
        g_object_set_data(G_OBJECT(change), "slot", GINT_TO_POINTER(i));
        g_signal_connect(change, "clicked", gcb(+[](GtkButton* b, gpointer self) {
                             auto* o = static_cast<CopyOverlay*>(self);
                             if(o->onChangePath)
                                 o->onChangePath(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "slot")));
                         }),
                         this);
        gtk_box_pack_start(GTK_BOX(row), pick, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(row), change, FALSE, FALSE, 0);
        mRows[i] = row;
        gtk_box_pack_start(GTK_BOX(mBox), row, FALSE, FALSE, 0);
    }
    GList* children = gtk_container_get_children(GTK_CONTAINER(mBox));
    for(GList* l = children; l; l = l->next) gtk_widget_show_all(GTK_WIDGET(l->data));
    g_list_free(children);
}

void CopyOverlay::refresh() {
    auto& paths = settings().savedPaths;
    for(int i = 0; i < 9; i++) {
        std::string p = i < (int)paths.size() ? paths[i] : "";
        gtk_label_set_text(GTK_LABEL(mLabels[i]), shortPath(p).c_str());
        gtk_widget_set_tooltip_text(mRows[i], p.c_str());
    }
}

void CopyOverlay::open(bool move) {
    mMove = move;
    gtk_label_set_text(GTK_LABEL(mHeader), move ? "Move to..." : "Copy to...");
    refresh();
    gtk_widget_show(mBox);
}

void CopyOverlay::close() {
    if(!visible()) return;
    gtk_widget_hide(mBox);
    if(onClosed) onClosed();
}

bool CopyOverlay::visible() const { return gtk_widget_get_visible(mBox); }

// ---- RenameOverlay ----------------------------------------------------------

RenameOverlay::RenameOverlay() {
    mBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    addClass(mBox, "qimgv-overlay");
    gtk_widget_set_halign(mBox, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(mBox, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(mBox, 420, -1);
    gtk_widget_set_no_show_all(mBox, TRUE);
    GtkWidget* l = gtk_label_new("Rename");
    addClass(l, "header");
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    mEntry = gtk_entry_new();
    gtk_entry_set_activates_default(GTK_ENTRY(mEntry), FALSE);
    g_signal_connect(mEntry, "activate", gcb(+[](GtkEntry* e, gpointer self) {
                         auto* o = static_cast<RenameOverlay*>(self);
                         std::string name = gtk_entry_get_text(e);
                         o->close();
                         if(o->onRename) o->onRename(name);
                     }),
                     this);
    g_signal_connect(mEntry, "key-press-event", gcb(+[](GtkWidget*, GdkEventKey* ev, gpointer self) -> gboolean {
                         if(ev->keyval == GDK_KEY_Escape) {
                             static_cast<RenameOverlay*>(self)->close();
                             return TRUE;
                         }
                         return FALSE;
                     }),
                     this);
    GtkWidget* hint = gtk_label_new("Enter to rename, Esc to cancel");
    addClass(hint, "dim-label");
    gtk_label_set_xalign(GTK_LABEL(hint), 0);
    gtk_box_pack_start(GTK_BOX(mBox), l, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mBox), mEntry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mBox), hint, FALSE, FALSE, 0);
    gtk_widget_show(l);
    gtk_widget_show(mEntry);
    gtk_widget_show(hint);
}

void RenameOverlay::open(const std::string& name) {
    gtk_entry_set_text(GTK_ENTRY(mEntry), name.c_str());
    gtk_widget_show(mBox);
    gtk_widget_grab_focus(mEntry);
    // Select the name without the extension.
    auto dot = name.find_last_of('.');
    int end = dot == std::string::npos || dot == 0 ? -1 : (int)g_utf8_strlen(name.c_str(), (gssize)dot);
    gtk_editable_select_region(GTK_EDITABLE(mEntry), 0, end);
}

void RenameOverlay::close() {
    if(!visible()) return;
    gtk_widget_hide(mBox);
    if(onClosed) onClosed();
}

bool RenameOverlay::visible() const { return gtk_widget_get_visible(mBox); }

// ---- SaveConfirmOverlay -----------------------------------------------------

SaveConfirmOverlay::SaveConfirmOverlay() {
    mBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    addClass(mBox, "qimgv-overlay");
    gtk_widget_set_halign(mBox, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(mBox, GTK_ALIGN_END);
    gtk_widget_set_margin_bottom(mBox, 12);
    gtk_widget_set_no_show_all(mBox, TRUE);
    GtkWidget* l = gtk_label_new("Image was edited.");
    gtk_box_pack_start(GTK_BOX(mBox), l, FALSE, FALSE, 6);
    auto add = [&](const char* label, const char* action) {
        GtkWidget* b = gtk_button_new_with_label(label);
        gtk_widget_set_can_focus(b, FALSE);
        g_object_set_data_full(G_OBJECT(b), "action", g_strdup(action), g_free);
        g_signal_connect(b, "clicked", gcb(+[](GtkButton* btn, gpointer self) {
                             auto* o = static_cast<SaveConfirmOverlay*>(self);
                             if(o->onAction) o->onAction((const char*)g_object_get_data(G_OBJECT(btn), "action"));
                         }),
                         this);
        gtk_box_pack_start(GTK_BOX(mBox), b, FALSE, FALSE, 0);
        gtk_widget_show(b);
    };
    add("Save", "save");
    add("Save as...", "saveAs");
    add("Discard", "discardEdits");
    gtk_widget_show(l);
}

void SaveConfirmOverlay::setVisible(bool on) { gtk_widget_set_visible(mBox, on); }

// ---- ImageInfoOverlay -------------------------------------------------------

ImageInfoOverlay::ImageInfoOverlay() {
    mBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    addClass(mBox, "qimgv-overlay");
    gtk_widget_set_halign(mBox, GTK_ALIGN_END);
    gtk_widget_set_valign(mBox, GTK_ALIGN_START);
    gtk_widget_set_margin_end(mBox, 12);
    gtk_widget_set_margin_top(mBox, 12);
    gtk_widget_set_size_request(mBox, 280, -1);
    gtk_widget_set_no_show_all(mBox, TRUE);
    GtkWidget* head = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    head = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(head), iconWidget("overlay/info16"), FALSE, FALSE, 0);
    GtkWidget* l = gtk_label_new("File info");
    addClass(l, "header");
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_box_pack_start(GTK_BOX(head), l, TRUE, TRUE, 0);
    GtkWidget* close = iconButton("overlay/close16", "Close");
    g_signal_connect(close, "clicked", gcb(+[](GtkButton*, gpointer self) {
                         static_cast<ImageInfoOverlay*>(self)->close();
                     }),
                     this);
    gtk_box_pack_end(GTK_BOX(head), close, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mBox), head, FALSE, FALSE, 0);
    mGrid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(mGrid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(mGrid), 3);
    gtk_box_pack_start(GTK_BOX(mBox), mGrid, FALSE, FALSE, 0);
    gtk_widget_show_all(head);
    gtk_widget_show(mGrid);
}

void ImageInfoOverlay::setInfo(const InfoList& file, const InfoList& exif) {
    GList* children = gtk_container_get_children(GTK_CONTAINER(mGrid));
    for(GList* l = children; l; l = l->next) gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);
    int row = 0;
    auto add = [&](const std::string& k, const std::string& v) {
        GtkWidget* kl = gtk_label_new(k.c_str());
        addClass(kl, "dim-label");
        gtk_label_set_xalign(GTK_LABEL(kl), 1);
        gtk_widget_set_valign(kl, GTK_ALIGN_START);
        GtkWidget* vl = gtk_label_new(v.c_str());
        gtk_label_set_xalign(GTK_LABEL(vl), 0);
        gtk_label_set_selectable(GTK_LABEL(vl), TRUE);
        gtk_widget_set_can_focus(vl, FALSE);
        gtk_label_set_line_wrap(GTK_LABEL(vl), TRUE);
        gtk_label_set_line_wrap_mode(GTK_LABEL(vl), PANGO_WRAP_WORD_CHAR);
        gtk_label_set_max_width_chars(GTK_LABEL(vl), 34);
        gtk_grid_attach(GTK_GRID(mGrid), kl, 0, row, 1, 1);
        gtk_grid_attach(GTK_GRID(mGrid), vl, 1, row, 1, 1);
        row++;
    };
    for(auto& [k, v] : file) add(k, v);
    if(!exif.empty()) {
        GtkWidget* sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_widget_set_margin_top(sep, 4);
        gtk_widget_set_margin_bottom(sep, 4);
        gtk_grid_attach(GTK_GRID(mGrid), sep, 0, row++, 2, 1);
        for(auto& [k, v] : exif) add(k, v);
    }
    gtk_widget_show_all(mGrid);
}

void ImageInfoOverlay::toggle() { gtk_widget_set_visible(mBox, !visible()); }
bool ImageInfoOverlay::visible() const { return gtk_widget_get_visible(mBox); }
void ImageInfoOverlay::close() { gtk_widget_hide(mBox); }

// ---- FullscreenInfo ---------------------------------------------------------

FullscreenInfo::FullscreenInfo() {
    mBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    addClass(mBox, "qimgv-fsinfo");
    gtk_widget_set_halign(mBox, GTK_ALIGN_START);
    gtk_widget_set_valign(mBox, GTK_ALIGN_START);
    gtk_widget_set_no_show_all(mBox, TRUE);
    mTitle = gtk_label_new("");
    mDetails = gtk_label_new("");
    addClass(mDetails, "dim-label");
    for(GtkWidget* l : {mTitle, mDetails}) {
        gtk_label_set_xalign(GTK_LABEL(l), 0);
        gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_MIDDLE);
        gtk_label_set_max_width_chars(GTK_LABEL(l), 70);
        gtk_box_pack_start(GTK_BOX(mBox), l, FALSE, FALSE, 0);
        gtk_widget_show(l);
    }
}

void FullscreenInfo::setText(const std::string& title, const std::string& details) {
    gtk_label_set_text(GTK_LABEL(mTitle), title.c_str());
    gtk_label_set_text(GTK_LABEL(mDetails), details.c_str());
    gtk_widget_set_visible(mDetails, !details.empty());
}

// ---- ControlsOverlay ----------------------------------------------------------

ControlsOverlay::ControlsOverlay() {
    mBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    addClass(mBox, "qimgv-overlay");
    gtk_style_context_add_class(gtk_widget_get_style_context(mBox), "controls");
    gtk_widget_set_halign(mBox, GTK_ALIGN_END);
    gtk_widget_set_valign(mBox, GTK_ALIGN_START);
    gtk_widget_set_no_show_all(mBox, TRUE);
    auto add = [&](const char* icon, const char* tip, const char* action) {
        GtkWidget* b = iconButton(icon, tip);
        g_object_set_data_full(G_OBJECT(b), "action", g_strdup(action), g_free);
        g_signal_connect(b, "clicked", gcb(+[](GtkButton* btn, gpointer self) {
                             auto* o = static_cast<ControlsOverlay*>(self);
                             if(o->onAction) o->onAction((const char*)g_object_get_data(G_OBJECT(btn), "action"));
                         }),
                         this);
        gtk_box_pack_start(GTK_BOX(mBox), b, FALSE, FALSE, 0);
        gtk_widget_show_all(b);
    };
    add("buttons/panel/folderview20", "Folder view", "folderView");
    add("buttons/panel/settings20", "Settings", "openSettings");
    add("buttons/panel/close16", "Exit", "exit");
}

}  // namespace qimgv
