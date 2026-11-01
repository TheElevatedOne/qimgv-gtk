// Main window: document view (viewer + overlays + panels) and folder view, plus all actions.
#pragma once

#include <gtk/gtk.h>

#include <memory>
#include <string>
#include <vector>

#include "core/directorymodel.hpp"
#include "core/loader.hpp"
#include "core/thumbnailer.hpp"
#include "gui/contextmenu.hpp"
#include "gui/croppanel.hpp"
#include "gui/folderview.hpp"
#include "gui/mainpanel.hpp"
#include "gui/overlays.hpp"
#include "gui/viewer.hpp"

namespace qimgv {

class MainWindow {
public:
    MainWindow();
    ~MainWindow();
    void open(const std::string& path);
    void show();
    // Testing aid: run comma separated actions, render the window to a PNG and quit
    // (QIMGV_SNAPSHOT=out.png [QIMGV_SNAPSHOT_ACTIONS=a,b,c]).
    void scheduleSnapshot(const std::string& png, const std::string& actions);

private:
    // construction / layout (mainwindow.cpp)
    void buildUi();
    void placePanel();
    void updatePanelVisibility(double x, double y);
    void updateChrome();
    void updateInfo();
    void updateTitle();
    void applySettings();
    void saveWindowState();
    void showMessage(const std::string& text, const char* icon = nullptr, bool error = false);

    // input
    bool handleKey(GdkEventKey* ev);
    bool handleSequence(const std::string& seq, double x, double y);

    // directory / navigation
    bool setDirectory(const std::string& dir);
    void refreshLists();
    void onDirectoryChanged();
    void loadIndex(int index);
    void showLoaded(const std::string& path, ImagePtr img, const std::string& error);
    void preloadNeighbours();
    void navigate(int step);
    void jumpTo(int index);
    void changeDirectory(int step);
    void setMode(ViewMode m);
    void folderActivate(int itemIndex);
    void folderSelect(int itemIndex);
    int currentIndex() const { return mIndex; }
    const FileEntry* currentEntry() const { return mDir.at(mIndex); }

    // actions (mainwindow_actions.cpp)
    void action(const std::string& name, double x = -1, double y = -1);
    bool ensureEditable();
    void markEdited();
    void rotate(int degrees);
    void flip(bool horizontal);
    void startCrop();
    void finishCrop(bool save);
    void cancelCrop();
    void resize();
    bool saveImage(const std::string& path);
    void saveAs();
    void discardEdits();
    void removeCurrent(bool trash);
    void quickCopyMove(int slot);
    void chooseSlotPath(int slot);
    void renameCurrent(const std::string& newName);
    void afterFileRemoved(const std::string& path);
    void openFileDialog();
    void copyFileToClipboard();
    void copyPathToClipboard();
    void pasteFiles();
    void print();
    void toggleFullscreen();
    void toggleSlideshow();
    void toggleShuffle();
    void toggleImageInfo();
    void updateImageInfo();
    void setSort(SortMode m);
    void showContextMenu(double x, double y);
    void runUserScript(const std::string& name);
    void closeFullscreenOrExit();
    void quit();

    static gboolean keyCb(GtkWidget*, GdkEventKey*, gpointer);
    static gboolean stateCb(GtkWidget*, GdkEventWindowState*, gpointer);
    static void dropCb(GtkWidget*, GdkDragContext*, gint, gint, GtkSelectionData*, guint, guint, gpointer);

    Thumbnailer mThumbs;
    Loader mLoader;
    DirectoryModel mDir;

    GtkWidget* mWindow = nullptr;
    GtkWidget* mStack = nullptr;
    GtkWidget* mDocBox = nullptr;
    GtkWidget* mTopSlot = nullptr;
    GtkWidget* mBottomSlot = nullptr;
    GtkWidget* mOverlay = nullptr;
    GtkWidget* mInfoBar = nullptr;
    GtkWidget* mInfoIndex = nullptr;
    GtkWidget* mInfoName = nullptr;
    GtkWidget* mInfoDetails = nullptr;
    std::unique_ptr<Viewer> mViewer;
    std::unique_ptr<MainPanel> mPanel;
    std::unique_ptr<FolderView> mFolder;
    std::unique_ptr<ContextMenu> mMenu;
    std::unique_ptr<CropPanel> mCrop;
    std::unique_ptr<FloatingMessage> mMessage;
    std::unique_ptr<CopyOverlay> mCopy;
    std::unique_ptr<RenameOverlay> mRename;
    std::unique_ptr<SaveConfirmOverlay> mSaveConfirm;
    std::unique_ptr<ImageInfoOverlay> mInfo;
    std::unique_ptr<FullscreenInfo> mFsInfo;
    std::unique_ptr<ControlsOverlay> mControls;

    int mIndex = -1;
    std::string mCurrentPath;
    std::string mLoadError;
    ImagePtr mImage;
    bool mEdited = false;
    bool mFullscreen = false;
    ViewMode mMode = ViewMode::Document;
    guint mSlideshow = 0;
    bool mShuffle = false;
    std::vector<int> mShuffleOrder;
    bool mPanelInLayout = false;
    double mPointerX = -1, mPointerY = -1;
};

}  // namespace qimgv
