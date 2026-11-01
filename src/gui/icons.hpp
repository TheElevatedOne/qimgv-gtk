// qimgv's monochrome icons (compiled in as GResources), painted with the GTK theme's
// foreground colour so they follow the system theme.
#pragma once

#include <gtk/gtk.h>

namespace qimgv {

// `name` is the path below data/icons without extension, e.g. "buttons/panel/open20".
// Returns a borrowed surface with device scale set (nullptr if missing).
cairo_surface_t* iconSurface(const char* name, int scale);
// Paint the icon mask at (x, y) in logical pixels using the current source colour.
void paintIcon(cairo_t* cr, const char* name, double x, double y, int scale);
int iconLogicalSize(const char* name, int* height = nullptr);

GtkWidget* iconWidget(const char* name);
GtkWidget* iconButton(const char* name, const char* tooltip);
GtkWidget* iconToggleButton(const char* name, const char* tooltip);
// Menu style row: icon + label (+ optional shortcut text), returned as a flat GtkButton.
GtkWidget* menuRow(const char* icon, const char* label, const char* shortcut = nullptr);

GdkPixbuf* appIcon(int size);

}  // namespace qimgv
