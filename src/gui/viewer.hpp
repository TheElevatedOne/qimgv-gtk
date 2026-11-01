// Image viewer widget: fit modes, zoom, panning, animation playback, high quality scaling
// (done on a worker thread), transparency grid, zoom indicator, mini-map and crop selection.
#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <memory>
#include <string>

#include "core/workers.hpp"
#include "image/image.hpp"
#include "settings.hpp"

namespace qimgv {

struct CropRect {
    int x = 0, y = 0, w = 0, h = 0;
};

class Viewer {
public:
    Viewer();
    ~Viewer();
    GtkWidget* widget() const { return mArea; }

    void setImage(ImagePtr img);
    void clear();
    void showLoading(bool on);
    void showError(const std::string& msg);
    void showHint(const std::string& msg);  // empty state text (no image open)
    // Image data was edited in place (rotate/flip/crop/resize).
    void imageEdited();
    ImagePtr image() const { return mImage; }

    void setFitMode(FitMode m);
    FitMode fitMode() const { return mFitMode; }
    void zoomIn(bool atCursor);
    void zoomOut(bool atCursor);
    double scale() const { return mScale; }  // 1.0 = one image pixel per device pixel
    void scrollBy(double dx, double dy);
    bool scrollable() const;
    void setKeepZoom(bool on) { mKeepZoom = on; }
    void settingsChanged();

    // Crop mode
    void startCrop();
    void endCrop();
    bool cropping() const { return mCropping; }
    CropRect cropRect() const { return mCrop; }
    void setCropRect(const CropRect& r);
    void setCropAspect(double ratio);  // 0 = free
    std::function<void(const CropRect&)> onCropChanged;

    // Mouse sequences ("LMB_DoubleClick", "RMB", "WheelUp", ...) for the shortcut map,
    // with the pointer position in widget coordinates.
    std::function<bool(const std::string& seq, double x, double y)> onInput;
    std::function<void(double x, double y)> onMotion;
    std::function<void()> onScaleChanged;
    std::function<void(int direction)> onEdgeClick;  // clickable edges: -1 prev, +1 next
    // Called first on every button press; return true to swallow the click (e.g. closing a popup).
    std::function<bool()> onPressHook;

private:
    static gboolean drawCb(GtkWidget*, cairo_t*, gpointer);
    static gboolean pressCb(GtkWidget*, GdkEventButton*, gpointer);
    static gboolean releaseCb(GtkWidget*, GdkEventButton*, gpointer);
    static gboolean motionCb(GtkWidget*, GdkEventMotion*, gpointer);
    static gboolean scrollCb(GtkWidget*, GdkEventScroll*, gpointer);
    static gboolean leaveCb(GtkWidget*, GdkEventCrossing*, gpointer);
    static void sizeCb(GtkWidget*, GdkRectangle*, gpointer);

    int dpr() const;
    double viewW() const;
    double viewH() const;
    double dispW() const;  // displayed image size in logical px
    double dispH() const;
    double fitScale(FitMode m) const;
    void applyFit(bool resetPosition);
    void clampOffset();
    void setScaleAt(double s, double ax, double ay);
    void scaleChanged();
    void requestHq();
    void dropHq();
    void startAnimation();
    void stopAnimation();
    void bumpMap();
    void bumpCursor();
    void draw(cairo_t* cr);
    void drawCrop(cairo_t* cr);
    void drawMap(cairo_t* cr);
    void drawZoomIndicator(cairo_t* cr);
    void drawCentered(cairo_t* cr, const char* icon, const std::string& text);
    int cropHandleAt(double x, double y) const;
    void imageToWidget(double ix, double iy, double& wx, double& wy) const;
    void widgetToImage(double wx, double wy, double& ix, double& iy) const;
    void updateCropFromDrag(double x, double y);

    GtkWidget* mArea;
    ImagePtr mImage;
    size_t mFrame = 0;
    guint mAnimTimer = 0;
    FitMode mFitMode = FitMode::Window;
    double mScale = 1.0;
    double mOx = 0, mOy = 0;  // image origin in widget coords
    bool mKeepZoom = false;

    // High quality downscaled copy of the current frame.
    cairo_surface_t* mHq = nullptr;
    double mHqScale = 0;
    guint mHqTimer = 0;
    std::unique_ptr<ThreadPool> mHqPool;
    std::shared_ptr<int> mHqGen;

    bool mLoading = false;
    guint mLoadingTimer = 0;
    bool mLoadingVisible = false;
    std::string mError;
    bool mHint = false;

    // Interaction
    bool mPanning = false, mDragged = false, mRmbZoom = false, mRmbDown = false;
    double mPressX = 0, mPressY = 0, mLastX = 0, mLastY = 0, mCursorX = -1, mCursorY = -1;
    double mScrollAccum = 0;
    guint mMapTimer = 0;
    bool mMapVisible = false;
    guint mCursorTimer = 0;
    bool mCursorHidden = false;

    // Crop
    bool mCropping = false;
    CropRect mCrop;
    double mCropAspect = 0;
    int mCropDrag = -1;  // handle index, 8 = move, 9 = new selection
    CropRect mCropStart;
    double mCropAnchorX = 0, mCropAnchorY = 0;
};

}  // namespace qimgv
