// CSS for qimgv's widgets. All colours come from the GTK theme.
#pragma once

#include <gtk/gtk.h>

namespace qimgv {

void installStyle();
// Lookup a named theme colour on a widget ("theme_bg_color", ...), with a fallback.
GdkRGBA themeColor(GtkWidget* w, const char* name, const char* fallback);
void addClass(GtkWidget* w, const char* cls);

// Function (not macro) replacement for G_CALLBACK so lambdas containing commas work.
template <typename F>
inline GCallback gcb(F fn) {
    return reinterpret_cast<GCallback>(fn);
}

}  // namespace qimgv
