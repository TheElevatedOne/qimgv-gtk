#include "image/image.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace qimgv {

ImageData::~ImageData() {
    for(auto& f : frames)
        if(f.surface) cairo_surface_destroy(f.surface);
}

void ImageData::setFrames(std::vector<Frame> f) {
    for(auto& old : frames)
        if(old.surface) cairo_surface_destroy(old.surface);
    frames = std::move(f);
    if(!frames.empty() && frames[0].surface) {
        width = cairo_image_surface_get_width(frames[0].surface);
        height = cairo_image_surface_get_height(frames[0].surface);
    }
}

std::shared_ptr<ImageData> ImageData::clone() const {
    auto c = std::make_shared<ImageData>();
    c->path = path;
    c->format = format;
    c->width = width;
    c->height = height;
    c->bitDepth = bitDepth;
    c->hasAlpha = hasAlpha;
    for(auto& f : frames) {
        Frame nf;
        nf.delayMs = f.delayMs;
        nf.surface = cropSurface(f.surface, 0, 0, cairo_image_surface_get_width(f.surface),
                                 cairo_image_surface_get_height(f.surface));
        c->frames.push_back(nf);
    }
    return c;
}

static inline uint8_t premul(uint8_t c, uint8_t a) {
    unsigned t = c * a + 0x80;
    return (uint8_t)((t + (t >> 8)) >> 8);
}

static cairo_surface_t* newSurface(int w, int h) {
    cairo_surface_t* s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    if(cairo_surface_status(s) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(s);
        return nullptr;
    }
    return s;
}

cairo_surface_t* surfaceFromRGBA(const uint8_t* rgba, int w, int h, size_t stride) {
    cairo_surface_t* s = newSurface(w, h);
    if(!s) return nullptr;
    cairo_surface_flush(s);
    uint8_t* dst = cairo_image_surface_get_data(s);
    int dstride = cairo_image_surface_get_stride(s);
    for(int y = 0; y < h; y++) {
        const uint8_t* src = rgba + y * stride;
        uint32_t* d = reinterpret_cast<uint32_t*>(dst + (size_t)y * dstride);
        for(int x = 0; x < w; x++, src += 4) {
            uint8_t a = src[3];
            if(a == 255)
                d[x] = 0xff000000u | (src[0] << 16) | (src[1] << 8) | src[2];
            else if(a == 0)
                d[x] = 0;
            else
                d[x] = ((uint32_t)a << 24) | (premul(src[0], a) << 16) | (premul(src[1], a) << 8) | premul(src[2], a);
        }
    }
    cairo_surface_mark_dirty(s);
    return s;
}

cairo_surface_t* surfaceFromRGB(const uint8_t* rgb, int w, int h, size_t stride) {
    cairo_surface_t* s = newSurface(w, h);
    if(!s) return nullptr;
    cairo_surface_flush(s);
    uint8_t* dst = cairo_image_surface_get_data(s);
    int dstride = cairo_image_surface_get_stride(s);
    for(int y = 0; y < h; y++) {
        const uint8_t* src = rgb + y * stride;
        uint32_t* d = reinterpret_cast<uint32_t*>(dst + (size_t)y * dstride);
        for(int x = 0; x < w; x++, src += 3)
            d[x] = 0xff000000u | (src[0] << 16) | (src[1] << 8) | src[2];
    }
    cairo_surface_mark_dirty(s);
    return s;
}

cairo_surface_t* surfaceFromBGRA(const uint8_t* bgra, int w, int h, size_t stride, bool premultiplied) {
    cairo_surface_t* s = newSurface(w, h);
    if(!s) return nullptr;
    cairo_surface_flush(s);
    uint8_t* dst = cairo_image_surface_get_data(s);
    int dstride = cairo_image_surface_get_stride(s);
    for(int y = 0; y < h; y++) {
        const uint8_t* src = bgra + y * stride;
        uint32_t* d = reinterpret_cast<uint32_t*>(dst + (size_t)y * dstride);
        for(int x = 0; x < w; x++, src += 4) {
            uint8_t a = src[3];
            if(premultiplied || a == 255)
                d[x] = ((uint32_t)a << 24) | (src[2] << 16) | (src[1] << 8) | src[0];
            else if(a == 0)
                d[x] = 0;
            else
                d[x] = ((uint32_t)a << 24) | (premul(src[2], a) << 16) | (premul(src[1], a) << 8) | premul(src[0], a);
        }
    }
    cairo_surface_mark_dirty(s);
    return s;
}

std::vector<uint8_t> surfaceToRGBA(cairo_surface_t* s, bool bgra) {
    cairo_surface_flush(s);
    int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
    int stride = cairo_image_surface_get_stride(s);
    const uint8_t* src = cairo_image_surface_get_data(s);
    std::vector<uint8_t> out((size_t)w * h * 4);
    for(int y = 0; y < h; y++) {
        const uint32_t* p = reinterpret_cast<const uint32_t*>(src + (size_t)y * stride);
        uint8_t* d = out.data() + (size_t)y * w * 4;
        for(int x = 0; x < w; x++, d += 4) {
            uint32_t v = p[x];
            uint8_t a = v >> 24;
            uint8_t r = (v >> 16) & 0xff, g = (v >> 8) & 0xff, b = v & 0xff;
            if(a != 255 && a != 0) {
                r = (uint8_t)std::min(255, (r * 255 + a / 2) / a);
                g = (uint8_t)std::min(255, (g * 255 + a / 2) / a);
                b = (uint8_t)std::min(255, (b * 255 + a / 2) / a);
            }
            if(bgra) {
                d[0] = b; d[1] = g; d[2] = r;
            } else {
                d[0] = r; d[1] = g; d[2] = b;
            }
            d[3] = a;
        }
    }
    return out;
}

bool surfaceHasAlpha(cairo_surface_t* s) {
    cairo_surface_flush(s);
    int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
    int stride = cairo_image_surface_get_stride(s);
    const uint8_t* src = cairo_image_surface_get_data(s);
    for(int y = 0; y < h; y++) {
        const uint32_t* p = reinterpret_cast<const uint32_t*>(src + (size_t)y * stride);
        for(int x = 0; x < w; x++)
            if((p[x] >> 24) != 255) return true;
    }
    return false;
}

// Draw `s` into a new w*h surface with the given transform matrix.
static cairo_surface_t* transformed(cairo_surface_t* s, int w, int h, const cairo_matrix_t& m) {
    cairo_surface_t* out = newSurface(w, h);
    if(!out) return nullptr;
    cairo_t* cr = cairo_create(out);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_transform(cr, &m);
    cairo_set_source_surface(cr, s, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
    cairo_paint(cr);
    cairo_destroy(cr);
    return out;
}

cairo_surface_t* rotateSurface(cairo_surface_t* s, int degrees) {
    int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
    degrees = ((degrees % 360) + 360) % 360;
    cairo_matrix_t m;
    switch(degrees) {
        case 90: cairo_matrix_init(&m, 0, 1, -1, 0, h, 0); return transformed(s, h, w, m);
        case 180: cairo_matrix_init(&m, -1, 0, 0, -1, w, h); return transformed(s, w, h, m);
        case 270: cairo_matrix_init(&m, 0, -1, 1, 0, 0, w); return transformed(s, h, w, m);
        default: return cropSurface(s, 0, 0, w, h);
    }
}

cairo_surface_t* flipSurface(cairo_surface_t* s, bool horizontal) {
    int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
    cairo_matrix_t m;
    if(horizontal)
        cairo_matrix_init(&m, -1, 0, 0, 1, w, 0);
    else
        cairo_matrix_init(&m, 1, 0, 0, -1, 0, h);
    return transformed(s, w, h, m);
}

cairo_surface_t* applyOrientation(cairo_surface_t* s, int o) {
    if(!s) return nullptr;
    switch(o) {
        case 2: return flipSurface(s, true);
        case 3: return rotateSurface(s, 180);
        case 4: return flipSurface(s, false);
        case 5: {  // transpose
            cairo_surface_t* t = rotateSurface(s, 90);
            cairo_surface_t* r = flipSurface(t, true);
            cairo_surface_destroy(t);
            return r;
        }
        case 6: return rotateSurface(s, 90);
        case 7: {  // transverse
            cairo_surface_t* t = rotateSurface(s, 270);
            cairo_surface_t* r = flipSurface(t, true);
            cairo_surface_destroy(t);
            return r;
        }
        case 8: return rotateSurface(s, 270);
        default: return cairo_surface_reference(s);
    }
}

cairo_surface_t* cropSurface(cairo_surface_t* s, int x, int y, int w, int h) {
    cairo_surface_t* out = newSurface(w, h);
    if(!out) return nullptr;
    cairo_t* cr = cairo_create(out);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(cr, s, -x, -y);
    cairo_paint(cr);
    cairo_destroy(cr);
    return out;
}

cairo_surface_t* scaleSurface(cairo_surface_t* s, int w, int h, bool smooth) {
    w = std::max(1, w);
    h = std::max(1, h);
    int sw = cairo_image_surface_get_width(s), sh = cairo_image_surface_get_height(s);
    cairo_surface_t* out = newSurface(w, h);
    if(!out) return nullptr;
    cairo_t* cr = cairo_create(out);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_scale(cr, (double)w / sw, (double)h / sh);
    cairo_set_source_surface(cr, s, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), smooth ? CAIRO_FILTER_GOOD : CAIRO_FILTER_NEAREST);
    cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_PAD);
    cairo_paint(cr);
    cairo_destroy(cr);
    return out;
}

}  // namespace qimgv
