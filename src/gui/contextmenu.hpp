// qimgv's context menu, drawn inside the window: zoom buttons, edit buttons and actions.
#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <map>
#include <string>

namespace qimgv {

class ContextMenu {
public:
    ContextMenu();
    GtkWidget* widget() const { return mBox; }
    // Show at (x, y) in overlay coordinates, kept inside `areaW` x `areaH`.
    void popup(double x, double y, int areaW, int areaH);
    void hide();
    bool visible() const;
    // Refresh shortcut hints and the scripts submenu.
    void update(const std::map<std::string, std::string>& shortcuts, const std::map<std::string, std::string>& scripts);
    std::function<void(const std::string& action)> onAction;

private:
    GtkWidget* addRow(GtkWidget* box, const char* icon, const char* label, const char* action);
    GtkWidget* mBox;
    GtkWidget* mStack;
    GtkWidget* mScriptsBox;
    std::map<std::string, GtkWidget*> mShortcutLabels;
};

}  // namespace qimgv
