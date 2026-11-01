#include "gui/mainpanel.hpp"

#include "gui/icons.hpp"
#include "gui/style.hpp"
#include "settings.hpp"

namespace qimgv {

MainPanel::MainPanel(Thumbnailer& thumbs) {
    mRevealer = gtk_revealer_new();
    gtk_revealer_set_transition_duration(GTK_REVEALER(mRevealer), 120);
    mBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    addClass(mBox, "qimgv-panel");
    mStrip = std::make_unique<ThumbGrid>(ThumbGrid::Mode::Strip, thumbs);
    gtk_widget_set_hexpand(mStrip->widget(), TRUE);
    gtk_box_pack_start(GTK_BOX(mBox), mStrip->widget(), TRUE, TRUE, 0);

    GtkWidget* buttons = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    addClass(buttons, "qimgv-panel-buttons");
    gtk_widget_set_margin_start(buttons, 4);
    auto add = [&](const char* icon, const char* tip, const char* action, bool end) {
        GtkWidget* b = iconButton(icon, tip);
        g_object_set_data_full(G_OBJECT(b), "action", g_strdup(action), g_free);
        g_signal_connect(b, "clicked", gcb(+[](GtkButton* btn, gpointer self) {
                             auto* p = static_cast<MainPanel*>(self);
                             if(p->onAction) p->onAction((const char*)g_object_get_data(G_OBJECT(btn), "action"));
                         }),
                         this);
        if(end)
            gtk_box_pack_end(GTK_BOX(buttons), b, FALSE, FALSE, 0);
        else
            gtk_box_pack_start(GTK_BOX(buttons), b, FALSE, FALSE, 0);
    };
    add("buttons/panel/close16", "Exit", "exit", false);
    add("buttons/panel/folderview20", "Folder view", "folderView", false);
    mPin = iconToggleButton("buttons/panel/pin-panel20", "Pin panel");
    g_signal_connect(mPin, "toggled", gcb(+[](GtkToggleButton* b, gpointer self) {
                         auto* p = static_cast<MainPanel*>(self);
                         if(!p->mBlockPin && p->onPinToggled) p->onPinToggled(gtk_toggle_button_get_active(b));
                     }),
                     this);
    gtk_box_pack_start(GTK_BOX(buttons), mPin, FALSE, FALSE, 0);
    add("buttons/panel/settings20", "Settings", "openSettings", true);
    add("buttons/panel/open20", "Open", "open", true);
    gtk_box_pack_start(GTK_BOX(mBox), buttons, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(mRevealer), mBox);
    setPosition(settings().panelPosition);
}

void MainPanel::setPosition(PanelPosition p) {
    GtkStyleContext* ctx = gtk_widget_get_style_context(mBox);
    if(p == PanelPosition::Bottom) {
        gtk_style_context_add_class(ctx, "bottom");
        gtk_revealer_set_transition_type(GTK_REVEALER(mRevealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
        gtk_widget_set_valign(mRevealer, GTK_ALIGN_END);
    } else {
        gtk_style_context_remove_class(ctx, "bottom");
        gtk_revealer_set_transition_type(GTK_REVEALER(mRevealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
        gtk_widget_set_valign(mRevealer, GTK_ALIGN_START);
    }
}

void MainPanel::setPinned(bool pinned) {
    mBlockPin = true;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mPin), pinned);
    mBlockPin = false;
}

void MainPanel::reveal(bool show) {
    if(show != (bool)gtk_revealer_get_reveal_child(GTK_REVEALER(mRevealer)))
        gtk_revealer_set_reveal_child(GTK_REVEALER(mRevealer), show);
}

bool MainPanel::revealed() const { return gtk_revealer_get_reveal_child(GTK_REVEALER(mRevealer)); }

int MainPanel::height() const {
    int h = 0;
    gtk_widget_get_preferred_height(mBox, nullptr, &h);
    return h;
}

}  // namespace qimgv
