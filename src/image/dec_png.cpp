// PNG and APNG. libpng (without the APNG patch) decodes each frame: the APNG chunks are
// reassembled into a standalone PNG per frame and composited here.
#include <png.h>
#include <zlib.h>

#include <algorithm>
#include <cstring>

#include "image/decoder.hpp"
#include "image/metadata.hpp"

namespace qimgv {

namespace {

uint32_t be32(const unsigned char* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
uint16_t be16(const unsigned char* p) { return (uint16_t)(p[0] << 8 | p[1]); }
void put32(std::string& s, uint32_t v) {
    s.push_back((char)(v >> 24));
    s.push_back((char)(v >> 16));
    s.push_back((char)(v >> 8));
    s.push_back((char)v);
}
void putChunk(std::string& out, const char* type, const unsigned char* data, size_t len) {
    put32(out, (uint32_t)len);
    size_t start = out.size();
    out.append(type, 4);
    if(len) out.append(reinterpret_cast<const char*>(data), len);
    uint32_t crc = crc32(0, reinterpret_cast<const Bytef*>(out.data() + start), (uInt)(len + 4));
    put32(out, crc);
}

// Decode a complete PNG in memory to straight RGBA8. Uses the full libpng API (the simplified
// API treats 16-bit files as linear light, which brightens most 16-bit PNGs).
struct MemReader {
    const unsigned char* data;
    size_t size, pos;
};
void readMem(png_structp png, png_bytep out, png_size_t len) {
    auto* r = static_cast<MemReader*>(png_get_io_ptr(png));
    if(r->pos + len > r->size) png_error(png, "unexpected end of data");
    memcpy(out, r->data + r->pos, len);
    r->pos += len;
}
void pngError(png_structp png, png_const_charp msg) {
    auto* err = static_cast<std::string*>(png_get_error_ptr(png));
    if(err) *err = msg;
    png_longjmp(png, 1);
}
void pngWarning(png_structp, png_const_charp) {}

bool decodePngMemory(const void* data, size_t size, std::vector<uint8_t>& rgba, int& w, int& h, std::string& err) {
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, &err, pngError, pngWarning);
    if(!png) return false;
    png_infop info = png_create_info_struct(png);
    MemReader reader{static_cast<const unsigned char*>(data), size, 0};
    std::vector<png_bytep> rows;
    if(setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        if(err.empty()) err = "Invalid PNG";
        return false;
    }
    png_set_read_fn(png, &reader, readMem);
    png_set_crc_action(png, PNG_CRC_QUIET_USE, PNG_CRC_QUIET_USE);
    png_read_info(png, info);
    png_uint_32 pw = png_get_image_width(png, info), ph = png_get_image_height(png, info);
    int depth = png_get_bit_depth(png, info), type = png_get_color_type(png, info);
    if(pw > 32767 || ph > 32767) png_error(png, "Image is too large");
    if(type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if(type == PNG_COLOR_TYPE_GRAY && depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if(png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if(depth == 16) png_set_scale_16(png);
    if(type == PNG_COLOR_TYPE_GRAY || type == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png);
    png_set_filler(png, 0xff, PNG_FILLER_AFTER);
    png_set_interlace_handling(png);
    png_read_update_info(png, info);
    w = (int)pw;
    h = (int)ph;
    rgba.resize((size_t)w * h * 4);
    rows.resize(h);
    for(int y = 0; y < h; y++) rows[y] = &rgba[(size_t)y * w * 4];
    png_read_image(png, rows.data());
    png_read_end(png, nullptr);
    png_destroy_read_struct(&png, &info, nullptr);
    return true;
}

struct ApngFrame {
    uint32_t w = 0, h = 0, x = 0, y = 0;
    uint16_t delayNum = 0, delayDen = 100;
    uint8_t dispose = 0, blend = 0;
    std::string data;  // concatenated zlib stream (IDAT / fdAT payloads)
};

}  // namespace

bool decodePng(const std::string& path, const DecodeOptions& opt, ImageData& img, std::string& err) {
    std::string file;
    if(!readWholeFile(path, file)) {
        err = "Could not read file";
        return false;
    }
    const auto* p = reinterpret_cast<const unsigned char*>(file.data());
    size_t n = file.size();
    if(n < 8 || memcmp(p, "\x89PNG\r\n\x1a\n", 8) != 0) {
        err = "Not a PNG file";
        return false;
    }

    // Walk the chunks.
    std::string ihdr;
    std::string shared;  // chunks every frame needs (PLTE, tRNS, gAMA, iCCP, ...)
    std::vector<ApngFrame> frames;
    bool actl = false, seenIdat = false;
    int bitDepth = 8, colorType = 6;
    for(size_t off = 8; off + 12 <= n;) {
        uint32_t len = be32(p + off);
        if(off + 12 + (size_t)len > n) break;
        const char* type = reinterpret_cast<const char*>(p + off + 4);
        const unsigned char* d = p + off + 8;
        if(!memcmp(type, "IHDR", 4) && len >= 13) {
            ihdr.assign(reinterpret_cast<const char*>(d), len);
            bitDepth = d[8];
            colorType = d[9];
        } else if(!memcmp(type, "acTL", 4)) {
            actl = true;
        } else if(!memcmp(type, "fcTL", 4) && len >= 26) {
            ApngFrame f;
            f.w = be32(d + 4);
            f.h = be32(d + 8);
            f.x = be32(d + 12);
            f.y = be32(d + 16);
            f.delayNum = be16(d + 20);
            f.delayDen = be16(d + 22);
            f.dispose = d[24];
            f.blend = d[25];
            frames.push_back(std::move(f));
        } else if(!memcmp(type, "IDAT", 4)) {
            seenIdat = true;
            if(!frames.empty()) frames.back().data.append(reinterpret_cast<const char*>(d), len);
        } else if(!memcmp(type, "fdAT", 4) && len >= 4) {
            if(!frames.empty()) frames.back().data.append(reinterpret_cast<const char*>(d + 4), len - 4);
        } else if(!memcmp(type, "IEND", 4)) {
            break;
        } else if(!seenIdat && frames.empty()) {
            putChunk(shared, std::string(type, 4).c_str(), d, len);
        }
        off += 12 + len;
    }

    img.bitDepth = bitDepth;
    img.hasAlpha = (colorType & 4) != 0 || shared.find("tRNS") != std::string::npos;

    bool animate = actl && frames.size() > 1 && ihdr.size() == 13 && opt.animations && !opt.firstFrameOnly;
    if(!animate) {
        std::vector<uint8_t> rgba;
        int w, h;
        if(!decodePngMemory(p, n, rgba, w, h, err)) return false;
        cairo_surface_t* s = surfaceFromRGBA(rgba.data(), w, h, (size_t)w * 4);
        if(!s) {
            err = "Image is too large";
            return false;
        }
        img.format = actl ? "APNG" : "PNG";
        img.frames.push_back({s, 0});
        return true;
    }

    uint32_t W = be32(reinterpret_cast<const unsigned char*>(ihdr.data()));
    uint32_t H = be32(reinterpret_cast<const unsigned char*>(ihdr.data()) + 4);
    if(W == 0 || H == 0 || W > 32767 || H > 32767) {
        err = "Invalid APNG size";
        return false;
    }
    std::vector<uint8_t> canvas((size_t)W * H * 4, 0), saved;
    for(size_t i = 0; i < frames.size(); i++) {
        ApngFrame& f = frames[i];
        if(f.data.empty() || f.w == 0 || f.h == 0 || f.x + f.w > W || f.y + f.h > H) continue;
        // Standalone PNG for this frame.
        std::string png("\x89PNG\r\n\x1a\n", 8);
        std::string hdr = ihdr;
        unsigned char* hp = reinterpret_cast<unsigned char*>(&hdr[0]);
        for(int k = 0; k < 4; k++) {
            hp[k] = (unsigned char)(f.w >> (24 - 8 * k));
            hp[4 + k] = (unsigned char)(f.h >> (24 - 8 * k));
        }
        putChunk(png, "IHDR", hp, hdr.size());
        png += shared;
        putChunk(png, "IDAT", reinterpret_cast<const unsigned char*>(f.data.data()), f.data.size());
        putChunk(png, "IEND", nullptr, 0);
        std::vector<uint8_t> rgba;
        int fw, fh;
        std::string ferr;
        if(!decodePngMemory(png.data(), png.size(), rgba, fw, fh, ferr)) continue;

        uint8_t dispose = f.dispose;
        if(i == 0 && dispose == 2) dispose = 1;
        if(dispose == 2) saved = canvas;
        if(i == 0) f.blend = 0;

        for(uint32_t y = 0; y < f.h; y++) {
            uint8_t* dst = &canvas[((size_t)(f.y + y) * W + f.x) * 4];
            const uint8_t* src = &rgba[(size_t)y * f.w * 4];
            if(f.blend == 0) {
                memcpy(dst, src, (size_t)f.w * 4);
                continue;
            }
            for(uint32_t x = 0; x < f.w; x++, dst += 4, src += 4) {
                unsigned sa = src[3];
                if(sa == 255) {
                    memcpy(dst, src, 4);
                } else if(sa != 0) {
                    unsigned da = dst[3];
                    unsigned outA = sa * 255 + da * (255 - sa);  // scaled by 255
                    for(int c = 0; c < 3; c++)
                        dst[c] = (uint8_t)((src[c] * sa * 255 + dst[c] * da * (255 - sa)) / outA);
                    dst[3] = (uint8_t)(outA / 255);
                }
            }
        }
        cairo_surface_t* s = surfaceFromRGBA(canvas.data(), (int)W, (int)H, (size_t)W * 4);
        if(!s) break;
        int den = f.delayDen ? f.delayDen : 100;
        img.frames.push_back({s, (int)(1000.0 * f.delayNum / den + 0.5)});

        if(dispose == 1) {
            for(uint32_t y = 0; y < f.h; y++)
                memset(&canvas[((size_t)(f.y + y) * W + f.x) * 4], 0, (size_t)f.w * 4);
        } else if(dispose == 2) {
            canvas.swap(saved);
        }
    }
    if(img.frames.empty()) {
        err = "Could not decode APNG frames";
        return false;
    }
    img.format = "APNG";
    img.hasAlpha = true;
    return true;
}

}  // namespace qimgv
