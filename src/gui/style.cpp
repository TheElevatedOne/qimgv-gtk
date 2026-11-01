#include "gui/style.hpp"

namespace qimgv {

// Square corners, flat buttons, opaque panels. Colours are the theme's named colours so the
// GTK theme (e.g. a pywal generated one) decides the palette.
static const char* kCss = R"CSS(
.qimgv, .qimgv * { border-radius: 0; }
.qimgv { background-color: @theme_bg_color; }

.qimgv-viewer { background-color: @theme_base_color; }

.qimgv button.qimgv-iconbtn, .qimgv button.qimgv-menurow, .qimgv button.qimgv-flat {
  background-image: none; background-color: transparent; border: none; box-shadow: none;
  text-shadow: none; -gtk-icon-shadow: none; outline: none; min-height: 0; min-width: 0;
}
.qimgv button.qimgv-iconbtn { padding: 7px; }
.qimgv button.qimgv-iconbtn:hover, .qimgv button.qimgv-menurow:hover, .qimgv button.qimgv-flat:hover {
  background-color: mix(@theme_bg_color, @theme_fg_color, 0.12);
}
.qimgv button.qimgv-iconbtn:active, .qimgv button.qimgv-iconbtn:checked,
.qimgv button.qimgv-flat:active, .qimgv button.qimgv-flat:checked {
  background-color: @theme_selected_bg_color; color: @theme_selected_fg_color;
}
.qimgv button.qimgv-menurow { padding: 5px 12px 5px 10px; }

/* Thumbnail panel */
.qimgv-panel { background-color: @theme_bg_color; border-bottom: 1px solid @borders; }
.qimgv-panel.bottom { border-bottom: none; border-top: 1px solid @borders; }
.qimgv-panel-buttons { background-color: @theme_bg_color; }
.qimgv-panel-buttons button.qimgv-iconbtn { padding: 5px 7px; }
.qimgv-overlay.controls { padding: 0; }

/* Info bar */
.qimgv-infobar { background-color: @theme_bg_color; border-top: 1px solid @borders; padding: 2px 8px; }
.qimgv-infobar label { font-size: 0.92em; }

/* Overlays: opaque boxes on top of the image */
.qimgv-overlay {
  background-color: @theme_bg_color; color: @theme_fg_color;
  border: 1px solid @borders; padding: 8px;
}
.qimgv-overlay .header { font-weight: bold; }
.qimgv-overlay entry { border-radius: 0; }
.qimgv-message { background-color: @theme_bg_color; border: 1px solid @borders; padding: 6px 12px; }
.qimgv-message.error { color: @error_color; }
.qimgv-contextmenu { background-color: @theme_bg_color; border: 1px solid @borders; padding: 4px 0; }
.qimgv-contextmenu .zoomrow button { padding: 6px 8px; }
.qimgv-contextmenu separator { margin: 4px 0; }
.qimgv-fsinfo { background-color: @theme_bg_color; border: 1px solid @borders; padding: 4px 10px; }
.qimgv-sidepanel { background-color: @theme_bg_color; border-left: 1px solid @borders; padding: 10px; }

/* Folder view */
.qimgv-topbar { background-color: @theme_bg_color; border-bottom: 1px solid @borders; padding: 0 4px; }
.qimgv-pathlabel { padding: 0 8px; }
.qimgv-places { background-color: @theme_bg_color; }
.qimgv-places .places-header { font-size: 0.85em; font-weight: bold; padding: 6px 8px; }
.qimgv-places list, .qimgv-places treeview { background-color: @theme_bg_color; }
.qimgv-grid { background-color: @theme_base_color; }

/* Settings */
.qimgv-settings-sidebar { background-color: @theme_bg_color; border-right: 1px solid @borders; }
.qimgv-settings-sidebar row { padding: 8px 12px; }
.qimgv-settings-page { padding: 18px; }
.qimgv-settings-page .section { font-weight: bold; margin-top: 12px; }
.qimgv-dialog-buttons { border-top: 1px solid @borders; padding: 8px; }
.qimgv-keycapture { padding: 14px; border: 1px solid @borders; background-color: @theme_base_color; }
)CSS";

void installStyle() {
    GtkCssProvider* p = gtk_css_provider_new();
    GError* e = nullptr;
    if(!gtk_css_provider_load_from_data(p, kCss, -1, &e)) {
        g_printerr("qimgv: CSS error: %s\n", e->message);
        g_error_free(e);
    }
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(p),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(p);
}

GdkRGBA themeColor(GtkWidget* w, const char* name, const char* fallback) {
    GdkRGBA c;
    GtkStyleContext* ctx = gtk_widget_get_style_context(w);
    if(!gtk_style_context_lookup_color(ctx, name, &c)) gdk_rgba_parse(&c, fallback);
    return c;
}

void addClass(GtkWidget* w, const char* cls) { gtk_style_context_add_class(gtk_widget_get_style_context(w), cls); }

}  // namespace qimgv
