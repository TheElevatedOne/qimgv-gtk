// HEIF / HEIC via libheif. libheif applies the irot/imir/clap transforms itself.
#include <libheif/heif.h>

#include <algorithm>

#include "image/decoder.hpp"

namespace qimgv {

namespace {
struct HeifInit {
    HeifInit() { heif_init(nullptr); }
    ~HeifInit() { heif_deinit(); }
};
}  // namespace

bool decodeHeif(const std::string& path, const DecodeOptions& opt, ImageData& img, std::string& err) {
    static HeifInit init;
    heif_context* ctx = heif_context_alloc();
    heif_error e = heif_context_read_from_file(ctx, path.c_str(), nullptr);
    if(e.code != heif_error_Ok) {
        err = e.message;
        heif_context_free(ctx);
        return false;
    }
    heif_image_handle* handle = nullptr;
    e = heif_context_get_primary_image_handle(ctx, &handle);
    if(e.code != heif_error_Ok) {
        err = e.message;
        heif_context_free(ctx);
        return false;
    }
    // Thumbnails: use the embedded thumbnail when it is big enough.
    if(opt.targetSize > 0 && heif_image_handle_get_number_of_thumbnails(handle) > 0) {
        heif_item_id id;
        heif_image_handle_get_list_of_thumbnail_IDs(handle, &id, 1);
        heif_image_handle* th = nullptr;
        if(heif_image_handle_get_thumbnail(handle, id, &th).code == heif_error_Ok) {
            if(std::max(heif_image_handle_get_width(th), heif_image_handle_get_height(th)) >= opt.targetSize) {
                heif_image_handle_release(handle);
                handle = th;
            } else {
                heif_image_handle_release(th);
            }
        }
    }
    int luma = heif_image_handle_get_luma_bits_per_pixel(handle);
    img.bitDepth = luma > 0 ? luma : 8;
    img.hasAlpha = heif_image_handle_has_alpha_channel(handle);
    bool hdr = luma > 8;
    heif_chroma chroma = hdr ? (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ ? heif_chroma_interleaved_RRGGBBAA_LE
                                                                           : heif_chroma_interleaved_RRGGBBAA_BE)
                             : heif_chroma_interleaved_RGBA;
    heif_image* im = nullptr;
    heif_decoding_options* dopt = heif_decoding_options_alloc();
    e = heif_decode_image(handle, &im, heif_colorspace_RGB, chroma, dopt);
    heif_decoding_options_free(dopt);
    bool ok = false;
    if(e.code == heif_error_Ok) {
        int w = heif_image_get_width(im, heif_channel_interleaved);
        int h = heif_image_get_height(im, heif_channel_interleaved);
        int stride = 0;
        const uint8_t* data = heif_image_get_plane_readonly(im, heif_channel_interleaved, &stride);
        cairo_surface_t* s = nullptr;
        if(data && !hdr) {
            s = surfaceFromRGBA(data, w, h, (size_t)stride);
        } else if(data) {
            int bits = heif_image_get_bits_per_pixel_range(im, heif_channel_interleaved);
            if(bits <= 0) bits = luma;
            int shift = std::max(0, bits - 8);
            std::vector<uint8_t> rgba((size_t)w * h * 4);
            for(int y = 0; y < h; y++) {
                const uint16_t* src = reinterpret_cast<const uint16_t*>(data + (size_t)y * stride);
                uint8_t* d = &rgba[(size_t)y * w * 4];
                for(int x = 0; x < w * 4; x++) d[x] = (uint8_t)std::min(255, src[x] >> shift);
            }
            s = surfaceFromRGBA(rgba.data(), w, h, (size_t)w * 4);
        }
        heif_image_release(im);
        if(s) {
            img.frames.push_back({s, 0});
            ok = true;
        } else {
            err = "Could not convert HEIF image";
        }
    } else {
        err = e.message;
    }
    heif_image_handle_release(handle);
    heif_context_free(ctx);
    if(ok) img.format = "HEIF";
    return ok;
}

}  // namespace qimgv
