// GIF (static and animated) via giflib.
#include <gif_lib.h>

#include <algorithm>
#include <cstring>

#include "image/decoder.hpp"

namespace qimgv {

bool decodeGif(const std::string& path, const DecodeOptions& opt, ImageData& img, std::string& err) {
    int e = 0;
    GifFileType* gif = DGifOpenFileName(path.c_str(), &e);
    if(!gif) {
        err = GifErrorString(e);
        return false;
    }
    if(DGifSlurp(gif) != GIF_OK || gif->ImageCount < 1) {
        err = GifErrorString(gif->Error);
        DGifCloseFile(gif, &e);
        return false;
    }
    int W = gif->SWidth, H = gif->SHeight;
    if(W <= 0 || H <= 0) {
        W = gif->SavedImages[0].ImageDesc.Width;
        H = gif->SavedImages[0].ImageDesc.Height;
    }
    if(W <= 0 || H <= 0 || W > 32767 || H > 32767) {
        err = "Invalid GIF size";
        DGifCloseFile(gif, &e);
        return false;
    }
    std::vector<uint8_t> canvas((size_t)W * H * 4, 0), saved;
    int count = (opt.firstFrameOnly || !opt.animations) ? 1 : gif->ImageCount;
    for(int i = 0; i < count; i++) {
        SavedImage& si = gif->SavedImages[i];
        GraphicsControlBlock gcb;
        gcb.DisposalMode = DISPOSAL_UNSPECIFIED;
        gcb.DelayTime = 0;
        gcb.TransparentColor = NO_TRANSPARENT_COLOR;
        DGifSavedExtensionToGCB(gif, i, &gcb);
        ColorMapObject* cmap = si.ImageDesc.ColorMap ? si.ImageDesc.ColorMap : gif->SColorMap;
        if(!cmap || !si.RasterBits) continue;
        if(gcb.DisposalMode == DISPOSE_PREVIOUS) saved = canvas;
        int fx = si.ImageDesc.Left, fy = si.ImageDesc.Top, fw = si.ImageDesc.Width, fh = si.ImageDesc.Height;
        for(int y = 0; y < fh; y++) {
            int cy = fy + y;
            if(cy < 0 || cy >= H) continue;
            for(int x = 0; x < fw; x++) {
                int cx = fx + x;
                if(cx < 0 || cx >= W) continue;
                int idx = si.RasterBits[(size_t)y * fw + x];
                if(idx == gcb.TransparentColor || idx >= cmap->ColorCount) continue;
                uint8_t* d = &canvas[((size_t)cy * W + cx) * 4];
                d[0] = cmap->Colors[idx].Red;
                d[1] = cmap->Colors[idx].Green;
                d[2] = cmap->Colors[idx].Blue;
                d[3] = 255;
            }
        }
        if(gcb.TransparentColor != NO_TRANSPARENT_COLOR) img.hasAlpha = true;
        cairo_surface_t* s = surfaceFromRGBA(canvas.data(), W, H, (size_t)W * 4);
        if(!s) break;
        img.frames.push_back({s, gcb.DelayTime * 10});
        if(gcb.DisposalMode == DISPOSE_BACKGROUND) {
            for(int y = std::max(0, fy); y < std::min(H, fy + fh); y++)
                for(int x = std::max(0, fx); x < std::min(W, fx + fw); x++)
                    memset(&canvas[((size_t)y * W + x) * 4], 0, 4);
        } else if(gcb.DisposalMode == DISPOSE_PREVIOUS && !saved.empty()) {
            canvas.swap(saved);
        }
    }
    DGifCloseFile(gif, &e);
    if(img.frames.empty()) {
        err = "GIF has no frames";
        return false;
    }
    img.format = "GIF";
    return true;
}

}  // namespace qimgv
