#include "actions.hpp"

#include <gtk/gtk.h>

#include <algorithm>

namespace qimgv {

const std::vector<ActionInfo>& allActions() {
    static const std::vector<ActionInfo> list = {
        {"nextImage", "Next image"},
        {"prevImage", "Previous image"},
        {"jumpToFirst", "First image"},
        {"jumpToLast", "Last image"},
        {"nextDirectory", "Next directory"},
        {"prevDirectory", "Previous directory"},
        {"toggleFullscreen", "Toggle fullscreen"},
        {"fitWindow", "Fit window"},
        {"fitWidth", "Fit width"},
        {"fitNormal", "Original size (1:1)"},
        {"fitWindowStretch", "Fit window (stretch)"},
        {"toggleFitMode", "Switch fit mode"},
        {"zoomIn", "Zoom in"},
        {"zoomOut", "Zoom out"},
        {"zoomInCursor", "Zoom in (at cursor)"},
        {"zoomOutCursor", "Zoom out (at cursor)"},
        {"lockZoom", "Keep zoom level"},
        {"scrollUp", "Scroll up"},
        {"scrollDown", "Scroll down"},
        {"scrollLeft", "Scroll left"},
        {"scrollRight", "Scroll right"},
        {"resize", "Resize"},
        {"rotateRight", "Rotate right"},
        {"rotateLeft", "Rotate left"},
        {"flipH", "Flip horizontal"},
        {"flipV", "Flip vertical"},
        {"crop", "Crop"},
        {"discardEdits", "Discard edits"},
        {"open", "Open"},
        {"save", "Save"},
        {"saveAs", "Save as"},
        {"print", "Print"},
        {"setWallpaper", "Set as wallpaper"},
        {"removeFile", "Delete file"},
        {"moveToTrash", "Move to trash"},
        {"copyFile", "Quick copy"},
        {"moveFile", "Quick move"},
        {"renameFile", "Rename"},
        {"reloadImage", "Reload image"},
        {"copyFileClipboard", "Copy file to clipboard"},
        {"copyPathClipboard", "Copy path to clipboard"},
        {"pasteFile", "Paste file"},
        {"showInDirectory", "Show in file manager"},
        {"folderView", "Folder view"},
        {"documentView", "Document view"},
        {"toggleFolderView", "Toggle folder view"},
        {"goUp", "Parent directory"},
        {"contextMenu", "Context menu"},
        {"toggleImageInfo", "Show image info"},
        {"toggleTransparencyGrid", "Toggle transparency grid"},
        {"toggleScalingFilter", "Toggle scaling filter"},
        {"toggleFullscreenInfoBar", "Toggle info bar in fullscreen"},
        {"toggleSlideshow", "Slideshow"},
        {"toggleShuffle", "Shuffle mode"},
        {"sortByName", "Sort by name"},
        {"sortByTime", "Sort by time"},
        {"sortBySize", "Sort by size"},
        {"openSettings", "Settings"},
        {"closeFullScreenOrExit", "Exit fullscreen / exit"},
        {"exit", "Exit"},
    };
    return list;
}

std::string actionLabel(const std::string& action) {
    if(action.rfind("s:", 0) == 0) return "Script: " + action.substr(2);
    for(auto& a : allActions())
        if(action == a.name) return a.label;
    return action;
}

std::map<std::string, std::string> defaultShortcuts() {
    return {
        {"Right", "nextImage"},
        {"Left", "prevImage"},
        {"XButton2", "nextImage"},
        {"XButton1", "prevImage"},
        {"WheelDown", "nextImage"},
        {"WheelUp", "prevImage"},
        {"F", "toggleFullscreen"},
        {"F11", "toggleFullscreen"},
        {"LMB_DoubleClick", "toggleFullscreen"},
        {"MiddleButton", "exit"},
        {"Space", "toggleFitMode"},
        {"1", "fitWindow"},
        {"2", "fitWidth"},
        {"3", "fitNormal"},
        {"4", "fitWindowStretch"},
        {"R", "resize"},
        {"H", "flipH"},
        {"V", "flipV"},
        {"Ctrl+R", "rotateRight"},
        {"Ctrl+L", "rotateLeft"},
        {"Ctrl+WheelUp", "zoomInCursor"},
        {"Ctrl+WheelDown", "zoomOutCursor"},
        {"=", "zoomIn"},
        {"Ctrl+=", "zoomIn"},
        {"+", "zoomIn"},
        {"Ctrl++", "zoomIn"},
        {"-", "zoomOut"},
        {"Ctrl+-", "zoomOut"},
        {"Ctrl+Down", "zoomOut"},
        {"Ctrl+Up", "zoomIn"},
        {"Alt+Up", "zoomIn"},
        {"Alt+Down", "zoomOut"},
        {"Up", "scrollUp"},
        {"Down", "scrollDown"},
        {"Ctrl+O", "open"},
        {"Ctrl+S", "save"},
        {"Ctrl+Shift+S", "saveAs"},
        {"Ctrl+W", "setWallpaper"},
        {"X", "crop"},
        {"Ctrl+P", "print"},
        {"Alt+X", "exit"},
        {"Ctrl+Q", "exit"},
        {"Esc", "closeFullScreenOrExit"},
        {"Del", "moveToTrash"},
        {"Shift+Del", "removeFile"},
        {"C", "copyFile"},
        {"M", "moveFile"},
        {"Home", "jumpToFirst"},
        {"End", "jumpToLast"},
        {"Enter", "folderView"},
        {"Backspace", "folderView"},
        {"F5", "reloadImage"},
        {"Ctrl+C", "copyFileClipboard"},
        {"Ctrl+Shift+C", "copyPathClipboard"},
        {"Ctrl+V", "pasteFile"},
        {"F2", "renameFile"},
        {"RMB", "contextMenu"},
        {"Menu", "contextMenu"},
        {"I", "toggleImageInfo"},
        {"Ctrl+`", "toggleShuffle"},
        {"Ctrl+D", "showInDirectory"},
        {"`", "toggleSlideshow"},
        {"Ctrl+Z", "discardEdits"},
        {"Shift+Right", "nextDirectory"},
        {"Shift+Left", "prevDirectory"},
        {"Shift+F", "toggleFullscreenInfoBar"},
        {"Ctrl+,", "openSettings"},
        {"P", "openSettings"},
    };
}

std::string modifierPrefix(guint state) {
    std::string s;
    if(state & GDK_CONTROL_MASK) s += "Ctrl+";
    if(state & GDK_MOD1_MASK) s += "Alt+";
    if(state & GDK_SHIFT_MASK) s += "Shift+";
    if(state & GDK_SUPER_MASK) s += "Super+";
    return s;
}

bool isModifierKey(const GdkEventKey* ev) {
    switch(ev->keyval) {
        case GDK_KEY_Shift_L: case GDK_KEY_Shift_R: case GDK_KEY_Control_L: case GDK_KEY_Control_R:
        case GDK_KEY_Alt_L: case GDK_KEY_Alt_R: case GDK_KEY_Meta_L: case GDK_KEY_Meta_R:
        case GDK_KEY_Super_L: case GDK_KEY_Super_R: case GDK_KEY_ISO_Level3_Shift: case GDK_KEY_Caps_Lock:
        case GDK_KEY_Num_Lock: case GDK_KEY_Hyper_L: case GDK_KEY_Hyper_R:
            return true;
        default:
            return false;
    }
}

static std::string keyName(guint kv, bool& shiftImplied) {
    shiftImplied = false;
    switch(kv) {
        case GDK_KEY_Left: case GDK_KEY_KP_Left: return "Left";
        case GDK_KEY_Right: case GDK_KEY_KP_Right: return "Right";
        case GDK_KEY_Up: case GDK_KEY_KP_Up: return "Up";
        case GDK_KEY_Down: case GDK_KEY_KP_Down: return "Down";
        case GDK_KEY_Escape: return "Esc";
        case GDK_KEY_Delete: case GDK_KEY_KP_Delete: return "Del";
        case GDK_KEY_Insert: case GDK_KEY_KP_Insert: return "Ins";
        case GDK_KEY_Return: case GDK_KEY_KP_Enter: case GDK_KEY_ISO_Enter: return "Enter";
        case GDK_KEY_BackSpace: return "Backspace";
        case GDK_KEY_space: case GDK_KEY_KP_Space: return "Space";
        case GDK_KEY_Tab: case GDK_KEY_ISO_Left_Tab: return "Tab";
        case GDK_KEY_Home: case GDK_KEY_KP_Home: return "Home";
        case GDK_KEY_End: case GDK_KEY_KP_End: return "End";
        case GDK_KEY_Page_Up: case GDK_KEY_KP_Page_Up: return "PgUp";
        case GDK_KEY_Page_Down: case GDK_KEY_KP_Page_Down: return "PgDown";
        case GDK_KEY_Menu: return "Menu";
        case GDK_KEY_Print: return "Print";
        case GDK_KEY_Pause: return "Pause";
        case GDK_KEY_KP_Add: shiftImplied = true; return "+";
        case GDK_KEY_KP_Subtract: return "-";
        case GDK_KEY_KP_Multiply: shiftImplied = true; return "*";
        case GDK_KEY_KP_Divide: return "/";
        case GDK_KEY_KP_Decimal: return ".";
        case GDK_KEY_dead_grave: return "`";
        default: break;
    }
    if(kv >= GDK_KEY_F1 && kv <= GDK_KEY_F35) return "F" + std::to_string(kv - GDK_KEY_F1 + 1);
    if(kv >= GDK_KEY_KP_0 && kv <= GDK_KEY_KP_9) return std::string(1, (char)('0' + kv - GDK_KEY_KP_0));
    gunichar c = gdk_keyval_to_unicode(kv);
    if(!c || g_unichar_iscntrl(c)) return {};
    if(g_unichar_isalpha(c)) {
        c = g_unichar_toupper(c);
    } else if(!g_unichar_isdigit(c)) {
        // Symbols: the character already reflects Shift ("+" rather than "Shift+=").
        shiftImplied = true;
    }
    char buf[8] = {0};
    g_unichar_to_utf8(c, buf);
    return buf;
}

std::string keySequence(const GdkEventKey* ev, bool layoutIndependent) {
    guint kv = ev->keyval;
    guint state = ev->state & gtk_accelerator_get_default_mod_mask();
    if(layoutIndependent) {
        guint mapped = 0;
        GdkModifierType consumed;
        GdkKeymap* km = gdk_keymap_get_for_display(gdk_display_get_default());
        if(gdk_keymap_translate_keyboard_state(km, ev->hardware_keycode, (GdkModifierType)(state & GDK_SHIFT_MASK), 0,
                                               &mapped, nullptr, nullptr, &consumed))
            kv = mapped;
    }
    bool shiftImplied = false;
    std::string name = keyName(kv, shiftImplied);
    if(name.empty()) return {};
    if(shiftImplied) state &= ~GDK_SHIFT_MASK;
    return modifierPrefix(state) + name;
}

std::string buttonSequence(guint button, guint state, bool doubleClick) {
    state &= gtk_accelerator_get_default_mod_mask();
    std::string name;
    switch(button) {
        case 1: name = "LMB"; break;
        case 2: name = "MiddleButton"; break;
        case 3: name = "RMB"; break;
        case 8: name = "XButton1"; break;
        case 9: name = "XButton2"; break;
        default: return {};
    }
    if(doubleClick) name += "_DoubleClick";
    return modifierPrefix(state) + name;
}

std::string scrollSequence(GdkScrollDirection dir, guint state) {
    state &= gtk_accelerator_get_default_mod_mask();
    const char* n = nullptr;
    switch(dir) {
        case GDK_SCROLL_UP: n = "WheelUp"; break;
        case GDK_SCROLL_DOWN: n = "WheelDown"; break;
        case GDK_SCROLL_LEFT: n = "WheelLeft"; break;
        case GDK_SCROLL_RIGHT: n = "WheelRight"; break;
        default: return {};
    }
    return modifierPrefix(state) + n;
}

}  // namespace qimgv
