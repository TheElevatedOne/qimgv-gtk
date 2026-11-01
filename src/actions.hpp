// Action names, default shortcuts and conversion of GDK input events to qimgv key sequences
// ("Ctrl+Shift+S", "WheelUp", "LMB_DoubleClick", "XButton1", ...), the same scheme qimgv uses.
#pragma once

#include <gdk/gdk.h>

#include <map>
#include <string>
#include <vector>

namespace qimgv {

struct ActionInfo {
    const char* name;
    const char* label;
};

const std::vector<ActionInfo>& allActions();
std::string actionLabel(const std::string& action);  // also handles "s:scriptname"
std::map<std::string, std::string> defaultShortcuts();

std::string modifierPrefix(guint state);
// Key event -> sequence. `layoutIndependent` maps through the first keyboard layout group
// (so "1" works on layouts where the digit row needs Shift).
std::string keySequence(const GdkEventKey* ev, bool layoutIndependent = false);
std::string buttonSequence(guint button, guint state, bool doubleClick);
std::string scrollSequence(GdkScrollDirection dir, guint state);
// True if the key is a lone modifier press.
bool isModifierKey(const GdkEventKey* ev);

}  // namespace qimgv
