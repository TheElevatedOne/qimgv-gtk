#include "image/decoder.hpp"

#include <cstdio>
#include <exception>

#include "image/metadata.hpp"

namespace qimgv {

bool readWholeFile(const std::string& path, std::string& out) {
    FILE* f = fopen(path.c_str(), "rb");
    if(!f) return false;
    if(fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }
    long size = ftell(f);
    if(size < 0) {
        fclose(f);
        return false;
    }
    rewind(f);
    out.resize((size_t)size);
    size_t n = size ? fread(&out[0], 1, (size_t)size, f) : 0;
    fclose(f);
    return n == (size_t)size;
}

ImagePtr decodeFile(const std::string& path, const DecodeOptions& opt, std::string* error) {
    auto img = std::make_shared<ImageData>();
    img->path = path;
    std::string err;
    Format fmt = detectFormat(path);
    bool ok = false;
    try {
        switch(fmt) {
            case Format::JPEG: ok = decodeJpeg(path, opt, *img, err); break;
            case Format::PNG: ok = decodePng(path, opt, *img, err); break;
            case Format::GIF: ok = decodeGif(path, opt, *img, err); break;
            case Format::WebP: ok = decodeWebp(path, opt, *img, err); break;
            case Format::JXL: ok = decodeJxl(path, opt, *img, err); break;
            case Format::AVIF: ok = decodeAvif(path, opt, *img, err); break;
            case Format::HEIF: ok = decodeHeif(path, opt, *img, err); break;
            case Format::Raw: ok = decodeRaw(path, opt, *img, err); break;
            case Format::Pixbuf: ok = decodePixbuf(path, opt, *img, err); break;
            case Format::Unknown: err = "Unsupported file type"; break;
        }
        // Some files have the wrong extension or use features our native decoders reject: let gdk-pixbuf try.
        if(!ok && fmt != Format::Pixbuf && fmt != Format::Unknown && fmt != Format::Raw) {
            std::string err2;
            img->frames.clear();
            if(decodePixbuf(path, opt, *img, err2)) ok = true;
        }
    } catch(const std::exception& e) {
        err = e.what();
        ok = false;
    }
    if(!ok || img->frames.empty() || !img->frames[0].surface) {
        if(error) *error = err.empty() ? "Could not decode image" : err;
        return nullptr;
    }
    img->width = cairo_image_surface_get_width(img->frames[0].surface);
    img->height = cairo_image_surface_get_height(img->frames[0].surface);
    for(auto& f : img->frames)
        if(f.delayMs <= 10) f.delayMs = 100;  // same clamping browsers use
    return img;
}

}  // namespace qimgv
