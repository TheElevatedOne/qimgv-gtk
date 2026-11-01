// JPEG XL via libjxl. Output is converted to sRGB by the decoder; animations are coalesced.
#include <jxl/decode.h>
#include <jxl/encode.h>
#include <jxl/resizable_parallel_runner.h>

#include "image/decoder.hpp"

namespace qimgv {

bool decodeJxl(const std::string& path, const DecodeOptions& opt, ImageData& img, std::string& err) {
    std::string file;
    if(!readWholeFile(path, file)) {
        err = "Could not read file";
        return false;
    }
    JxlDecoder* dec = JxlDecoderCreate(nullptr);
    void* runner = JxlResizableParallelRunnerCreate(nullptr);
    auto cleanup = [&] {
        JxlResizableParallelRunnerDestroy(runner);
        JxlDecoderDestroy(dec);
    };
    if(!dec || !runner || JxlDecoderSetParallelRunner(dec, JxlResizableParallelRunner, runner) != JXL_DEC_SUCCESS ||
       JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING | JXL_DEC_FRAME |
                                          JXL_DEC_FULL_IMAGE) != JXL_DEC_SUCCESS) {
        err = "libjxl init failed";
        cleanup();
        return false;
    }
    JxlDecoderSetInput(dec, reinterpret_cast<const uint8_t*>(file.data()), file.size());
    JxlDecoderCloseInput(dec);

    JxlBasicInfo info{};
    JxlPixelFormat fmt = {4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
    std::vector<uint8_t> buffer;
    int frameDelay = 0;
    bool ok = false;
    for(;;) {
        JxlDecoderStatus st = JxlDecoderProcessInput(dec);
        if(st == JXL_DEC_ERROR) {
            err = "Invalid JPEG XL file";
            break;
        } else if(st == JXL_DEC_NEED_MORE_INPUT) {
            err = "Truncated JPEG XL file";
            ok = !img.frames.empty();
            break;
        } else if(st == JXL_DEC_BASIC_INFO) {
            if(JxlDecoderGetBasicInfo(dec, &info) != JXL_DEC_SUCCESS) {
                err = "Invalid JPEG XL header";
                break;
            }
            img.bitDepth = (int)info.bits_per_sample;
            img.hasAlpha = info.alpha_bits > 0;
            JxlResizableParallelRunnerSetThreads(
                runner, JxlResizableParallelRunnerSuggestThreads(info.xsize, info.ysize));
        } else if(st == JXL_DEC_COLOR_ENCODING) {
            JxlColorEncoding srgb;
            JxlColorEncodingSetToSRGB(&srgb, JXL_FALSE);
            JxlDecoderSetPreferredColorProfile(dec, &srgb);
        } else if(st == JXL_DEC_FRAME) {
            JxlFrameHeader fh;
            frameDelay = 0;
            if(JxlDecoderGetFrameHeader(dec, &fh) == JXL_DEC_SUCCESS && info.have_animation &&
               info.animation.tps_numerator)
                frameDelay = (int)(1000.0 * fh.duration * info.animation.tps_denominator /
                                   info.animation.tps_numerator + 0.5);
        } else if(st == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
            size_t size = 0;
            if(JxlDecoderImageOutBufferSize(dec, &fmt, &size) != JXL_DEC_SUCCESS) {
                err = "libjxl buffer size error";
                break;
            }
            buffer.resize(size);
            if(JxlDecoderSetImageOutBuffer(dec, &fmt, buffer.data(), size) != JXL_DEC_SUCCESS) {
                err = "libjxl buffer error";
                break;
            }
        } else if(st == JXL_DEC_FULL_IMAGE) {
            cairo_surface_t* s = surfaceFromRGBA(buffer.data(), (int)info.xsize, (int)info.ysize, (size_t)info.xsize * 4);
            if(!s) {
                err = "Image is too large";
                break;
            }
            img.frames.push_back({s, frameDelay});
            if(opt.firstFrameOnly || !opt.animations || !info.have_animation) {
                ok = true;
                break;
            }
        } else if(st == JXL_DEC_SUCCESS) {
            ok = true;
            break;
        }
    }
    cleanup();
    if(!ok || img.frames.empty()) return false;
    img.format = "JPEG XL";
    return true;
}

}  // namespace qimgv
