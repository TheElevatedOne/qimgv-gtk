// Camera RAW via LibRaw (thread safe libraw_r, C API, loaded at run time by image/optlibs.cpp).
// Thumbnails and the optional fast mode use the embedded JPEG preview.
// When the installed LibRaw is a different major.minor version than the headers qimgv was built with,
// libraw_data_t's layout may differ, so only LibRaw's functions are used ("basic mode").
#include <algorithm>
#include <vector>

#include "image/decoder.hpp"
#include "image/metadata.hpp"
#include "image/optlibs.hpp"

namespace qimgv {

#ifndef QIMGV_WITH_RAW
bool decodeRaw(const std::string&, const DecodeOptions&, ImageData&, std::string& err) {
    err = "qimgv was built without RAW support";
    return false;
}
#else

static int flipToOrientation(int flip) {
    switch(flip) {
        case 3: return 3;
        case 5: return 8;
        case 6: return 6;
        default: return 1;
    }
}

static void quietDataError(void*, const char*, const INT64) {}

namespace {
struct RawHandle {
    const RawApi* api;
    libraw_data_t* d;
    ~RawHandle() {
        if(d) api->libraw_close(d);
    }
};
}  // namespace

static cairo_surface_t* embeddedPreview(const RawApi* api, libraw_data_t* d, const std::string& path, int targetSize,
                                        std::string& err) {
    if(api->libraw_unpack_thumb(d) != LIBRAW_SUCCESS) return nullptr;
    int rc = 0;
    libraw_processed_image_t* th = api->libraw_dcraw_make_mem_thumb(d, &rc);
    if(!th) return nullptr;
    cairo_surface_t* s = nullptr;
    if(th->type == LIBRAW_IMAGE_JPEG)
        s = decodeJpegMemory(th->data, th->data_size, targetSize, err);
    else if(th->type == LIBRAW_IMAGE_BITMAP && th->colors == 3 && th->bits == 8)
        s = surfaceFromRGB(th->data, th->width, th->height, (size_t)th->width * 3);
    api->libraw_dcraw_clear_mem(th);
    if(!s) return nullptr;
    int o = api->sameAbi ? flipToOrientation(d->sizes.flip) : readOrientation(path);
    if(o > 1) {
        cairo_surface_t* r = applyOrientation(s, o);
        cairo_surface_destroy(s);
        s = r;
    }
    return s;
}

bool decodeRaw(const std::string& path, const DecodeOptions& opt, ImageData& img, std::string& err) {
    const RawApi* api = rawApi();
    if(!api) {
        err = "RAW support needs LibRaw (libraw_r)";
        return false;
    }
    RawHandle raw{api, api->libraw_init(LIBRAW_OPTIONS_NONE)};
    libraw_data_t* d = raw.d;
    if(!d) {
        err = "LibRaw init failed";
        return false;
    }
    api->libraw_set_dataerror_handler(d, quietDataError, nullptr);
    int rc = api->libraw_open_file(d, path.c_str());
    if(rc != LIBRAW_SUCCESS) {
        err = api->libraw_strerror(rc);
        return false;
    }
    img.format = "RAW";
    img.bitDepth = api->sameAbi && d->color.raw_bps ? d->color.raw_bps : 12;

    bool preferPreview = opt.rawUseEmbeddedPreview || opt.targetSize > 0;
    if(preferPreview) {
        cairo_surface_t* s = embeddedPreview(api, d, path, opt.targetSize, err);
        // Small previews are not good enough for full view.
        if(s && (opt.targetSize > 0 ||
                 std::max(cairo_image_surface_get_width(s), cairo_image_surface_get_height(s)) >= 1000)) {
            img.frames.push_back({s, 0});
            return true;
        }
        if(s) cairo_surface_destroy(s);
    }

    api->libraw_set_output_bps(d, 8);
    api->libraw_set_output_color(d, 1);  // sRGB
    if(api->sameAbi) {
        d->params.use_camera_wb = 1;
        d->params.user_flip = -1;  // honour the camera orientation (also LibRaw's default)
        if(opt.targetSize > 0) d->params.half_size = 1;
    }
    if((rc = api->libraw_unpack(d)) != LIBRAW_SUCCESS || (rc = api->libraw_dcraw_process(d)) != LIBRAW_SUCCESS) {
        err = api->libraw_strerror(rc);
        // Fall back to the embedded preview if the raw data itself is unsupported.
        cairo_surface_t* s = embeddedPreview(api, d, path, 0, err);
        if(!s) return false;
        img.frames.push_back({s, 0});
        return true;
    }
    libraw_processed_image_t* im = api->libraw_dcraw_make_mem_image(d, &rc);
    if(!im) {
        err = api->libraw_strerror(rc);
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
    api->libraw_dcraw_clear_mem(im);
    if(!s) {
        err = "Unsupported raw output";
        return false;
    }
    img.frames.push_back({s, 0});
    return true;
}
#endif

}  // namespace qimgv
