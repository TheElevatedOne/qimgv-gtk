// Application settings, stored in $XDG_CONFIG_HOME/qimgv/qimgv-gtk.conf (GKeyFile).
#pragma once

#include <map>
#include <string>
#include <vector>

namespace qimgv {

enum class FitMode { Window = 0, Width = 1, Normal = 2, Stretch = 3, Free = 4 };
enum class SortMode { Name = 0, NameDesc, Size, SizeDesc, TimeOldest, TimeNewest };
enum class FolderEndAction { Nothing = 0, Loop = 1, NextFolder = 2 };
enum class ScalingFilter { Nearest = 0, Bilinear = 1, Quality = 2 };
enum class ZoomIndicator { Off = 0, On = 1, Auto = 2 };
enum class FocusPoint { Top = 0, Center = 1, Cursor = 2 };
enum class ViewMode { Document = 0, Folder = 1 };
enum class PanelPosition { Top = 0, Bottom = 1 };

struct Script {
    std::string command;  // shell command (with %file%) or path to an executable script
    bool isFile = false;
};

struct Settings {
    // General
    ViewMode defaultViewMode = ViewMode::Document;
    SortMode sortMode = SortMode::Name;
    FolderEndAction folderEndAction = FolderEndAction::Nothing;
    bool confirmDelete = true;
    bool confirmTrash = false;
    bool showSaveOverlay = true;
    bool usePreloader = true;
    int slideshowInterval = 3000;  // ms
    bool cursorAutohide = true;
    bool fullscreenOnStart = false;
    bool windowTitleExtendedInfo = true;
    int jpegQuality = 95;
    // View
    FitMode fitMode = FitMode::Window;
    bool expandImage = false;
    int expandLimit = 2;  // max upscale factor when expanding, 0 = unlimited
    double zoomStep = 0.2;
    bool transparencyGrid = false;
    ZoomIndicator zoomIndicator = ZoomIndicator::Auto;
    FocusPoint focusPoint = FocusPoint::Cursor;
    bool infoBarWindowed = true;
    bool infoBarFullscreen = false;
    bool panelEnabled = true;
    bool panelPinned = false;
    bool panelFullscreenOnly = false;
    PanelPosition panelPosition = PanelPosition::Top;
    int panelThumbSize = 120;
    bool squareThumbnails = false;
    bool wheelScrollsZoomed = false;  // mouse wheel scrolls images larger than the window
    bool clickableEdges = false;
    bool mapOverlay = true;
    bool keepZoom = false;  // "lockZoom"
    // Scaling
    ScalingFilter scalingFilter = ScalingFilter::Quality;
    bool smoothUpscaling = true;
    bool smoothAnimated = true;
    // Folder view
    int folderViewIconSize = 160;
    bool placesPanel = true;
    int placesPanelWidth = 230;
    std::vector<std::string> bookmarks;
    // Copy / move destinations (9 slots)
    std::vector<std::string> savedPaths;
    // Advanced
    bool useThumbnailCache = true;
    int thumbnailerThreads = 4;
    bool rawEmbeddedPreview = false;
    bool animations = true;
    // Window state
    int windowWidth = 1100, windowHeight = 700;
    bool maximized = false;
    // Shortcuts: key sequence -> action
    std::map<std::string, std::string> shortcuts;
    std::map<std::string, Script> scripts;

    void load();
    void save() const;
    static std::string configDir();
    static std::string cacheDir();
    static std::string thumbnailDir();
};

Settings& settings();

}  // namespace qimgv
