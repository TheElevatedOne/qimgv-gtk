#include "gui/thumbgrid.hpp"

#include <algorithm>
#include <cmath>

#include "gui/icons.hpp"
#include "gui/style.hpp"

namespace qimgv {

static constexpr int kPad = 6;      // spacing around a thumbnail inside its cell
static constexpr int kLabelH = 22;  // folder view label height

ThumbGrid::ThumbGrid(Mode mode, Thumbnailer& thumbnailer) : mMode(mode), mThumbs(thumbnailer) {
    mScroll = gtk_scrolled_window_new(nullptr, nullptr);
    mArea = gtk_drawing_area_new();
    addClass(mArea, "qimgv-grid");
    gtk_widget_add_events(mArea, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK |
                                     GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK | GDK_KEY_PRESS_MASK);
    if(mode == Mode::Strip) {
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(mScroll), GTK_POLICY_EXTERNAL, GTK_POLICY_NEVER);
        gtk_widget_set_can_focus(mArea, FALSE);
    } else {
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(mScroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_widget_set_can_focus(mArea, TRUE);
    }
    gtk_container_add(GTK_CONTAINER(mScroll), mArea);  // wraps it in a GtkViewport
    GtkWidget* vp = gtk_bin_get_child(GTK_BIN(mScroll));
    gtk_viewport_set_shadow_type(GTK_VIEWPORT(vp), GTK_SHADOW_NONE);
    g_signal_connect(mArea, "draw", G_CALLBACK(drawCb), this);
    g_signal_connect(mArea, "button-press-event", G_CALLBACK(pressCb), this);
    g_signal_connect(mArea, "motion-notify-event", G_CALLBACK(motionCb), this);
    g_signal_connect(mArea, "leave-notify-event", G_CALLBACK(leaveCb), this);
    g_signal_connect(mArea, "scroll-event", G_CALLBACK(scrollCb), this);
    g_signal_connect(mArea, "key-press-event", G_CALLBACK(keyCb), this);
    g_signal_connect(mScroll, "size-allocate", G_CALLBACK(sizeCb), this);
    relayout();
}

int ThumbGrid::cellW() const { return mSize + kPad * 2; }
int ThumbGrid::cellH() const { return mMode == Mode::Strip ? mSize + kPad * 2 : mSize + kPad * 2 + kLabelH; }
int ThumbGrid::columns() const { return mMode == Mode::Strip ? std::max(1, (int)mItems.size()) : mCols; }

void ThumbGrid::relayout() {
    if(mMode == Mode::Strip) {
        gtk_widget_set_size_request(mArea, std::max(1, (int)mItems.size()) * cellW(), cellH());
        gtk_widget_set_size_request(mScroll, -1, cellH());
    } else {
        int w = gtk_widget_get_allocated_width(mScroll);
        if(w <= 1) w = 800;
        mCols = std::max(1, w / cellW());
        mMarginX = std::max(0, (w - mCols * cellW()) / 2);
        int rows = ((int)mItems.size() + mCols - 1) / mCols;
        gtk_widget_set_size_request(mArea, -1, std::max(1, rows * cellH() + kPad));
    }
    gtk_widget_queue_draw(mArea);
}

void ThumbGrid::sizeCb(GtkWidget*, GdkRectangle* a, gpointer self) {
    auto* g = static_cast<ThumbGrid*>(self);
    if(g->mMode == Mode::Grid && a->width != g->mLastWidth) {
        g->mLastWidth = a->width;
        // Re-layout outside of the allocation pass.
        g_idle_add(
            [](gpointer p) -> gboolean {
                static_cast<ThumbGrid*>(p)->relayout();
                return G_SOURCE_REMOVE;
            },
            g);
    }
}

void ThumbGrid::setItems(std::vector<Item> items) {
    mItems = std::move(items);
    mThumbs.cancelPending();
    mHover = -1;
    if(mCurrent >= (int)mItems.size()) mCurrent = (int)mItems.size() - 1;
    relayout();
}

void ThumbGrid::setThumbSize(int size) {
    if(size == mSize) return;
    mSize = size;
    mThumbs.cancelPending();
    relayout();
    scrollToCurrent();
}

void ThumbGrid::setSquare(bool on) {
    mSquare = on;
    gtk_widget_queue_draw(mArea);
}

void ThumbGrid::refreshItem(const std::string& path) {
    mThumbs.forget(path);
    gtk_widget_queue_draw(mArea);
}

GdkRectangle ThumbGrid::itemRect(int i) const {
    GdkRectangle r;
    if(mMode == Mode::Strip) {
        r.x = i * cellW();
        r.y = std::max(0, (gtk_widget_get_allocated_height(mArea) - cellH()) / 2);  // centre vertically
    } else {
        r.x = mMarginX + (i % mCols) * cellW();
        r.y = kPad / 2 + (i / mCols) * cellH();
    }
    r.width = cellW();
    r.height = cellH();
    return r;
}

int ThumbGrid::indexAt(double x, double y) const {
    int i;
    if(mMode == Mode::Strip) {
        i = (int)(x / cellW());
    } else {
        if(x < mMarginX || x >= mMarginX + mCols * cellW()) return -1;
        int col = (int)((x - mMarginX) / cellW());
        int row = (int)((y - kPad / 2) / cellH());
        if(row < 0) return -1;
        i = row * mCols + col;
    }
    return i >= 0 && i < (int)mItems.size() ? i : -1;
}

void ThumbGrid::setCurrent(int index, bool scroll) {
    if(index == mCurrent && !scroll) return;
    mCurrent = index;
    gtk_widget_queue_draw(mArea);
    if(scroll) scrollToCurrent();
}

void ThumbGrid::scrollToCurrent() {
    if(mCurrent < 0 || mCurrent >= (int)mItems.size()) return;
    // Defer until layout is done so the adjustment has its final range.
    g_idle_add(
        [](gpointer p) -> gboolean {
            auto* g = static_cast<ThumbGrid*>(p);
            if(g->mCurrent < 0) return G_SOURCE_REMOVE;
            GdkRectangle r = g->itemRect(g->mCurrent);
            if(g->mMode == Mode::Strip) {
                GtkAdjustment* a = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(g->mScroll));
                double page = gtk_adjustment_get_page_size(a);
                gtk_adjustment_set_value(a, r.x + r.width / 2.0 - page / 2.0);  // keep current centred
            } else {
                GtkAdjustment* a = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(g->mScroll));
                double v = gtk_adjustment_get_value(a), page = gtk_adjustment_get_page_size(a);
                if(r.y < v)
                    gtk_adjustment_set_value(a, r.y);
                else if(r.y + r.height > v + page)
                    gtk_adjustment_set_value(a, r.y + r.height - page);
            }
            return G_SOURCE_REMOVE;
        },
        this);
}

int ThumbGrid::requestSize() const {
    static const int buckets[] = {64, 96, 128, 160, 200, 256, 320, 400, 512, 640, 800};
    int want = mSize * std::max(1, gtk_widget_get_scale_factor(mArea));
    for(int b : buckets)
        if(b >= want) return b;
    return 800;
}

gboolean ThumbGrid::drawCb(GtkWidget*, cairo_t* cr, gpointer self) {
    static_cast<ThumbGrid*>(self)->draw(cr);
    return TRUE;
}

static void setColor(cairo_t* cr, const GdkRGBA& c) { cairo_set_source_rgba(cr, c.red, c.green, c.blue, c.alpha); }

void ThumbGrid::draw(cairo_t* cr) {
    GtkStyleContext* ctx = gtk_widget_get_style_context(mArea);
    int W = gtk_widget_get_allocated_width(mArea), H = gtk_widget_get_allocated_height(mArea);
    gtk_render_background(ctx, cr, 0, 0, W, H);
    GdkRectangle clip;
    if(!gdk_cairo_get_clip_rectangle(cr, &clip)) clip = {0, 0, W, H};
    GdkRGBA fg = themeColor(mArea, "theme_fg_color", "#d0d0d0");
    GdkRGBA sel = themeColor(mArea, "theme_selected_bg_color", "#3584e4");
    GdkRGBA selFg = themeColor(mArea, "theme_selected_fg_color", "#ffffff");
    GdkRGBA bg = themeColor(mArea, "theme_base_color", "#2d2d2d");
    GdkRGBA hover = {bg.red * 0.85 + fg.red * 0.15, bg.green * 0.85 + fg.green * 0.15,
                     bg.blue * 0.85 + fg.blue * 0.15, 1};
    int first, last;
    if(mMode == Mode::Strip) {
        first = std::max(0, clip.x / cellW());
        last = std::min((int)mItems.size() - 1, (clip.x + clip.width) / cellW());
    } else {
        int r0 = std::max(0, (clip.y - kPad / 2) / cellH()), r1 = (clip.y + clip.height) / cellH();
        first = r0 * mCols;
        last = std::min((int)mItems.size() - 1, (r1 + 1) * mCols - 1);
    }
    int dpr = std::max(1, gtk_widget_get_scale_factor(mArea));
    int req = requestSize();
    for(int i = first; i <= last; i++) {
        const Item& it = mItems[i];
        GdkRectangle r = itemRect(i);
        if(i == mCurrent) {
            setColor(cr, sel);
            cairo_rectangle(cr, r.x + 1, r.y + 1, r.width - 2, r.height - 2);
            cairo_fill(cr);
        } else if(i == mHover) {
            setColor(cr, hover);
            cairo_rectangle(cr, r.x + 1, r.y + 1, r.width - 2, r.height - 2);
            cairo_fill(cr);
        }
        double tx = r.x + kPad, ty = r.y + kPad;
        if(it.isDir) {
            int iw, ih;
            iw = iconLogicalSize("other/folder96", &ih);
            double k = std::min(1.0, (double)mSize / std::max(iw, ih) * 0.8);
            cairo_save(cr);
            cairo_translate(cr, tx + (mSize - iw * k) / 2, ty + (mSize - ih * k) / 2);
            cairo_scale(cr, k, k);
            setColor(cr, i == mCurrent ? selFg : fg);
            paintIcon(cr, "other/folder96", 0, 0, dpr * (k > 0.5 ? 2 : 1));
            cairo_restore(cr);
        } else {
            cairo_surface_t* t = mThumbs.get(it.path, req, mSquare, [this](const std::string&, int, cairo_surface_t*) {
                gtk_widget_queue_draw(mArea);
            });
            if(t) {
                int sw = cairo_image_surface_get_width(t), sh = cairo_image_surface_get_height(t);
                // Fit the cell, but never upscale small images past their natural size.
                double k = std::min({(double)mSize / sw, (double)mSize / sh, 1.0 / dpr});
                double dw = sw * k, dh = sh * k;
                cairo_save(cr);
                cairo_translate(cr, std::round(tx + (mSize - dw) / 2), std::round(ty + (mSize - dh) / 2));
                cairo_scale(cr, k, k);
                cairo_set_source_surface(cr, t, 0, 0);
                cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
                cairo_paint(cr);
                cairo_restore(cr);
            }
        }
        if(mMode == Mode::Grid) {
            PangoLayout* l = gtk_widget_create_pango_layout(mArea, it.label.c_str());
            pango_layout_set_width(l, (r.width - 8) * PANGO_SCALE);
            pango_layout_set_ellipsize(l, PANGO_ELLIPSIZE_MIDDLE);
            pango_layout_set_alignment(l, PANGO_ALIGN_CENTER);
            int lw, lh;
            pango_layout_get_pixel_size(l, &lw, &lh);
            setColor(cr, i == mCurrent ? selFg : fg);
            cairo_move_to(cr, r.x + 4, r.y + kPad + mSize + (kLabelH + kPad - lh) / 2);
            pango_cairo_show_layout(cr, l);
            g_object_unref(l);
        }
    }
}

gboolean ThumbGrid::pressCb(GtkWidget*, GdkEventButton* ev, gpointer self) {
    auto* g = static_cast<ThumbGrid*>(self);
    if(g->mMode == Mode::Grid) gtk_widget_grab_focus(g->mArea);
    int i = g->indexAt(ev->x, ev->y);
    if(ev->type == GDK_2BUTTON_PRESS && ev->button == 1) {
        if(i >= 0 && g->onActivate) g->onActivate(i);
        return TRUE;
    }
    if(ev->type != GDK_BUTTON_PRESS) return TRUE;
    if(ev->button == 1) {
        if(i < 0) {
            if(g->onBackground) g->onBackground();
            return TRUE;
        }
        if(g->mMode == Mode::Strip) {
            if(g->onActivate) g->onActivate(i);
        } else {
            g->setCurrent(i, false);
            if(g->onSelect) g->onSelect(i);
        }
        return TRUE;
    }
    if(ev->button == 3) {
        if(i >= 0 && g->mMode == Mode::Grid) {
            g->setCurrent(i, false);
            if(g->onSelect) g->onSelect(i);
        }
        if(g->onRightClick) g->onRightClick(i, ev);
        return TRUE;
    }
    return FALSE;
}

gboolean ThumbGrid::motionCb(GtkWidget*, GdkEventMotion* ev, gpointer self) {
    auto* g = static_cast<ThumbGrid*>(self);
    int i = g->indexAt(ev->x, ev->y);
    if(i != g->mHover) {
        g->mHover = i;
        gtk_widget_queue_draw(g->mArea);
    }
    return FALSE;
}

gboolean ThumbGrid::leaveCb(GtkWidget*, GdkEventCrossing*, gpointer self) {
    auto* g = static_cast<ThumbGrid*>(self);
    if(g->mHover != -1) {
        g->mHover = -1;
        gtk_widget_queue_draw(g->mArea);
    }
    return FALSE;
}

gboolean ThumbGrid::scrollCb(GtkWidget*, GdkEventScroll* ev, gpointer self) {
    auto* g = static_cast<ThumbGrid*>(self);
    if(g->mMode != Mode::Strip) return FALSE;  // the scrolled window handles the grid
    // Vertical wheel scrolls the strip horizontally.
    GtkAdjustment* a = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(g->mScroll));
    double delta = 0;
    if(ev->direction == GDK_SCROLL_SMOOTH)
        delta = (std::abs(ev->delta_y) > std::abs(ev->delta_x) ? ev->delta_y : ev->delta_x) * g->cellW() * 0.6;
    else if(ev->direction == GDK_SCROLL_DOWN || ev->direction == GDK_SCROLL_RIGHT)
        delta = g->cellW();
    else
        delta = -g->cellW();
    gtk_adjustment_set_value(a, gtk_adjustment_get_value(a) + delta);
    return TRUE;
}

void ThumbGrid::moveSelection(int delta) {
    if(mItems.empty()) return;
    int n = std::clamp(mCurrent < 0 ? 0 : mCurrent + delta, 0, (int)mItems.size() - 1);
    setCurrent(n, true);
    if(onSelect) onSelect(n);
}

gboolean ThumbGrid::keyCb(GtkWidget*, GdkEventKey* ev, gpointer self) {
    auto* g = static_cast<ThumbGrid*>(self);
    if(g->mMode != Mode::Grid) return FALSE;
    if(ev->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_SHIFT_MASK)) return FALSE;
    int rowsPerPage = std::max(1, gtk_widget_get_allocated_height(g->mScroll) / g->cellH());
    switch(ev->keyval) {
        case GDK_KEY_Left: g->moveSelection(-1); return TRUE;
        case GDK_KEY_Right: g->moveSelection(1); return TRUE;
        case GDK_KEY_Up: g->moveSelection(-g->mCols); return TRUE;
        case GDK_KEY_Down: g->moveSelection(g->mCols); return TRUE;
        case GDK_KEY_Page_Up: g->moveSelection(-g->mCols * rowsPerPage); return TRUE;
        case GDK_KEY_Page_Down: g->moveSelection(g->mCols * rowsPerPage); return TRUE;
        case GDK_KEY_Home: g->moveSelection(-(int)g->mItems.size()); return TRUE;
        case GDK_KEY_End: g->moveSelection((int)g->mItems.size()); return TRUE;
        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
            if(g->mCurrent >= 0 && g->onActivate) g->onActivate(g->mCurrent);
            return TRUE;
        default: return FALSE;
    }
}

}  // namespace qimgv
