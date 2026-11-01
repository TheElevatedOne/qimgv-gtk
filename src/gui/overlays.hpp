// Floating widgets shown on top of the image (children of the main GtkOverlay).
#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <string>
#include <vector>

#include "image/metadata.hpp"

namespace qimgv {

// Short notification ("Copied to ...", "Saved", end-of-folder hints).
class FloatingMessage {
public:
    FloatingMessage();
    GtkWidget* widget() const { return mBox; }
    void show(const std::string& text, const char* icon = nullptr, int ms = 1500, bool error = false);
    void hide();

private:
    GtkWidget* mBox;
    GtkWidget* mIconHolder;
    GtkWidget* mLabel;
    guint mTimer = 0;
};

// Quick copy / move panel with 9 destinations (keys 1-9).
class CopyOverlay {
public:
    CopyOverlay();
    GtkWidget* widget() const { return mBox; }
    void open(bool move);
    void close();
    bool visible() const;
    bool moveMode() const { return mMove; }
    void refresh();
    std::function<void(int slot)> onPick;
    std::function<void(int slot)> onChangePath;
    std::function<void()> onClosed;

private:
    GtkWidget* mBox;
    GtkWidget* mHeader;
    GtkWidget* mRows[9];
    GtkWidget* mLabels[9];
    bool mMove = false;
};

class RenameOverlay {
public:
    RenameOverlay();
    GtkWidget* widget() const { return mBox; }
    void open(const std::string& name);
    void close();
    bool visible() const;
    std::function<void(const std::string&)> onRename;
    std::function<void()> onClosed;

private:
    GtkWidget* mBox;
    GtkWidget* mEntry;
};

class SaveConfirmOverlay {
public:
    SaveConfirmOverlay();
    GtkWidget* widget() const { return mBox; }
    void setVisible(bool on);
    std::function<void(const std::string& action)> onAction;  // save, saveAs, discardEdits

private:
    GtkWidget* mBox;
};

class ImageInfoOverlay {
public:
    ImageInfoOverlay();
    GtkWidget* widget() const { return mBox; }
    void setInfo(const InfoList& file, const InfoList& exif);
    void toggle();
    bool visible() const;
    void close();

private:
    GtkWidget* mBox;
    GtkWidget* mGrid;
};

// Fullscreen: "[3/25] name.jpg" + image details in the top left corner.
class FullscreenInfo {
public:
    FullscreenInfo();
    GtkWidget* widget() const { return mBox; }
    void setText(const std::string& title, const std::string& details);

private:
    GtkWidget* mBox;
    GtkWidget* mTitle;
    GtkWidget* mDetails;
};

// Fullscreen: folder view / settings / exit buttons in the top right corner (shown on hover).
class ControlsOverlay {
public:
    ControlsOverlay();
    GtkWidget* widget() const { return mBox; }
    std::function<void(const std::string& action)> onAction;

private:
    GtkWidget* mBox;
};

}  // namespace qimgv
