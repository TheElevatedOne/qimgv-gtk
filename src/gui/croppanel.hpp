// Side panel shown in crop mode: selection geometry, aspect ratio and crop / save / cancel.
#pragma once

#include <gtk/gtk.h>

#include <functional>

#include "gui/viewer.hpp"

namespace qimgv {

class CropPanel {
public:
    CropPanel();
    GtkWidget* widget() const { return mRevealer; }
    void open(int imageW, int imageH);
    void close();
    bool visible() const;
    void setRect(const CropRect& r);
    std::function<void(const CropRect&)> onRectChanged;
    std::function<void(double ratio)> onAspectChanged;
    std::function<void(bool save)> onCrop;
    std::function<void()> onCancel;

private:
    GtkWidget* mRevealer;
    GtkWidget* mX;
    GtkWidget* mY;
    GtkWidget* mW;
    GtkWidget* mH;
    GtkWidget* mAspect;
    int mImgW = 0, mImgH = 0;
    bool mBlock = false;
};

}  // namespace qimgv
