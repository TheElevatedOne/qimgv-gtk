#include "gui/croppanel.hpp"

#include "gui/icons.hpp"
#include "gui/style.hpp"

namespace qimgv {

static const struct {
    const char* label;
    double ratio;  // 0 = free, -1 = image
} kAspects[] = {{"Free", 0},          {"Image", -1},       {"1:1", 1.0},        {"4:3", 4.0 / 3},
                {"3:2", 3.0 / 2},     {"16:9", 16.0 / 9},  {"16:10", 16.0 / 10}, {"21:9", 21.0 / 9},
                {"3:4", 3.0 / 4},     {"2:3", 2.0 / 3},    {"9:16", 9.0 / 16}};

CropPanel::CropPanel() {
    mRevealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(mRevealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT);
    gtk_revealer_set_transition_duration(GTK_REVEALER(mRevealer), 120);
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    addClass(box, "qimgv-sidepanel");
    gtk_widget_set_size_request(box, 210, -1);
    GtkWidget* icon = iconWidget("other/image-crop48");
    gtk_widget_set_margin_top(icon, 6);
    gtk_box_pack_start(GTK_BOX(box), icon, FALSE, FALSE, 0);
    GtkWidget* title = gtk_label_new("Crop");
    addClass(title, "header");
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);

    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    auto spin = [&](const char* label, int row) {
        GtkWidget* l = gtk_label_new(label);
        gtk_label_set_xalign(GTK_LABEL(l), 1);
        GtkWidget* s = gtk_spin_button_new_with_range(0, 100000, 1);
        gtk_widget_set_hexpand(s, TRUE);
        g_signal_connect(s, "value-changed", gcb(+[](GtkSpinButton*, gpointer self) {
                             auto* p = static_cast<CropPanel*>(self);
                             if(p->mBlock || !p->onRectChanged) return;
                             CropRect r;
                             r.x = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(p->mX));
                             r.y = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(p->mY));
                             r.w = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(p->mW));
                             r.h = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(p->mH));
                             p->onRectChanged(r);
                         }),
                         this);
        gtk_grid_attach(GTK_GRID(grid), l, 0, row, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), s, 1, row, 1, 1);
        return s;
    };
    mX = spin("X", 0);
    mY = spin("Y", 1);
    mW = spin("Width", 2);
    mH = spin("Height", 3);
    GtkWidget* al = gtk_label_new("Aspect");
    gtk_label_set_xalign(GTK_LABEL(al), 1);
    mAspect = gtk_combo_box_text_new();
    for(auto& a : kAspects) gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(mAspect), a.label);
    gtk_combo_box_set_active(GTK_COMBO_BOX(mAspect), 0);
    g_signal_connect(mAspect, "changed", gcb(+[](GtkComboBox* c, gpointer self) {
                         auto* p = static_cast<CropPanel*>(self);
                         int i = gtk_combo_box_get_active(c);
                         if(i < 0 || !p->onAspectChanged) return;
                         double r = kAspects[i].ratio;
                         if(r < 0) r = p->mImgH > 0 ? (double)p->mImgW / p->mImgH : 0;
                         p->onAspectChanged(r);
                     }),
                     this);
    gtk_grid_attach(GTK_GRID(grid), al, 0, 4, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), mAspect, 1, 4, 1, 1);
    gtk_box_pack_start(GTK_BOX(box), grid, FALSE, FALSE, 6);

    auto button = [&](const char* label, int mode) {
        GtkWidget* b = gtk_button_new_with_label(label);
        g_object_set_data(G_OBJECT(b), "mode", GINT_TO_POINTER(mode));
        g_signal_connect(b, "clicked", gcb(+[](GtkButton* btn, gpointer self) {
                             auto* p = static_cast<CropPanel*>(self);
                             int m = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "mode"));
                             if(m == 2) {
                                 if(p->onCancel) p->onCancel();
                             } else if(p->onCrop) {
                                 p->onCrop(m == 1);
                             }
                         }),
                         this);
        gtk_box_pack_start(GTK_BOX(box), b, FALSE, FALSE, 0);
        return b;
    };
    GtkWidget* crop = button("Crop", 0);
    addClass(crop, "suggested-action");
    button("Crop and save", 1);
    button("Cancel", 2);
    GtkWidget* hint = gtk_label_new("Enter: crop    Esc: cancel\nDrag to select, drag handles to resize.");
    addClass(hint, "dim-label");
    gtk_label_set_justify(GTK_LABEL(hint), GTK_JUSTIFY_CENTER);
    gtk_box_pack_end(GTK_BOX(box), hint, FALSE, FALSE, 6);
    gtk_container_add(GTK_CONTAINER(mRevealer), box);
    gtk_widget_show_all(box);
}

void CropPanel::open(int w, int h) {
    mImgW = w;
    mImgH = h;
    mBlock = true;
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(mX), 0, w - 1);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(mY), 0, h - 1);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(mW), 1, w);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(mH), 1, h);
    gtk_combo_box_set_active(GTK_COMBO_BOX(mAspect), 0);
    mBlock = false;
    gtk_revealer_set_reveal_child(GTK_REVEALER(mRevealer), TRUE);
}

void CropPanel::close() { gtk_revealer_set_reveal_child(GTK_REVEALER(mRevealer), FALSE); }
bool CropPanel::visible() const { return gtk_revealer_get_reveal_child(GTK_REVEALER(mRevealer)); }

void CropPanel::setRect(const CropRect& r) {
    mBlock = true;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(mX), r.x);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(mY), r.y);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(mW), r.w);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(mH), r.h);
    mBlock = false;
}

}  // namespace qimgv
