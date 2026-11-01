// JPEG via libjpeg-turbo. Decodes straight into cairo's BGRA layout and supports
// DCT scaling for fast thumbnails.
#include <turbojpeg.h>

#include <algorithm>

#include "image/decoder.hpp"
#include "image/metadata.hpp"

namespace qimgv {

cairo_surface_t* decodeJpegMemory(const unsigned char* data, size_t size, int targetSize, std::string& err) {
    tjhandle tj = tj3Init(TJINIT_DECOMPRESS);
    if(!tj) {
        err = "libjpeg-turbo init failed";
        return nullptr;
    }
    cairo_surface_t* result = nullptr;
    if(tj3DecompressHeader(tj, data, size) != 0) {
        err = tj3GetErrorStr(tj);
        tj3Destroy(tj);
        return nullptr;
    }
    int w = tj3Get(tj, TJPARAM_JPEGWIDTH), h = tj3Get(tj, TJPARAM_JPEGHEIGHT);
    tj3Set(tj, TJPARAM_STOPONWARNING, 0);
    if(targetSize > 0) {
        int n = 0;
        tjscalingfactor* sf = tj3GetScalingFactors(&n);
        tjscalingfactor best = {1, 1};
        for(int i = 0; i < n; i++) {
            int sw = TJSCALED(w, sf[i]), sh = TJSCALED(h, sf[i]);
            if(std::max(sw, sh) >= targetSize && (double)sf[i].num / sf[i].denom < (double)best.num / best.denom &&
               sf[i].num <= sf[i].denom)
                best = sf[i];
        }
        tj3SetScalingFactor(tj, best);
        w = TJSCALED(w, best);
        h = TJSCALED(h, best);
    }
    cairo_surface_t* s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    if(cairo_surface_status(s) != CAIRO_STATUS_SUCCESS) {
        err = "Image is too large";
        cairo_surface_destroy(s);
        tj3Destroy(tj);
        return nullptr;
    }
    cairo_surface_flush(s);
    // cairo ARGB32 on little endian is B,G,R,A in memory; TJPF_BGRA fills alpha with 0xFF.
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    int pf = TJPF_ARGB;
#else
    int pf = TJPF_BGRA;
#endif
    if(tj3Decompress8(tj, data, size, cairo_image_surface_get_data(s), cairo_image_surface_get_stride(s), pf) != 0 &&
       tj3GetErrorCode(tj) == TJERR_FATAL) {
        err = tj3GetErrorStr(tj);
        cairo_surface_destroy(s);
    } else {
        cairo_surface_mark_dirty(s);
        result = s;
    }
    tj3Destroy(tj);
    return result;
}

bool decodeJpeg(const std::string& path, const DecodeOptions& opt, ImageData& img, std::string& err) {
    std::string data;
    if(!readWholeFile(path, data)) {
        err = "Could not read file";
        return false;
    }
    cairo_surface_t* s = decodeJpegMemory(reinterpret_cast<const unsigned char*>(data.data()), data.size(),
                                          opt.targetSize, err);
    if(!s) return false;
    int o = readOrientation(path);
    if(o > 1) {
        cairo_surface_t* r = applyOrientation(s, o);
        cairo_surface_destroy(s);
        s = r;
    }
    img.format = "JPEG";
    img.frames.push_back({s, 0});
    return true;
}

}  // namespace qimgv
