// Fallback for everything else gdk-pixbuf can read (BMP, TIFF, SVG, ICO, TGA, QOI, ...).
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "image/decoder.hpp"

namespace qimgv {

bool decodePixbuf(const std::string& path, const DecodeOptions& opt, ImageData& img, std::string& err) {
    GError* gerr = nullptr;
    GdkPixbuf* pb = nullptr;
    if(opt.targetSize > 0)
        pb = gdk_pixbuf_new_from_file_at_size(path.c_str(), opt.targetSize, opt.targetSize, &gerr);
    else
        pb = gdk_pixbuf_new_from_file(path.c_str(), &gerr);
    if(!pb) {
        err = gerr ? gerr->message : "Unsupported image format";
        if(gerr) g_error_free(gerr);
        return false;
    }
    GdkPixbuf* oriented = gdk_pixbuf_apply_embedded_orientation(pb);
    g_object_unref(pb);
    pb = oriented;
    int w = gdk_pixbuf_get_width(pb), h = gdk_pixbuf_get_height(pb);
    int stride = gdk_pixbuf_get_rowstride(pb);
    const guint8* px = gdk_pixbuf_read_pixels(pb);
    bool alpha = gdk_pixbuf_get_has_alpha(pb);
    cairo_surface_t* s = alpha ? surfaceFromRGBA(px, w, h, (size_t)stride) : surfaceFromRGB(px, w, h, (size_t)stride);
    img.hasAlpha = alpha;
    img.bitDepth = gdk_pixbuf_get_bits_per_sample(pb);
    GdkPixbufFormat* f = gdk_pixbuf_get_file_info(path.c_str(), nullptr, nullptr);
    if(f) {
        gchar* name = gdk_pixbuf_format_get_name(f);
        img.format = name ? name : "";
        for(auto& c : img.format) c = (char)g_ascii_toupper(c);
        g_free(name);
    }
    g_object_unref(pb);
    if(!s) {
        err = "Image is too large";
        return false;
    }
    img.frames.push_back({s, 0});
    return true;
}

}  // namespace qimgv
