#include "image/encoder.hpp"

#include <avif/avif.h>
#include <jxl/encode.h>
#include <png.h>
#include <turbojpeg.h>
#include <webp/encode.h>

#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

#include "image/formats.hpp"
#include "image/image.hpp"

namespace qimgv {

static bool writeFile(const std::string& path, const void* data, size_t size, std::string& err) {
    FILE* f = fopen(path.c_str(), "wb");
    if(!f) {
        err = std::string("Could not open file for writing: ") + strerror(errno);
        return false;
    }
    bool ok = fwrite(data, 1, size, f) == size;
    ok = (fclose(f) == 0) && ok;
    if(!ok) err = "Write error";
    return ok;
}

static bool saveJpeg(cairo_surface_t* s, const std::string& path, int quality, std::string& err) {
    // Flatten onto white: JPEG has no alpha.
    int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
    cairo_surface_t* flat = cairo_image_surface_create(CAIRO_FORMAT_RGB24, w, h);
    cairo_t* cr = cairo_create(flat);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    cairo_set_source_surface(cr, s, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);
    cairo_surface_flush(flat);
    tjhandle tj = tj3Init(TJINIT_COMPRESS);
    tj3Set(tj, TJPARAM_QUALITY, quality);
    tj3Set(tj, TJPARAM_SUBSAMP, quality >= 90 ? TJSAMP_444 : TJSAMP_420);
    tj3Set(tj, TJPARAM_OPTIMIZE, 1);
    unsigned char* out = nullptr;
    size_t outSize = 0;
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    int pf = TJPF_XRGB;
#else
    int pf = TJPF_BGRX;
#endif
    bool ok = tj3Compress8(tj, cairo_image_surface_get_data(flat), w, cairo_image_surface_get_stride(flat), h, pf,
                           &out, &outSize) == 0;
    if(!ok) err = tj3GetErrorStr(tj);
    if(ok) ok = writeFile(path, out, outSize, err);
    tj3Free(out);
    tj3Destroy(tj);
    cairo_surface_destroy(flat);
    return ok;
}

static bool savePng(cairo_surface_t* s, const std::string& path, std::string& err) {
    int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
    std::vector<uint8_t> rgba = surfaceToRGBA(s);
    png_image image;
    memset(&image, 0, sizeof(image));
    image.version = PNG_IMAGE_VERSION;
    image.width = (png_uint_32)w;
    image.height = (png_uint_32)h;
    image.format = surfaceHasAlpha(s) ? PNG_FORMAT_RGBA : PNG_FORMAT_RGB;
    std::vector<uint8_t> rgb;
    const void* px = rgba.data();
    if(image.format == PNG_FORMAT_RGB) {
        rgb.resize((size_t)w * h * 3);
        for(size_t i = 0; i < (size_t)w * h; i++) memcpy(&rgb[i * 3], &rgba[i * 4], 3);
        px = rgb.data();
    }
    if(!png_image_write_to_file(&image, path.c_str(), 0, px, 0, nullptr)) {
        err = image.message;
        return false;
    }
    return true;
}

static bool saveWebp(cairo_surface_t* s, const std::string& path, int quality, std::string& err) {
    int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
    std::vector<uint8_t> rgba = surfaceToRGBA(s);
    uint8_t* out = nullptr;
    size_t size = quality >= 100 ? WebPEncodeLosslessRGBA(rgba.data(), w, h, w * 4, &out)
                                 : WebPEncodeRGBA(rgba.data(), w, h, w * 4, (float)quality, &out);
    if(!size) {
        err = "WebP encoding failed";
        return false;
    }
    bool ok = writeFile(path, out, size, err);
    WebPFree(out);
    return ok;
}

static bool saveJxl(cairo_surface_t* s, const std::string& path, int quality, std::string& err) {
    int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
    bool alpha = surfaceHasAlpha(s);
    std::vector<uint8_t> rgba = surfaceToRGBA(s);
    JxlEncoder* enc = JxlEncoderCreate(nullptr);
    JxlBasicInfo info;
    JxlEncoderInitBasicInfo(&info);
    info.xsize = (uint32_t)w;
    info.ysize = (uint32_t)h;
    info.bits_per_sample = 8;
    info.num_color_channels = 3;
    info.num_extra_channels = alpha ? 1 : 0;
    info.alpha_bits = alpha ? 8 : 0;
    info.uses_original_profile = quality >= 100 ? JXL_TRUE : JXL_FALSE;
    bool ok = JxlEncoderSetBasicInfo(enc, &info) == JXL_ENC_SUCCESS;
    JxlColorEncoding ce;
    JxlColorEncodingSetToSRGB(&ce, JXL_FALSE);
    ok = ok && JxlEncoderSetColorEncoding(enc, &ce) == JXL_ENC_SUCCESS;
    JxlEncoderFrameSettings* fs = JxlEncoderFrameSettingsCreate(enc, nullptr);
    if(quality >= 100)
        JxlEncoderSetFrameLossless(fs, JXL_TRUE);
    else
        JxlEncoderSetFrameDistance(fs, JxlEncoderDistanceFromQuality((float)quality));
    JxlPixelFormat pf = {4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
    ok = ok && JxlEncoderAddImageFrame(fs, &pf, rgba.data(), rgba.size()) == JXL_ENC_SUCCESS;
    JxlEncoderCloseInput(enc);
    std::vector<uint8_t> out(64 * 1024);
    uint8_t* next = out.data();
    size_t avail = out.size();
    JxlEncoderStatus st = JXL_ENC_NEED_MORE_OUTPUT;
    while(ok && st == JXL_ENC_NEED_MORE_OUTPUT) {
        st = JxlEncoderProcessOutput(enc, &next, &avail);
        if(st == JXL_ENC_NEED_MORE_OUTPUT) {
            size_t used = next - out.data();
            out.resize(out.size() * 2);
            next = out.data() + used;
            avail = out.size() - used;
        }
    }
    ok = ok && st == JXL_ENC_SUCCESS;
    JxlEncoderDestroy(enc);
    if(!ok) {
        err = "JPEG XL encoding failed";
        return false;
    }
    return writeFile(path, out.data(), next - out.data(), err);
}

static bool saveAvif(cairo_surface_t* s, const std::string& path, int quality, std::string& err) {
    int w = cairo_image_surface_get_width(s), h = cairo_image_surface_get_height(s);
    std::vector<uint8_t> rgba = surfaceToRGBA(s);
    avifImage* image = avifImageCreate((uint32_t)w, (uint32_t)h, 8,
                                       quality >= 100 ? AVIF_PIXEL_FORMAT_YUV444 : AVIF_PIXEL_FORMAT_YUV420);
    avifRGBImage rgb;
    avifRGBImageSetDefaults(&rgb, image);
    rgb.format = AVIF_RGB_FORMAT_RGBA;
    rgb.depth = 8;
    rgb.pixels = rgba.data();
    rgb.rowBytes = (uint32_t)w * 4;
    bool ok = avifImageRGBToYUV(image, &rgb) == AVIF_RESULT_OK;
    avifRWData out = AVIF_DATA_EMPTY;
    avifEncoder* enc = avifEncoderCreate();
    enc->quality = quality;
    enc->qualityAlpha = quality;
    enc->speed = 6;
    enc->maxThreads = (int)std::max(1u, std::thread::hardware_concurrency());
    ok = ok && avifEncoderWrite(enc, image, &out) == AVIF_RESULT_OK;
    avifEncoderDestroy(enc);
    avifImageDestroy(image);
    if(!ok) {
        avifRWDataFree(&out);
        err = "AVIF encoding failed";
        return false;
    }
    ok = writeFile(path, out.data, out.size, err);
    avifRWDataFree(&out);
    return ok;
}

bool saveSurface(cairo_surface_t* s, const std::string& path, int quality, std::string& err) {
    if(!s) {
        err = "Nothing to save";
        return false;
    }
    std::string ext = lowerExtension(path);
    if(ext == "jpg" || ext == "jpeg" || ext == "jpe" || ext == "jfif") return saveJpeg(s, path, quality, err);
    if(ext == "png") return savePng(s, path, err);
    if(ext == "webp") return saveWebp(s, path, quality, err);
    if(ext == "jxl") return saveJxl(s, path, quality, err);
    if(ext == "avif") return saveAvif(s, path, quality, err);
    err = "Saving to ." + ext + " is not supported (use jpg, png, webp, jxl or avif)";
    return false;
}

}  // namespace qimgv
