// Camera RAW via LibRaw (thread safe libraw_r). Thumbnails and the optional fast mode use the
// embedded JPEG preview.
#include <libraw/libraw.h>

#include <algorithm>
#include <memory>

#include "image/decoder.hpp"

namespace qimgv {

static int flipToOrientation(int flip) {
    switch(flip) {
        case 3: return 3;
        case 5: return 8;
        case 6: return 6;
        default: return 1;
    }
}

static void quietDataError(void*, const char*, const INT64) {}

static cairo_surface_t* embeddedPreview(LibRaw& raw, int targetSize, std::string& err) {
    if(raw.unpack_thumb() != LIBRAW_SUCCESS) return nullptr;
    int rc = 0;
    libraw_processed_image_t* th = raw.dcraw_make_mem_thumb(&rc);
    if(!th) return nullptr;
    cairo_surface_t* s = nullptr;
    if(th->type == LIBRAW_IMAGE_JPEG)
        s = decodeJpegMemory(th->data, th->data_size, targetSize, err);
    else if(th->type == LIBRAW_IMAGE_BITMAP && th->colors == 3 && th->bits == 8)
        s = surfaceFromRGB(th->data, th->width, th->height, (size_t)th->width * 3);
    LibRaw::dcraw_clear_mem(th);
    if(!s) return nullptr;
    int o = flipToOrientation(raw.imgdata.sizes.flip);
    if(o > 1) {
        cairo_surface_t* r = applyOrientation(s, o);
        cairo_surface_destroy(s);
        s = r;
    }
    return s;
}

bool decodeRaw(const std::string& path, const DecodeOptions& opt, ImageData& img, std::string& err) {
    auto raw = std::make_unique<LibRaw>(LIBRAW_OPTIONS_NONE);
    raw->set_dataerror_handler(quietDataError, nullptr);
    int rc = raw->open_file(path.c_str());
    if(rc != LIBRAW_SUCCESS) {
        err = libraw_strerror(rc);
        return false;
    }
    img.format = "RAW";
    img.bitDepth = raw->imgdata.color.raw_bps ? raw->imgdata.color.raw_bps : 12;

    bool preferPreview = opt.rawUseEmbeddedPreview || opt.targetSize > 0;
    if(preferPreview) {
        cairo_surface_t* s = embeddedPreview(*raw, opt.targetSize, err);
        // Small previews are not good enough for full view.
        if(s && (opt.targetSize > 0 ||
                 std::max(cairo_image_surface_get_width(s), cairo_image_surface_get_height(s)) >= 1000)) {
            img.frames.push_back({s, 0});
            return true;
        }
        if(s) cairo_surface_destroy(s);
    }

    auto& p = raw->imgdata.params;
    p.use_camera_wb = 1;
    p.output_bps = 8;
    p.output_color = 1;  // sRGB
    p.user_flip = -1;    // honour the camera orientation
    if(opt.targetSize > 0) p.half_size = 1;
    if((rc = raw->unpack()) != LIBRAW_SUCCESS || (rc = raw->dcraw_process()) != LIBRAW_SUCCESS) {
        err = libraw_strerror(rc);
        // Fall back to the embedded preview if the raw data itself is unsupported.
        cairo_surface_t* s = embeddedPreview(*raw, 0, err);
        if(!s) return false;
        img.frames.push_back({s, 0});
        return true;
    }
    libraw_processed_image_t* im = raw->dcraw_make_mem_image(&rc);
    if(!im) {
        err = libraw_strerror(rc);
        return false;
    }
    cairo_surface_t* s = nullptr;
    if(im->type == LIBRAW_IMAGE_BITMAP && im->colors == 3 && im->bits == 8)
        s = surfaceFromRGB(im->data, im->width, im->height, (size_t)im->width * 3);
    else if(im->type == LIBRAW_IMAGE_BITMAP && im->colors == 1 && im->bits == 8) {
        std::vector<uint8_t> rgb((size_t)im->width * im->height * 3);
        for(size_t i = 0; i < (size_t)im->width * im->height; i++) rgb[i * 3] = rgb[i * 3 + 1] = rgb[i * 3 + 2] = im->data[i];
        s = surfaceFromRGB(rgb.data(), im->width, im->height, (size_t)im->width * 3);
    }
    LibRaw::dcraw_clear_mem(im);
    if(!s) {
        err = "Unsupported raw output";
        return false;
    }
    img.frames.push_back({s, 0});
    return true;
}

}  // namespace qimgv
