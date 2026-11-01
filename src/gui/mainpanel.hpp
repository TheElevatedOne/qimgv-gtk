// Thumbnail panel that slides in at the top (or bottom) edge of the window: a strip of
// thumbnails plus qimgv's button column (exit, folder view, pin, open, settings).
#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <memory>
#include <string>

#include "gui/thumbgrid.hpp"
#include "settings.hpp"

namespace qimgv {

class MainPanel {
public:
    explicit MainPanel(Thumbnailer& thumbs);
    GtkWidget* widget() const { return mRevealer; }
    ThumbGrid& strip() { return *mStrip; }
    void setPosition(PanelPosition p);
    void setPinned(bool pinned);
    void reveal(bool show);
    bool revealed() const;
    int height() const;
    std::function<void(const std::string& action)> onAction;
    std::function<void(bool pinned)> onPinToggled;

private:
    GtkWidget* mRevealer;
    GtkWidget* mBox;
    GtkWidget* mPin;
    std::unique_ptr<ThumbGrid> mStrip;
    bool mBlockPin = false;
};

}  // namespace qimgv
