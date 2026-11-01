// Custom drawn thumbnail list used by the thumbnail panel (single row strip) and the folder view
// (wrapping grid with file names). Only visible items are drawn and thumbnailed.
#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <string>
#include <vector>

#include "core/thumbnailer.hpp"

namespace qimgv {

class ThumbGrid {
public:
    enum class Mode { Strip, Grid };
    struct Item {
        std::string path;
        std::string label;
        bool isDir = false;
    };

    ThumbGrid(Mode mode, Thumbnailer& thumbnailer);
    GtkWidget* widget() const { return mScroll; }
    GtkWidget* canvas() const { return mArea; }

    void setItems(std::vector<Item> items);
    const std::vector<Item>& items() const { return mItems; }
    void setCurrent(int index, bool scrollTo = true);
    int current() const { return mCurrent; }
    void setThumbSize(int size);
    int thumbSize() const { return mSize; }
    void setSquare(bool on);
    void refreshItem(const std::string& path);
    void scrollToCurrent();

    std::function<void(int index)> onActivate;  // double click / Enter (strip: single click)
    std::function<void(int index)> onSelect;    // selection changed by the user
    std::function<void(int index, GdkEventButton* ev)> onRightClick;
    std::function<void()> onBackground;          // click on empty space

private:
    static gboolean drawCb(GtkWidget*, cairo_t*, gpointer);
    static gboolean pressCb(GtkWidget*, GdkEventButton*, gpointer);
    static gboolean motionCb(GtkWidget*, GdkEventMotion*, gpointer);
    static gboolean leaveCb(GtkWidget*, GdkEventCrossing*, gpointer);
    static gboolean scrollCb(GtkWidget*, GdkEventScroll*, gpointer);
    static gboolean keyCb(GtkWidget*, GdkEventKey*, gpointer);
    static void sizeCb(GtkWidget*, GdkRectangle*, gpointer);

    void relayout();
    int cellW() const;
    int cellH() const;
    int columns() const;
    int indexAt(double x, double y) const;
    GdkRectangle itemRect(int i) const;
    int requestSize() const;
    void draw(cairo_t* cr);
    void moveSelection(int delta);

    Mode mMode;
    Thumbnailer& mThumbs;
    GtkWidget* mScroll;
    GtkWidget* mArea;
    std::vector<Item> mItems;
    int mCurrent = -1;
    int mHover = -1;
    int mSize = 120;
    bool mSquare = false;
    int mCols = 1;
    int mMarginX = 0;
    int mLastWidth = 0;
    double mScrollAccum = 0;
};

}  // namespace qimgv
