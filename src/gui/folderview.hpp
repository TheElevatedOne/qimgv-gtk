// Folder view: top bar, places panel (bookmarks + filesystem tree) and a thumbnail grid.
#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "gui/thumbgrid.hpp"
#include "settings.hpp"

namespace qimgv {

class FolderView {
public:
    explicit FolderView(Thumbnailer& thumbs);
    GtkWidget* widget() const { return mRoot; }
    ThumbGrid& grid() { return *mGrid; }

    // Items are directories followed by files; `selected` indexes into items.
    void setDirectory(const std::string& dir, std::vector<ThumbGrid::Item> items, int selected);
    void setSortMode(SortMode m);
    void setPlacesVisible(bool on);
    void refreshBookmarks();
    void focusGrid();

    std::function<void(const std::string& action)> onAction;
    std::function<void(const std::string& dir)> onDirectory;
    std::function<void(int index)> onActivate;
    std::function<void(int index)> onSelect;
    std::function<void(SortMode)> onSortChanged;
    std::function<void(int index, GdkEventButton* ev)> onRightClick;

private:
    void buildTopBar();
    void buildPlaces();
    void populateTree(GtkTreeIter* parent, const std::string& dir);
    void setTreeRoot(const std::string& root);

    GtkWidget* mRoot;
    GtkWidget* mPaned;
    GtkWidget* mPlaces;
    GtkWidget* mPathLabel;
    GtkWidget* mSort;
    GtkWidget* mSlider;
    GtkWidget* mPlacesToggle;
    GtkWidget* mBookmarks;
    GtkWidget* mTree;
    GtkTreeStore* mStore;
    std::unique_ptr<ThumbGrid> mGrid;
    std::string mDir;
    bool mBlock = false;
};

}  // namespace qimgv
