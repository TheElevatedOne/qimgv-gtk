// WebP (static and animated) via libwebp's animation decoder.
#include <webp/decode.h>
#include <webp/demux.h>

#include "image/decoder.hpp"
#include "image/metadata.hpp"

namespace qimgv {

bool decodeWebp(const std::string& path, const DecodeOptions& opt, ImageData& img, std::string& err) {
    std::string file;
    if(!readWholeFile(path, file)) {
        err = "Could not read file";
        return false;
    }
    WebPData data = {reinterpret_cast<const uint8_t*>(file.data()), file.size()};
    WebPAnimDecoderOptions o;
    WebPAnimDecoderOptionsInit(&o);
    o.color_mode = MODE_RGBA;
    o.use_threads = 1;
    WebPAnimDecoder* dec = WebPAnimDecoderNew(&data, &o);
    if(!dec) {
        err = "Invalid WebP file";
        return false;
    }
    WebPAnimInfo info;
    WebPAnimDecoderGetInfo(dec, &info);
    int prevTs = 0;
    while(WebPAnimDecoderHasMoreFrames(dec)) {
        uint8_t* buf = nullptr;
        int ts = 0;
        if(!WebPAnimDecoderGetNext(dec, &buf, &ts)) break;
        cairo_surface_t* s = surfaceFromRGBA(buf, (int)info.canvas_width, (int)info.canvas_height,
                                             (size_t)info.canvas_width * 4);
        if(!s) break;
        img.frames.push_back({s, ts - prevTs});
        prevTs = ts;
        if(opt.firstFrameOnly || !opt.animations) break;
    }
    WebPAnimDecoderDelete(dec);
    if(img.frames.empty()) {
        err = "Could not decode WebP";
        return false;
    }
    WebPBitstreamFeatures feat;
    if(WebPGetFeatures(data.bytes, data.size, &feat) == VP8_STATUS_OK) img.hasAlpha = feat.has_alpha;
    img.format = "WebP";
    if(img.frames.size() == 1) {
        int ori = readOrientation(path);
        if(ori > 1) {
            cairo_surface_t* r = applyOrientation(img.frames[0].surface, ori);
            cairo_surface_destroy(img.frames[0].surface);
            img.frames[0].surface = r;
        }
    }
    return true;
}

}  // namespace qimgv
