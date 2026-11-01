// Decodes every file given on the command line and prints format, size, frame count and timing.
// Exit code is the number of files that failed. Usage: qimgv-decode-test [--thumb N] FILE...
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>

#include "image/decoder.hpp"
#include "image/metadata.hpp"
#include "image/thumbnail.hpp"

int main(int argc, char** argv) {
    qimgv::initMetadata();
    int failed = 0, thumb = 0;
    for(int i = 1; i < argc; i++) {
        if(!strcmp(argv[i], "--thumb") && i + 1 < argc) {
            thumb = atoi(argv[++i]);
            continue;
        }
        auto t0 = std::chrono::steady_clock::now();
        std::string err;
        if(thumb > 0) {
            cairo_surface_t* s = qimgv::generateThumbnail(argv[i], thumb, false, &err);
            double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            if(!s) {
                printf("FAIL  %s: %s\n", argv[i], err.c_str());
                failed++;
                continue;
            }
            printf("OK    %-40s thumb %dx%d  %.1f ms\n", argv[i], cairo_image_surface_get_width(s),
                   cairo_image_surface_get_height(s), ms);
            cairo_surface_destroy(s);
            continue;
        }
        qimgv::DecodeOptions opt;
        auto img = qimgv::decodeFile(argv[i], opt, &err);
        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        if(!img) {
            printf("FAIL  %s: %s\n", argv[i], err.c_str());
            failed++;
            continue;
        }
        cairo_surface_t* s = img->surface(0);
        cairo_surface_flush(s);
        const uint32_t* px = reinterpret_cast<const uint32_t*>(cairo_image_surface_get_data(s));
        uint32_t center = px[(img->height / 2) * (cairo_image_surface_get_stride(s) / 4) + img->width / 2];
        printf("OK    %-40s %-8s %5dx%-5d frames=%zu depth=%d alpha=%d center=#%08x  %.1f ms\n", argv[i],
               img->format.c_str(), img->width, img->height, img->frames.size(), img->bitDepth, img->hasAlpha,
               center, ms);
    }
    return failed;
}
