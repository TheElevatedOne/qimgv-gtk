#include "gui/contextmenu.hpp"

#include <algorithm>

#include "gui/icons.hpp"
#include "gui/style.hpp"

namespace qimgv {

static void actionClicked(GtkButton* b, gpointer self) {
    auto* m = static_cast<ContextMenu*>(self);
    std::string action = (const char*)g_object_get_data(G_OBJECT(b), "action");
    if(action == ":scripts") {
        gtk_stack_set_visible_child_name(GTK_STACK(g_object_get_data(G_OBJECT(b), "stack")), "scripts");
        return;
    }
    if(action == ":back") {
        gtk_stack_set_visible_child_name(GTK_STACK(g_object_get_data(G_OBJECT(b), "stack")), "main");
        return;
    }
    m->hide();
    if(m->onAction) m->onAction(action);
}

static GtkWidget* smallButton(const char* icon, const char* tip, const char* action, ContextMenu* menu) {
    GtkWidget* b = iconButton(icon, tip);
    g_object_set_data_full(G_OBJECT(b), "action", g_strdup(action), g_free);
    g_signal_connect(b, "clicked", G_CALLBACK(actionClicked), menu);
    return b;
}

GtkWidget* ContextMenu::addRow(GtkWidget* box, const char* icon, const char* label, const char* action) {
    GtkWidget* row = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(row), GTK_RELIEF_NONE);
    gtk_widget_set_can_focus(row, FALSE);
    addClass(row, "qimgv-menurow");
    GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget* ic = icon ? iconWidget(icon) : gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_size_request(ic, 16, 16);
    gtk_box_pack_start(GTK_BOX(inner), ic, FALSE, FALSE, 0);
    GtkWidget* l = gtk_label_new(label);
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_box_pack_start(GTK_BOX(inner), l, TRUE, TRUE, 0);
    GtkWidget* sc = gtk_label_new("");
    addClass(sc, "dim-label");
    gtk_widget_set_margin_start(sc, 24);
    gtk_box_pack_end(GTK_BOX(inner), sc, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(row), inner);
    g_object_set_data_full(G_OBJECT(row), "action", g_strdup(action), g_free);
    g_object_set_data(G_OBJECT(row), "stack", mStack);
    g_signal_connect(row, "clicked", G_CALLBACK(actionClicked), this);
    gtk_box_pack_start(GTK_BOX(box), row, FALSE, FALSE, 0);
    mShortcutLabels[action] = sc;
    return row;
}

ContextMenu::ContextMenu() {
    mBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    addClass(mBox, "qimgv-contextmenu");
    gtk_widget_set_halign(mBox, GTK_ALIGN_START);
    gtk_widget_set_valign(mBox, GTK_ALIGN_START);
    gtk_widget_set_no_show_all(mBox, TRUE);
    mStack = gtk_stack_new();
    gtk_stack_set_vhomogeneous(GTK_STACK(mStack), FALSE);
    gtk_stack_set_hhomogeneous(GTK_STACK(mStack), TRUE);
    gtk_box_pack_start(GTK_BOX(mBox), mStack, TRUE, TRUE, 0);

    GtkWidget* main = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget* zoom = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    addClass(zoom, "zoomrow");
    gtk_widget_set_halign(zoom, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(zoom), smallButton("buttons/contextmenu/zoom-in18", "Zoom in", "zoomIn", this), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(zoom), smallButton("buttons/contextmenu/zoom-out18", "Zoom out", "zoomOut", this), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(zoom), smallButton("buttons/contextmenu/zoom-original18", "Original size", "fitNormal", this), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(zoom), smallButton("buttons/contextmenu/fit-window18", "Fit window", "fitWindow", this), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(zoom), smallButton("buttons/contextmenu/fit-width18", "Fit width", "fitWidth", this), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(zoom), smallButton("buttons/contextmenu/fit-height-stretch18", "Fit window (stretch)", "fitWindowStretch", this), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main), zoom, FALSE, FALSE, 0);

    // "Edit" header with lines on both sides, like qimgv.
    GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start(header, 10);
    gtk_widget_set_margin_end(header, 10);
    gtk_widget_set_margin_top(header, 2);
    GtkWidget* s1 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget* s2 = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_valign(s1, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(s2, GTK_ALIGN_CENTER);
    GtkWidget* hl = gtk_label_new("Edit");
    addClass(hl, "dim-label");
    gtk_box_pack_start(GTK_BOX(header), s1, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(header), hl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header), s2, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(main), header, FALSE, FALSE, 0);

    GtkWidget* edit = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    addClass(edit, "zoomrow");
    gtk_widget_set_halign(edit, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(edit), smallButton("menuitem/rotate-left16", "Rotate left", "rotateLeft", this), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(edit), smallButton("menuitem/rotate-right16", "Rotate right", "rotateRight", this), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(edit), smallButton("menuitem/flip-h16", "Flip horizontal", "flipH", this), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(edit), smallButton("menuitem/flip-v16", "Flip vertical", "flipV", this), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(edit), smallButton("menuitem/image-crop16", "Crop", "crop", this), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(edit), smallButton("menuitem/resize16", "Resize", "resize", this), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main), edit, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);

    addRow(main, "menuitem/open16", "Open", "open");
    addRow(main, "menuitem/folderview16", "Folder view", "folderView");
    addRow(main, "menuitem/settings16", "Settings", "openSettings");
    gtk_box_pack_start(GTK_BOX(main), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);
    addRow(main, "menuitem/copy16", "Quick copy", "copyFile");
    addRow(main, "menuitem/move16", "Quick move", "moveFile");
    addRow(main, "overlay/edit16", "Rename", "renameFile");
    addRow(main, "menuitem/trash16", "Move to trash", "moveToTrash");
    gtk_box_pack_start(GTK_BOX(main), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);
    addRow(main, "menuitem/folder16", "Show in file manager", "showInDirectory");
    addRow(main, nullptr, "Set as wallpaper", "setWallpaper");
    addRow(main, "menuitem/print16", "Print", "print");
    GtkWidget* scripts = addRow(main, "menuitem/run16", "Run script", ":scripts");
    (void)scripts;
    gtk_label_set_text(GTK_LABEL(mShortcutLabels[":scripts"]), "▸");
    gtk_stack_add_named(GTK_STACK(mStack), main, "main");

    GtkWidget* sp = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    addRow(sp, "menuitem/back16", "Back", ":back");
    gtk_box_pack_start(GTK_BOX(sp), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);
    mScriptsBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(sp), mScriptsBox, FALSE, FALSE, 0);
    gtk_stack_add_named(GTK_STACK(mStack), sp, "scripts");
    gtk_widget_show_all(mStack);
}

void ContextMenu::update(const std::map<std::string, std::string>& shortcuts,
                         const std::map<std::string, std::string>& scripts) {
    // First (shortest) key bound to each action.
    std::map<std::string, std::string> keyFor;
    for(auto& [seq, action] : shortcuts) {
        auto it = keyFor.find(action);
        if(it == keyFor.end() || seq.size() < it->second.size()) keyFor[action] = seq;
    }
    for(auto& [action, label] : mShortcutLabels) {
        if(action[0] == ':') continue;
        auto it = keyFor.find(action);
        gtk_label_set_text(GTK_LABEL(label), it == keyFor.end() ? "" : it->second.c_str());
    }
    for(auto it = mShortcutLabels.begin(); it != mShortcutLabels.end();)
        it = it->first.rfind("s:", 0) == 0 ? mShortcutLabels.erase(it) : std::next(it);
    GList* children = gtk_container_get_children(GTK_CONTAINER(mScriptsBox));
    for(GList* l = children; l; l = l->next) gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);
    if(scripts.empty()) {
        GtkWidget* l = gtk_label_new("No scripts. Add them in Settings > Scripts.");
        addClass(l, "dim-label");
        gtk_widget_set_margin_start(l, 12);
        gtk_widget_set_margin_end(l, 12);
        gtk_widget_set_margin_top(l, 6);
        gtk_widget_set_margin_bottom(l, 6);
        gtk_box_pack_start(GTK_BOX(mScriptsBox), l, FALSE, FALSE, 0);
    }
    for(auto& [name, key] : scripts) {
        std::string action = "s:" + name;
        addRow(mScriptsBox, "menuitem/run16", name.c_str(), action.c_str());
        gtk_label_set_text(GTK_LABEL(mShortcutLabels[action]), key.c_str());
    }
    gtk_widget_show_all(mScriptsBox);
}

void ContextMenu::popup(double x, double y, int areaW, int areaH) {
    gtk_stack_set_visible_child_name(GTK_STACK(mStack), "main");
    gtk_widget_show(mBox);
    int w = 0, h = 0;
    gtk_widget_get_preferred_width(mBox, nullptr, &w);
    gtk_widget_get_preferred_height(mBox, nullptr, &h);
    int px = (int)x, py = (int)y;
    if(px + w > areaW) px = std::max(0, px - w);
    if(py + h > areaH) py = std::max(0, areaH - h);
    gtk_widget_set_margin_start(mBox, px);
    gtk_widget_set_margin_top(mBox, py);
}

void ContextMenu::hide() { gtk_widget_hide(mBox); }
bool ContextMenu::visible() const { return gtk_widget_get_visible(mBox); }

}  // namespace qimgv
