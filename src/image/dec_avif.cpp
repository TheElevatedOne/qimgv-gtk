// AVIF (still images and image sequences) via libavif.
#include <avif/avif.h>

#include <thread>

#include "image/decoder.hpp"

namespace qimgv {

static cairo_surface_t* applyAvifTransform(cairo_surface_t* s, const avifImage* im) {
    cairo_surface_t* cur = cairo_surface_reference(s);
    if(im->transformFlags & AVIF_TRANSFORM_IROT) {
        // irot angle is counter-clockwise in steps of 90 degrees.
        int ccw = im->irot.angle & 3;
        if(ccw) {
            cairo_surface_t* r = rotateSurface(cur, (360 - ccw * 90) % 360);
            cairo_surface_destroy(cur);
            cur = r;
        }
    }
    if(im->transformFlags & AVIF_TRANSFORM_IMIR) {
        // axis 0: mirror about the vertical axis (left <-> right), 1: about the horizontal axis.
        cairo_surface_t* r = flipSurface(cur, im->imir.axis == 0);
        cairo_surface_destroy(cur);
        cur = r;
    }
    return cur;
}

bool decodeAvif(const std::string& path, const DecodeOptions& opt, ImageData& img, std::string& err) {
    avifDecoder* dec = avifDecoderCreate();
    if(!dec) {
        err = "libavif init failed";
        return false;
    }
    dec->maxThreads = (int)std::max(1u, std::thread::hardware_concurrency());
    dec->strictFlags = AVIF_STRICT_DISABLED;
    dec->ignoreExif = AVIF_TRUE;
    dec->ignoreXMP = AVIF_TRUE;
    avifResult r = avifDecoderSetIOFile(dec, path.c_str());
    if(r == AVIF_RESULT_OK) r = avifDecoderParse(dec);
    if(r != AVIF_RESULT_OK) {
        err = avifResultToString(r);
        avifDecoderDestroy(dec);
        return false;
    }
    img.bitDepth = (int)dec->image->depth;
    img.hasAlpha = dec->alphaPresent;
    int count = (opt.firstFrameOnly || !opt.animations) ? 1 : dec->imageCount;
    for(int i = 0; i < count; i++) {
        r = avifDecoderNextImage(dec);
        if(r != AVIF_RESULT_OK) {
            if(img.frames.empty()) err = avifResultToString(r);
            break;
        }
        avifRGBImage rgb;
        avifRGBImageSetDefaults(&rgb, dec->image);
        rgb.format = AVIF_RGB_FORMAT_RGBA;
        rgb.depth = 8;
        rgb.maxThreads = dec->maxThreads;
        if(avifRGBImageAllocatePixels(&rgb) != AVIF_RESULT_OK) {
            err = "Out of memory";
            break;
        }
        r = avifImageYUVToRGB(dec->image, &rgb);
        cairo_surface_t* s = nullptr;
        if(r == AVIF_RESULT_OK) s = surfaceFromRGBA(rgb.pixels, (int)rgb.width, (int)rgb.height, rgb.rowBytes);
        avifRGBImageFreePixels(&rgb);
        if(!s) {
            if(img.frames.empty()) err = r == AVIF_RESULT_OK ? "Image is too large" : avifResultToString(r);
            break;
        }
        cairo_surface_t* t = applyAvifTransform(s, dec->image);
        cairo_surface_destroy(s);
        img.frames.push_back({t, (int)(dec->imageTiming.duration * 1000.0 + 0.5)});
    }
    avifDecoderDestroy(dec);
    if(img.frames.empty()) return false;
    img.format = "AVIF";
    return true;
}

}  // namespace qimgv
