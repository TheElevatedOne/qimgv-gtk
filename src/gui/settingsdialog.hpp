// Settings window: sidebar with qimgv's pages and OK / Apply / Cancel.
#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <memory>

namespace qimgv {

class SettingsDialog {
public:
    // Opens the window (non-blocking). `onApply` runs after settings were changed and saved.
    static void open(GtkWindow* parent, std::function<void()> onApply);
};

}  // namespace qimgv
