#include "gui/icons.hpp"

#include <cstring>
#include <map>
#include <string>

namespace qimgv {

// Read PNGs from the resource bundle with cairo directly (fast, no gdk-pixbuf loaders involved).
static cairo_surface_t* loadResourcePng(const std::string& path) {
    GBytes* bytes = g_resources_lookup_data(path.c_str(), G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr);
    if(!bytes) return nullptr;
    struct Reader {
        const unsigned char* data;
        size_t size, pos;
    } r;
    gsize size = 0;
    r.data = static_cast<const unsigned char*>(g_bytes_get_data(bytes, &size));
    r.size = size;
    r.pos = 0;
    cairo_surface_t* s = cairo_image_surface_create_from_png_stream(
        [](void* closure, unsigned char* out, unsigned int len) -> cairo_status_t {
            auto* rd = static_cast<Reader*>(closure);
            if(rd->pos + len > rd->size) return CAIRO_STATUS_READ_ERROR;
            memcpy(out, rd->data + rd->pos, len);
            rd->pos += len;
            return CAIRO_STATUS_SUCCESS;
        },
        &r);
    g_bytes_unref(bytes);
    if(cairo_surface_status(s) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(s);
        return nullptr;
    }
    return s;
}

cairo_surface_t* iconSurface(const char* name, int scale) {
    static std::map<std::string, cairo_surface_t*> cache;
    std::string key = std::string(name) + (scale >= 2 ? "@2x" : "");
    auto it = cache.find(key);
    if(it != cache.end()) return it->second;
    cairo_surface_t* s = nullptr;
    if(scale >= 2) {
        s = loadResourcePng(std::string("/qimgv/icons/") + name + "@2x.png");
        if(s) cairo_surface_set_device_scale(s, 2, 2);
    }
    if(!s) s = loadResourcePng(std::string("/qimgv/icons/") + name + ".png");
    cache[key] = s;
    return s;
}

int iconLogicalSize(const char* name, int* height) {
    cairo_surface_t* s = iconSurface(name, 1);
    if(!s) {
        if(height) *height = 16;
        return 16;
    }
    if(height) *height = cairo_image_surface_get_height(s);
    return cairo_image_surface_get_width(s);
}

void paintIcon(cairo_t* cr, const char* name, double x, double y, int scale) {
    cairo_surface_t* s = iconSurface(name, scale);
    if(s) cairo_mask_surface(cr, s, x, y);
}

static gboolean drawIcon(GtkWidget* w, cairo_t* cr, gpointer data) {
    const char* name = static_cast<const char*>(data);
    GtkStyleContext* ctx = gtk_widget_get_style_context(w);
    GdkRGBA c;
    gtk_style_context_get_color(ctx, gtk_style_context_get_state(ctx), &c);
    int iw, ih;
    iw = iconLogicalSize(name, &ih);
    double x = (gtk_widget_get_allocated_width(w) - iw) / 2.0;
    double y = (gtk_widget_get_allocated_height(w) - ih) / 2.0;
    gdk_cairo_set_source_rgba(cr, &c);
    paintIcon(cr, name, (int)x, (int)y, gtk_widget_get_scale_factor(w));
    return FALSE;
}

GtkWidget* iconWidget(const char* name) {
    GtkWidget* da = gtk_drawing_area_new();
    char* copy = g_strdup(name);
    g_object_set_data_full(G_OBJECT(da), "icon-name", copy, g_free);
    int h;
    int w = iconLogicalSize(name, &h);
    gtk_widget_set_size_request(da, w, h);
    gtk_style_context_add_class(gtk_widget_get_style_context(da), "qimgv-icon");
    g_signal_connect(da, "draw", G_CALLBACK(drawIcon), copy);
    return da;
}

static void setupButton(GtkWidget* b, const char* name, const char* tooltip) {
    gtk_button_set_relief(GTK_BUTTON(b), GTK_RELIEF_NONE);
    gtk_widget_set_can_focus(b, FALSE);
    gtk_container_add(GTK_CONTAINER(b), iconWidget(name));
    if(tooltip) gtk_widget_set_tooltip_text(b, tooltip);
    gtk_style_context_add_class(gtk_widget_get_style_context(b), "qimgv-iconbtn");
}

GtkWidget* iconButton(const char* name, const char* tooltip) {
    GtkWidget* b = gtk_button_new();
    setupButton(b, name, tooltip);
    return b;
}

GtkWidget* iconToggleButton(const char* name, const char* tooltip) {
    GtkWidget* b = gtk_toggle_button_new();
    setupButton(b, name, tooltip);
    return b;
}

GtkWidget* menuRow(const char* icon, const char* label, const char* shortcut) {
    GtkWidget* b = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(b), GTK_RELIEF_NONE);
    gtk_widget_set_can_focus(b, FALSE);
    gtk_style_context_add_class(gtk_widget_get_style_context(b), "qimgv-menurow");
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget* ic = icon ? iconWidget(icon) : gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_size_request(ic, 16, 16);
    gtk_box_pack_start(GTK_BOX(box), ic, FALSE, FALSE, 0);
    GtkWidget* l = gtk_label_new(label);
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_box_pack_start(GTK_BOX(box), l, TRUE, TRUE, 0);
    if(shortcut && *shortcut) {
        GtkWidget* s = gtk_label_new(shortcut);
        gtk_style_context_add_class(gtk_widget_get_style_context(s), "dim-label");
        gtk_box_pack_end(GTK_BOX(box), s, FALSE, FALSE, 0);
    }
    gtk_container_add(GTK_CONTAINER(b), box);
    return b;
}

GdkPixbuf* appIcon(int size) {
    cairo_surface_t* s = loadResourcePng("/qimgv/logo.png");
    if(!s) return nullptr;
    int w = cairo_image_surface_get_width(s);
    cairo_surface_t* scaled = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
    cairo_t* cr = cairo_create(scaled);
    cairo_scale(cr, (double)size / w, (double)size / w);
    cairo_set_source_surface(cr, s, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
    cairo_paint(cr);
    cairo_destroy(cr);
    GdkPixbuf* pb = gdk_pixbuf_get_from_surface(scaled, 0, 0, size, size);
    cairo_surface_destroy(scaled);
    cairo_surface_destroy(s);
    return pb;
}

}  // namespace qimgv
