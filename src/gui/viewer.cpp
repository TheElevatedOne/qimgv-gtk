#include "gui/viewer.hpp"

#include <algorithm>
#include <cmath>

#include "actions.hpp"
#include "gui/icons.hpp"
#include "gui/style.hpp"

namespace qimgv {

static constexpr double kMinScale = 0.01, kMaxScale = 60.0;

Viewer::Viewer() : mHqPool(std::make_unique<ThreadPool>(1)), mHqGen(std::make_shared<int>(0)) {
    mArea = gtk_drawing_area_new();
    addClass(mArea, "qimgv-viewer");
    gtk_widget_set_hexpand(mArea, TRUE);
    gtk_widget_set_vexpand(mArea, TRUE);
    gtk_widget_add_events(mArea, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK |
                                     GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK | GDK_LEAVE_NOTIFY_MASK);
    g_signal_connect(mArea, "draw", G_CALLBACK(drawCb), this);
    g_signal_connect(mArea, "button-press-event", G_CALLBACK(pressCb), this);
    g_signal_connect(mArea, "button-release-event", G_CALLBACK(releaseCb), this);
    g_signal_connect(mArea, "motion-notify-event", G_CALLBACK(motionCb), this);
    g_signal_connect(mArea, "scroll-event", G_CALLBACK(scrollCb), this);
    g_signal_connect(mArea, "leave-notify-event", G_CALLBACK(leaveCb), this);
    g_signal_connect(mArea, "size-allocate", G_CALLBACK(sizeCb), this);
    mFitMode = settings().fitMode;
    mKeepZoom = settings().keepZoom;
}

Viewer::~Viewer() {
    ++*mHqGen;
    mHqPool->stop();
    stopAnimation();
    dropHq();
    for(guint* t : {&mHqTimer, &mLoadingTimer, &mMapTimer, &mCursorTimer})
        if(*t) g_source_remove(*t);
}

// ---- geometry -------------------------------------------------------------

int Viewer::dpr() const { return std::max(1, gtk_widget_get_scale_factor(mArea)); }
double Viewer::viewW() const { return std::max(1, gtk_widget_get_allocated_width(mArea)); }
double Viewer::viewH() const { return std::max(1, gtk_widget_get_allocated_height(mArea)); }
double Viewer::dispW() const { return mImage ? mImage->width * mScale / dpr() : 0; }
double Viewer::dispH() const { return mImage ? mImage->height * mScale / dpr() : 0; }

bool Viewer::scrollable() const { return mImage && (dispW() > viewW() + 0.5 || dispH() > viewH() + 0.5); }

double Viewer::fitScale(FitMode m) const {
    if(!mImage || mImage->width <= 0 || mImage->height <= 0) return 1.0;
    double W = viewW() * dpr(), H = viewH() * dpr();
    double w = mImage->width, h = mImage->height;
    auto limitUpscale = [&](double s) {
        if(s <= 1.0) return s;
        if(!settings().expandImage) return 1.0;
        return settings().expandLimit > 0 ? std::min(s, (double)settings().expandLimit) : s;
    };
    switch(m) {
        case FitMode::Window: return limitUpscale(std::min(W / w, H / h));
        case FitMode::Width: return limitUpscale(W / w);
        case FitMode::Normal: return 1.0;
        case FitMode::Stretch: return std::min(W / w, H / h);
        default: return mScale;
    }
}

void Viewer::clampOffset() {
    double W = viewW(), H = viewH(), dw = dispW(), dh = dispH();
    mOx = dw <= W ? (W - dw) / 2 : std::clamp(mOx, W - dw, 0.0);
    mOy = dh <= H ? (H - dh) / 2 : std::clamp(mOy, H - dh, 0.0);
}

void Viewer::applyFit(bool resetPosition) {
    if(!mImage) return;
    if(mFitMode != FitMode::Free) mScale = fitScale(mFitMode);
    if(resetPosition) {
        double W = viewW(), dw = dispW();
        mOx = (W - dw) / 2;
        // Tall images start at the top in width / 1:1 mode.
        if(mFitMode == FitMode::Width || (mFitMode == FitMode::Normal && settings().focusPoint == FocusPoint::Top))
            mOy = 0;
        else
            mOy = (viewH() - dispH()) / 2;
    }
    clampOffset();
    scaleChanged();
}

void Viewer::setScaleAt(double s, double ax, double ay) {
    if(!mImage) return;
    s = std::clamp(s, kMinScale, kMaxScale);
    double k = mScale / dpr(), nk = s / dpr();
    double ix = (ax - mOx) / k, iy = (ay - mOy) / k;
    mScale = s;
    mOx = ax - ix * nk;
    mOy = ay - iy * nk;
    clampOffset();
    scaleChanged();
}

void Viewer::scaleChanged() {
    if(mHq && std::abs(mHqScale - mScale) > 1e-9) dropHq();
    requestHq();
    if(onScaleChanged) onScaleChanged();
    gtk_widget_queue_draw(mArea);
}

void Viewer::imageToWidget(double ix, double iy, double& wx, double& wy) const {
    double k = mScale / dpr();
    wx = mOx + ix * k;
    wy = mOy + iy * k;
}

void Viewer::widgetToImage(double wx, double wy, double& ix, double& iy) const {
    double k = mScale / dpr();
    ix = (wx - mOx) / k;
    iy = (wy - mOy) / k;
}

// ---- public API -----------------------------------------------------------

void Viewer::setImage(ImagePtr img) {
    showLoading(false);
    mError.clear();
    stopAnimation();
    dropHq();
    bool keep = mKeepZoom && mImage && img && mFitMode == FitMode::Free;
    double keepScale = mScale;
    mImage = std::move(img);
    mFrame = 0;
    if(mCropping) endCrop();
    if(!mImage) {
        gtk_widget_queue_draw(mArea);
        return;
    }
    if(keep) {
        mScale = keepScale;
        applyFit(true);
    } else {
        if(mFitMode == FitMode::Free) mFitMode = settings().fitMode;
        applyFit(true);
    }
    startAnimation();
}

void Viewer::clear() {
    showLoading(false);
    mError.clear();
    stopAnimation();
    dropHq();
    mImage.reset();
    if(mCropping) endCrop();
    gtk_widget_queue_draw(mArea);
}

void Viewer::showLoading(bool on) {
    if(mLoadingTimer) {
        g_source_remove(mLoadingTimer);
        mLoadingTimer = 0;
    }
    mLoading = on;
    if(!on) {
        if(mLoadingVisible) gtk_widget_queue_draw(mArea);
        mLoadingVisible = false;
        return;
    }
    // Only show the indicator for slow decodes; keep the previous image until then.
    mLoadingTimer = g_timeout_add(
        250,
        [](gpointer p) -> gboolean {
            auto* v = static_cast<Viewer*>(p);
            v->mLoadingTimer = 0;
            v->mLoadingVisible = true;
            v->stopAnimation();
            v->dropHq();
            v->mImage.reset();
            gtk_widget_queue_draw(v->mArea);
            return G_SOURCE_REMOVE;
        },
        this);
}

void Viewer::showError(const std::string& msg) {
    clear();
    mError = msg;
    mHint = false;
    gtk_widget_queue_draw(mArea);
}

void Viewer::showHint(const std::string& msg) {
    clear();
    mError = msg;
    mHint = true;
    gtk_widget_queue_draw(mArea);
}

void Viewer::imageEdited() {
    dropHq();
    stopAnimation();
    mFrame = 0;
    if(mFitMode == FitMode::Free) mFitMode = settings().fitMode;
    applyFit(true);
    startAnimation();
}

void Viewer::settingsChanged() {
    mKeepZoom = settings().keepZoom;
    dropHq();
    applyFit(false);
    gtk_widget_queue_draw(mArea);
}

void Viewer::setFitMode(FitMode m) {
    if(!mImage) {
        mFitMode = m;
        return;
    }
    if(m == FitMode::Normal && settings().focusPoint == FocusPoint::Cursor && mCursorX >= 0) {
        // Zoom to 1:1 around the point under the cursor.
        double ax = mCursorX, ay = mCursorY;
        mFitMode = FitMode::Normal;
        setScaleAt(1.0, ax, ay);
        return;
    }
    mFitMode = m;
    applyFit(true);
}

void Viewer::zoomIn(bool atCursor) {
    if(!mImage) return;
    double ax = viewW() / 2, ay = viewH() / 2;
    if(atCursor && mCursorX >= 0) {
        ax = mCursorX;
        ay = mCursorY;
    }
    mFitMode = FitMode::Free;
    setScaleAt(mScale * (1.0 + settings().zoomStep), ax, ay);
    bumpMap();
}

void Viewer::zoomOut(bool atCursor) {
    if(!mImage) return;
    double ax = viewW() / 2, ay = viewH() / 2;
    if(atCursor && mCursorX >= 0) {
        ax = mCursorX;
        ay = mCursorY;
    }
    mFitMode = FitMode::Free;
    setScaleAt(mScale / (1.0 + settings().zoomStep), ax, ay);
    bumpMap();
}

void Viewer::scrollBy(double dx, double dy) {
    if(!mImage) return;
    mOx -= dx;
    mOy -= dy;
    clampOffset();
    bumpMap();
    gtk_widget_queue_draw(mArea);
}

// ---- animation ------------------------------------------------------------

void Viewer::startAnimation() {
    stopAnimation();
    if(!mImage || !mImage->animated()) return;
    mAnimTimer = g_timeout_add(
        std::max(20, mImage->frames[mFrame].delayMs),
        [](gpointer p) -> gboolean {
            auto* v = static_cast<Viewer*>(p);
            v->mAnimTimer = 0;
            if(!v->mImage || !v->mImage->animated()) return G_SOURCE_REMOVE;
            v->mFrame = (v->mFrame + 1) % v->mImage->frames.size();
            gtk_widget_queue_draw(v->mArea);
            v->startAnimation();
            return G_SOURCE_REMOVE;
        },
        this);
}

void Viewer::stopAnimation() {
    if(mAnimTimer) {
        g_source_remove(mAnimTimer);
        mAnimTimer = 0;
    }
}

// ---- high quality scaling ---------------------------------------------------

void Viewer::dropHq() {
    ++*mHqGen;
    if(mHq) {
        cairo_surface_destroy(mHq);
        mHq = nullptr;
    }
    mHqScale = 0;
}

void Viewer::requestHq() {
    if(mHqTimer) {
        g_source_remove(mHqTimer);
        mHqTimer = 0;
    }
    if(!mImage || mImage->animated() || settings().scalingFilter != ScalingFilter::Quality || mScale >= 1.0 ||
       (mHq && std::abs(mHqScale - mScale) < 1e-9))
        return;
    mHqTimer = g_timeout_add(
        40,
        [](gpointer p) -> gboolean {
            auto* v = static_cast<Viewer*>(p);
            v->mHqTimer = 0;
            if(!v->mImage) return G_SOURCE_REMOVE;
            cairo_surface_t* src = cairo_surface_reference(v->mImage->surface(0));
            double scale = v->mScale;
            int d = v->dpr();
            int tw = std::max(1, (int)std::lround(v->mImage->width * scale));
            int th = std::max(1, (int)std::lround(v->mImage->height * scale));
            int gen = ++*v->mHqGen;
            std::weak_ptr<int> wgen = v->mHqGen;
            unsigned char* data = cairo_image_surface_get_data(src);
            int sw = cairo_image_surface_get_width(src), sh = cairo_image_surface_get_height(src);
            int stride = cairo_image_surface_get_stride(src);
            cairo_surface_flush(src);
            v->mHqPool->clear();
            v->mHqPool->submit([v, src, data, sw, sh, stride, tw, th, scale, d, gen, wgen] {
                {
                    auto g = wgen.lock();
                    if(!g || *g != gen) {
                        runOnMain([src] { cairo_surface_destroy(src); });
                        return;
                    }
                }
                // Wrap the pixels in a private surface object: cairo surfaces are not thread safe,
                // the pixel memory is (read only). `src` keeps it alive.
                cairo_surface_t* wrap = cairo_image_surface_create_for_data(data, CAIRO_FORMAT_ARGB32, sw, sh, stride);
                cairo_surface_t* out = scaleSurface(wrap, tw, th, true);
                cairo_surface_destroy(wrap);
                runOnMain([v, src, out, scale, d, gen, wgen] {
                    cairo_surface_destroy(src);
                    auto g = wgen.lock();
                    if(!g || *g != gen || !out) {
                        if(out) cairo_surface_destroy(out);
                        return;
                    }
                    cairo_surface_set_device_scale(out, d, d);
                    if(v->mHq) cairo_surface_destroy(v->mHq);
                    v->mHq = out;
                    v->mHqScale = scale;
                    gtk_widget_queue_draw(v->mArea);
                });
            });
            return G_SOURCE_REMOVE;
        },
        this);
}

// ---- drawing --------------------------------------------------------------

gboolean Viewer::drawCb(GtkWidget*, cairo_t* cr, gpointer self) {
    static_cast<Viewer*>(self)->draw(cr);
    return TRUE;
}

static void setColor(cairo_t* cr, const GdkRGBA& c, double alpha = 1.0) {
    cairo_set_source_rgba(cr, c.red, c.green, c.blue, c.alpha * alpha);
}

void Viewer::drawCentered(cairo_t* cr, const char* icon, const std::string& text) {
    GdkRGBA fg = themeColor(mArea, "theme_fg_color", "#d0d0d0");
    int iw, ih;
    iw = iconLogicalSize(icon, &ih);
    double x = (viewW() - iw) / 2, y = (viewH() - ih) / 2 - (text.empty() ? 0 : 14);
    setColor(cr, fg, 0.6);
    paintIcon(cr, icon, (int)x, (int)y, dpr());
    if(!text.empty()) {
        PangoLayout* l = gtk_widget_create_pango_layout(mArea, text.c_str());
        int tw, th;
        pango_layout_get_pixel_size(l, &tw, &th);
        setColor(cr, fg, 0.85);
        cairo_move_to(cr, (int)((viewW() - tw) / 2), (int)(y + ih + 10));
        pango_cairo_show_layout(cr, l);
        g_object_unref(l);
    }
}

void Viewer::draw(cairo_t* cr) {
    GtkStyleContext* ctx = gtk_widget_get_style_context(mArea);
    gtk_render_background(ctx, cr, 0, 0, viewW(), viewH());

    if(!mImage) {
        if(mLoadingVisible)
            drawCentered(cr, "other/loading72", "");
        else if(!mError.empty())
            drawCentered(cr, mHint ? "buttons/panel/open20" : "other/loading-error72", mError);
        return;
    }
    cairo_surface_t* src = mImage->surface(mFrame);
    if(!src) return;
    double dw = dispW(), dh = dispH();

    if(settings().transparencyGrid && mImage->hasAlpha) {
        GdkRGBA a = themeColor(mArea, "theme_base_color", "#2d2d2d");
        GdkRGBA b = themeColor(mArea, "theme_fg_color", "#d0d0d0");
        cairo_save(cr);
        cairo_rectangle(cr, mOx, mOy, dw, dh);
        cairo_clip(cr);
        setColor(cr, a);
        cairo_paint(cr);
        // 8px checker in a slightly lighter shade
        setColor(cr, {a.red * 0.8 + b.red * 0.2, a.green * 0.8 + b.green * 0.2, a.blue * 0.8 + b.blue * 0.2, 1});
        int x0 = (int)std::floor(std::max(0.0, mOx)), y0 = (int)std::floor(std::max(0.0, mOy));
        int x1 = (int)std::min(viewW(), mOx + dw), y1 = (int)std::min(viewH(), mOy + dh);
        for(int y = y0 - ((y0 - (int)mOy) % 16); y < y1; y += 8)
            for(int x = x0 - ((x0 - (int)mOx) % 16) + ((((y - (int)mOy) / 8) & 1) ? 8 : 0); x < x1; x += 16)
                cairo_rectangle(cr, x, y, 8, 8);
        cairo_fill(cr);
        cairo_restore(cr);
    }

    if(mHq && mFrame == 0 && std::abs(mHqScale - mScale) < 1e-9) {
        // Snap to device pixels so the pre-scaled copy is drawn 1:1.
        double d = dpr();
        cairo_set_source_surface(cr, mHq, std::round(mOx * d) / d, std::round(mOy * d) / d);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_NEAREST);
        cairo_paint(cr);
    } else {
        cairo_filter_t filter = CAIRO_FILTER_BILINEAR;
        bool upscale = mScale > 1.0;
        switch(settings().scalingFilter) {
            case ScalingFilter::Nearest: filter = CAIRO_FILTER_NEAREST; break;
            case ScalingFilter::Bilinear: filter = CAIRO_FILTER_BILINEAR; break;
            case ScalingFilter::Quality:
                filter = upscale ? (settings().smoothUpscaling ? CAIRO_FILTER_BILINEAR : CAIRO_FILTER_NEAREST)
                                 : (mImage->animated() ? CAIRO_FILTER_GOOD : CAIRO_FILTER_BILINEAR);
                break;
        }
        if(mImage->animated() && !settings().smoothAnimated) filter = CAIRO_FILTER_NEAREST;
        if(std::abs(mScale - 1.0) < 1e-9) filter = CAIRO_FILTER_NEAREST;
        double k = mScale / dpr();
        cairo_save(cr);
        cairo_rectangle(cr, mOx, mOy, dw, dh);
        cairo_clip(cr);
        cairo_translate(cr, mOx, mOy);
        cairo_scale(cr, k, k);
        cairo_set_source_surface(cr, src, 0, 0);
        cairo_pattern_set_filter(cairo_get_source(cr), filter);
        cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_PAD);
        cairo_paint(cr);
        cairo_restore(cr);
    }

    if(mCropping) drawCrop(cr);

    // Clickable edges: arrow hint while hovering the zone.
    if(settings().clickableEdges && !mCropping && !mPanning && mCursorX >= 0) {
        double zone = std::max(60.0, viewW() * 0.12);
        const char* icon = nullptr;
        double x = 0;
        if(mCursorX < zone) {
            icon = "overlay/arrow_left_50";
            x = 16;
        } else if(mCursorX > viewW() - zone) {
            icon = "overlay/arrow_right_50";
            x = viewW() - 16 - 50;
        }
        if(icon) {
            double y = (viewH() - 50) / 2;
            setColor(cr, themeColor(mArea, "theme_bg_color", "#303030"));
            cairo_rectangle(cr, x, y, 50, 50);
            cairo_fill(cr);
            setColor(cr, themeColor(mArea, "theme_fg_color", "#d0d0d0"));
            paintIcon(cr, icon, x, y, dpr());
        }
    }
    if(mMapVisible && settings().mapOverlay && scrollable() && !mCropping) drawMap(cr);
    drawZoomIndicator(cr);
    if(mLoadingVisible) drawCentered(cr, "other/loading72", "");
}

void Viewer::drawZoomIndicator(cairo_t* cr) {
    ZoomIndicator mode = settings().zoomIndicator;
    if(mode == ZoomIndicator::Off) return;
    if(mode == ZoomIndicator::Auto && (mFitMode == FitMode::Window || std::abs(mScale - 1.0) < 1e-6 ||
                                       std::abs(mScale - fitScale(FitMode::Window)) < 1e-6))
        return;
    char buf[32];
    snprintf(buf, sizeof(buf), "%d%%", (int)std::lround(mScale * 100));
    PangoLayout* l = gtk_widget_create_pango_layout(mArea, buf);
    int tw, th;
    pango_layout_get_pixel_size(l, &tw, &th);
    double x = 10, y = viewH() - th - 18;
    setColor(cr, themeColor(mArea, "theme_bg_color", "#303030"));
    cairo_rectangle(cr, x, y, tw + 16, th + 8);
    cairo_fill(cr);
    setColor(cr, themeColor(mArea, "theme_fg_color", "#d0d0d0"));
    cairo_move_to(cr, x + 8, y + 4);
    pango_cairo_show_layout(cr, l);
    g_object_unref(l);
}

void Viewer::drawMap(cairo_t* cr) {
    double maxW = 150, maxH = 110;
    double ar = (double)mImage->width / mImage->height;
    double mw = maxW, mh = maxW / ar;
    if(mh > maxH) {
        mh = maxH;
        mw = maxH * ar;
    }
    double x = viewW() - mw - 14, y = viewH() - mh - 14;
    GdkRGBA bg = themeColor(mArea, "theme_bg_color", "#303030");
    GdkRGBA border = themeColor(mArea, "borders", "#1b1b1b");
    GdkRGBA sel = themeColor(mArea, "theme_selected_bg_color", "#3584e4");
    setColor(cr, bg);
    cairo_rectangle(cr, x - 3, y - 3, mw + 6, mh + 6);
    cairo_fill(cr);
    cairo_save(cr);
    cairo_rectangle(cr, x, y, mw, mh);
    cairo_clip(cr);
    cairo_translate(cr, x, y);
    cairo_scale(cr, mw / mImage->width, mh / mImage->height);
    cairo_set_source_surface(cr, mImage->surface(mFrame), 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_FAST);
    cairo_paint(cr);
    cairo_restore(cr);
    setColor(cr, border);
    cairo_set_line_width(cr, 1);
    cairo_rectangle(cr, x - 2.5, y - 2.5, mw + 5, mh + 5);
    cairo_stroke(cr);
    // Visible region
    double ix0, iy0, ix1, iy1;
    widgetToImage(0, 0, ix0, iy0);
    widgetToImage(viewW(), viewH(), ix1, iy1);
    ix0 = std::clamp(ix0, 0.0, (double)mImage->width);
    ix1 = std::clamp(ix1, 0.0, (double)mImage->width);
    iy0 = std::clamp(iy0, 0.0, (double)mImage->height);
    iy1 = std::clamp(iy1, 0.0, (double)mImage->height);
    double kx = mw / mImage->width, ky = mh / mImage->height;
    setColor(cr, sel);
    cairo_set_line_width(cr, 2);
    cairo_rectangle(cr, x + ix0 * kx + 1, y + iy0 * ky + 1, std::max(2.0, (ix1 - ix0) * kx - 2),
                    std::max(2.0, (iy1 - iy0) * ky - 2));
    cairo_stroke(cr);
}

void Viewer::drawCrop(cairo_t* cr) {
    double x0, y0, x1, y1;
    imageToWidget(mCrop.x, mCrop.y, x0, y0);
    imageToWidget(mCrop.x + mCrop.w, mCrop.y + mCrop.h, x1, y1);
    double ix0, iy0, ix1, iy1;
    imageToWidget(0, 0, ix0, iy0);
    imageToWidget(mImage->width, mImage->height, ix1, iy1);
    // Darken everything outside the selection.
    cairo_save(cr);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
    cairo_rectangle(cr, ix0, iy0, ix1 - ix0, iy1 - iy0);
    cairo_rectangle(cr, x0, y0, x1 - x0, y1 - y0);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.6);
    cairo_fill(cr);
    cairo_restore(cr);
    GdkRGBA sel = themeColor(mArea, "theme_selected_bg_color", "#3584e4");
    setColor(cr, sel);
    cairo_set_line_width(cr, 1);
    cairo_rectangle(cr, std::floor(x0) + 0.5, std::floor(y0) + 0.5, std::round(x1 - x0), std::round(y1 - y0));
    cairo_stroke(cr);
    // Rule of thirds
    setColor(cr, sel, 0.5);
    for(int i = 1; i < 3; i++) {
        double gx = std::floor(x0 + (x1 - x0) * i / 3) + 0.5, gy = std::floor(y0 + (y1 - y0) * i / 3) + 0.5;
        cairo_move_to(cr, gx, y0);
        cairo_line_to(cr, gx, y1);
        cairo_move_to(cr, x0, gy);
        cairo_line_to(cr, x1, gy);
    }
    cairo_stroke(cr);
    // Handles
    setColor(cr, sel);
    double hx[8] = {x0, (x0 + x1) / 2, x1, x1, x1, (x0 + x1) / 2, x0, x0};
    double hy[8] = {y0, y0, y0, (y0 + y1) / 2, y1, y1, y1, (y0 + y1) / 2};
    for(int i = 0; i < 8; i++) cairo_rectangle(cr, std::round(hx[i]) - 4, std::round(hy[i]) - 4, 8, 8);
    cairo_fill(cr);
}

// ---- crop -----------------------------------------------------------------

void Viewer::startCrop() {
    if(!mImage) return;
    mCropping = true;
    mCrop = {0, 0, mImage->width, mImage->height};
    mFitMode = FitMode::Window;
    applyFit(true);
    if(onCropChanged) onCropChanged(mCrop);
    gtk_widget_queue_draw(mArea);
}

void Viewer::endCrop() {
    mCropping = false;
    mCropDrag = -1;
    gdk_window_set_cursor(gtk_widget_get_window(mArea), nullptr);
    gtk_widget_queue_draw(mArea);
}

void Viewer::setCropRect(const CropRect& r) {
    if(!mImage) return;
    CropRect c = r;
    c.w = std::clamp(c.w, 1, mImage->width);
    c.h = std::clamp(c.h, 1, mImage->height);
    c.x = std::clamp(c.x, 0, mImage->width - c.w);
    c.y = std::clamp(c.y, 0, mImage->height - c.h);
    mCrop = c;
    gtk_widget_queue_draw(mArea);
}

void Viewer::setCropAspect(double ratio) {
    mCropAspect = ratio;
    if(ratio > 0 && mImage) {
        CropRect c = mCrop;
        int h = (int)std::lround(c.w / ratio);
        if(h > mImage->height - c.y) {
            h = mImage->height - c.y;
            c.w = (int)std::lround(h * ratio);
        }
        c.h = std::max(1, h);
        setCropRect(c);
        if(onCropChanged) onCropChanged(mCrop);
    }
}

int Viewer::cropHandleAt(double x, double y) const {
    double x0, y0, x1, y1;
    imageToWidget(mCrop.x, mCrop.y, x0, y0);
    imageToWidget(mCrop.x + mCrop.w, mCrop.y + mCrop.h, x1, y1);
    double hx[8] = {x0, (x0 + x1) / 2, x1, x1, x1, (x0 + x1) / 2, x0, x0};
    double hy[8] = {y0, y0, y0, (y0 + y1) / 2, y1, y1, y1, (y0 + y1) / 2};
    for(int i = 0; i < 8; i++)
        if(std::abs(x - hx[i]) <= 8 && std::abs(y - hy[i]) <= 8) return i;
    if(x > x0 && x < x1 && y > y0 && y < y1) return 8;
    return -1;
}

void Viewer::updateCropFromDrag(double x, double y) {
    double ix, iy;
    widgetToImage(x, y, ix, iy);
    int W = mImage->width, H = mImage->height;
    ix = std::clamp(ix, 0.0, (double)W);
    iy = std::clamp(iy, 0.0, (double)H);
    CropRect s = mCropStart;
    int l = s.x, t = s.y, r = s.x + s.w, b = s.y + s.h;
    if(mCropDrag == 8) {
        double sx, sy;
        widgetToImage(mPressX, mPressY, sx, sy);
        int dx = (int)std::lround(ix - sx), dy = (int)std::lround(iy - sy);
        CropRect c = s;
        c.x = std::clamp(s.x + dx, 0, W - s.w);
        c.y = std::clamp(s.y + dy, 0, H - s.h);
        mCrop = c;
    } else {
        if(mCropDrag == 9) {
            l = (int)std::lround(std::min(mCropAnchorX, ix));
            r = (int)std::lround(std::max(mCropAnchorX, ix));
            t = (int)std::lround(std::min(mCropAnchorY, iy));
            b = (int)std::lround(std::max(mCropAnchorY, iy));
        } else {
            int h = mCropDrag;
            if(h == 0 || h == 6 || h == 7) l = (int)std::lround(ix);
            if(h == 2 || h == 3 || h == 4) r = (int)std::lround(ix);
            if(h == 0 || h == 1 || h == 2) t = (int)std::lround(iy);
            if(h == 4 || h == 5 || h == 6) b = (int)std::lround(iy);
            if(l > r) std::swap(l, r);
            if(t > b) std::swap(t, b);
        }
        int w = std::max(1, r - l), hh = std::max(1, b - t);
        if(mCropAspect > 0) {
            // Keep the ratio: derive height from width (edges on the vertical axis drive width).
            if(mCropDrag == 1 || mCropDrag == 5)
                w = (int)std::lround(hh * mCropAspect);
            else
                hh = (int)std::lround(w / mCropAspect);
            if(t + hh > H) {
                hh = H - t;
                w = (int)std::lround(hh * mCropAspect);
            }
            if(l + w > W) {
                w = W - l;
                hh = (int)std::lround(w / mCropAspect);
            }
        }
        mCrop = {l, t, std::max(1, w), std::max(1, hh)};
        setCropRect(mCrop);
    }
    if(onCropChanged) onCropChanged(mCrop);
    gtk_widget_queue_draw(mArea);
}

// ---- input ----------------------------------------------------------------

void Viewer::bumpMap() {
    if(!settings().mapOverlay) return;
    mMapVisible = true;
    if(mMapTimer) g_source_remove(mMapTimer);
    mMapTimer = g_timeout_add(
        1500,
        [](gpointer p) -> gboolean {
            auto* v = static_cast<Viewer*>(p);
            v->mMapTimer = 0;
            v->mMapVisible = false;
            gtk_widget_queue_draw(v->mArea);
            return G_SOURCE_REMOVE;
        },
        this);
}

void Viewer::bumpCursor() {
    GdkWindow* win = gtk_widget_get_window(mArea);
    if(mCursorHidden && win) {
        gdk_window_set_cursor(win, nullptr);
        mCursorHidden = false;
    }
    if(mCursorTimer) {
        g_source_remove(mCursorTimer);
        mCursorTimer = 0;
    }
    if(!settings().cursorAutohide) return;
    mCursorTimer = g_timeout_add(
        1500,
        [](gpointer p) -> gboolean {
            auto* v = static_cast<Viewer*>(p);
            v->mCursorTimer = 0;
            GdkWindow* w = gtk_widget_get_window(v->mArea);
            if(w && !v->mCropping && !v->mPanning && v->mCursorX >= 0) {
                GdkCursor* c = gdk_cursor_new_for_display(gdk_window_get_display(w), GDK_BLANK_CURSOR);
                gdk_window_set_cursor(w, c);
                g_object_unref(c);
                v->mCursorHidden = true;
            }
            return G_SOURCE_REMOVE;
        },
        this);
}

gboolean Viewer::pressCb(GtkWidget*, GdkEventButton* ev, gpointer self) {
    auto* v = static_cast<Viewer*>(self);
    v->bumpCursor();
    if(v->onPressHook && v->onPressHook()) return TRUE;
    if(ev->type == GDK_3BUTTON_PRESS) return TRUE;
    if(ev->type == GDK_2BUTTON_PRESS) {
        if(ev->button == 1 && !v->mCropping && v->onInput)
            v->onInput(buttonSequence(1, ev->state, true), ev->x, ev->y);
        return TRUE;
    }
    v->mPressX = v->mLastX = ev->x;
    v->mPressY = v->mLastY = ev->y;
    v->mDragged = false;
    if(ev->button == 1) {
        if(v->mCropping && v->mImage) {
            v->mCropDrag = v->cropHandleAt(ev->x, ev->y);
            v->mCropStart = v->mCrop;
            if(v->mCropDrag < 0) {
                v->mCropDrag = 9;
                double ix, iy;
                v->widgetToImage(ev->x, ev->y, ix, iy);
                v->mCropAnchorX = std::clamp(ix, 0.0, (double)v->mImage->width);
                v->mCropAnchorY = std::clamp(iy, 0.0, (double)v->mImage->height);
            }
            return TRUE;
        }
        v->mPanning = true;
        return TRUE;
    }
    if(ev->button == 3) {
        v->mRmbDown = true;
        v->mRmbZoom = false;
        return TRUE;
    }
    if(v->onInput) {
        std::string seq = buttonSequence(ev->button, ev->state, false);
        if(!seq.empty()) v->onInput(seq, ev->x, ev->y);
    }
    return TRUE;
}

gboolean Viewer::releaseCb(GtkWidget*, GdkEventButton* ev, gpointer self) {
    auto* v = static_cast<Viewer*>(self);
    if(ev->button == 1) {
        if(v->mCropping) {
            v->mCropDrag = -1;
            return TRUE;
        }
        bool wasPanning = v->mPanning;
        v->mPanning = false;
        if(wasPanning && !v->mDragged) {
            if(settings().clickableEdges && v->onEdgeClick) {
                double zone = std::max(60.0, v->viewW() * 0.12);
                if(ev->x < zone)
                    v->onEdgeClick(-1);
                else if(ev->x > v->viewW() - zone)
                    v->onEdgeClick(1);
            }
            if(v->onInput && (ev->state & gtk_accelerator_get_default_mod_mask()))
                v->onInput(buttonSequence(1, ev->state, false), ev->x, ev->y);
        }
        gtk_widget_queue_draw(v->mArea);
        return TRUE;
    }
    if(ev->button == 3) {
        bool zoomed = v->mRmbZoom;
        v->mRmbDown = false;
        v->mRmbZoom = false;
        if(!zoomed && v->onInput) v->onInput(buttonSequence(3, ev->state, false), ev->x, ev->y);
        return TRUE;
    }
    return TRUE;
}

gboolean Viewer::motionCb(GtkWidget*, GdkEventMotion* ev, gpointer self) {
    auto* v = static_cast<Viewer*>(self);
    bool edgeBefore = settings().clickableEdges;
    double oldX = v->mCursorX;
    v->mCursorX = ev->x;
    v->mCursorY = ev->y;
    v->bumpCursor();
    if(v->mCropping && v->mImage) {
        if(v->mCropDrag >= 0) {
            v->updateCropFromDrag(ev->x, ev->y);
        } else {
            static const char* names[] = {"nw-resize", "n-resize", "ne-resize", "e-resize", "se-resize",
                                          "s-resize",  "sw-resize", "w-resize", "move"};
            int h = v->cropHandleAt(ev->x, ev->y);
            GdkWindow* w = gtk_widget_get_window(v->mArea);
            GdkCursor* c = gdk_cursor_new_from_name(gdk_window_get_display(w), h >= 0 ? names[h] : "crosshair");
            gdk_window_set_cursor(w, c);
            if(c) g_object_unref(c);
        }
    } else if(v->mPanning && (ev->state & GDK_BUTTON1_MASK)) {
        double dx = ev->x - v->mLastX, dy = ev->y - v->mLastY;
        if(std::abs(ev->x - v->mPressX) > 3 || std::abs(ev->y - v->mPressY) > 3) v->mDragged = true;
        if(v->mDragged && v->scrollable()) {
            v->mOx += dx;
            v->mOy += dy;
            v->clampOffset();
            v->bumpMap();
            gtk_widget_queue_draw(v->mArea);
        }
    } else if(v->mRmbDown && (ev->state & GDK_BUTTON3_MASK) && v->mImage) {
        if(std::abs(ev->y - v->mPressY) > 10) v->mRmbZoom = true;
        if(v->mRmbZoom) {
            double dy = ev->y - v->mLastY;
            v->mFitMode = FitMode::Free;
            v->setScaleAt(v->mScale * std::pow(1.006, -dy), v->mPressX, v->mPressY);
            v->bumpMap();
        }
    } else if(edgeBefore) {
        double zone = std::max(60.0, v->viewW() * 0.12);
        auto region = [&](double x) { return x < 0 ? 0 : x < zone ? 1 : x > v->viewW() - zone ? 2 : 0; };
        if(region(oldX) != region(ev->x)) gtk_widget_queue_draw(v->mArea);
    }
    v->mLastX = ev->x;
    v->mLastY = ev->y;
    if(v->onMotion) v->onMotion(ev->x, ev->y);
    return FALSE;
}

gboolean Viewer::leaveCb(GtkWidget*, GdkEventCrossing* ev, gpointer self) {
    auto* v = static_cast<Viewer*>(self);
    if(ev->mode != GDK_CROSSING_NORMAL) return FALSE;
    v->mCursorX = v->mCursorY = -1;
    if(settings().clickableEdges) gtk_widget_queue_draw(v->mArea);
    return FALSE;
}

gboolean Viewer::scrollCb(GtkWidget*, GdkEventScroll* ev, gpointer self) {
    auto* v = static_cast<Viewer*>(self);
    v->mCursorX = ev->x;
    v->mCursorY = ev->y;
    auto step = [&](GdkScrollDirection dir) {
        guint mods = ev->state & gtk_accelerator_get_default_mod_mask();
        if(settings().wheelScrollsZoomed && !mods && v->scrollable() &&
           (dir == GDK_SCROLL_UP || dir == GDK_SCROLL_DOWN) && v->dispH() > v->viewH() + 0.5) {
            v->scrollBy(0, dir == GDK_SCROLL_UP ? -120 : 120);
            return;
        }
        if(v->onInput) v->onInput(scrollSequence(dir, ev->state), ev->x, ev->y);
    };
    if(ev->direction == GDK_SCROLL_SMOOTH) {
        v->mScrollAccum += ev->delta_y;
        while(v->mScrollAccum >= 1.0) {
            v->mScrollAccum -= 1.0;
            step(GDK_SCROLL_DOWN);
        }
        while(v->mScrollAccum <= -1.0) {
            v->mScrollAccum += 1.0;
            step(GDK_SCROLL_UP);
        }
        if(ev->is_stop) v->mScrollAccum = 0;
    } else {
        step(ev->direction);
    }
    return TRUE;
}

void Viewer::sizeCb(GtkWidget*, GdkRectangle*, gpointer self) {
    auto* v = static_cast<Viewer*>(self);
    if(!v->mImage) return;
    if(v->mFitMode != FitMode::Free)
        v->applyFit(true);
    else
        v->clampOffset();
}

}  // namespace qimgv
