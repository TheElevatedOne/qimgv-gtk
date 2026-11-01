// Modal dialogs (undecorated, the window manager draws nothing on Hyprland and the app adds its
// own title row) and file choosers.
#pragma once

#include <gtk/gtk.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "settings.hpp"

namespace qimgv {

// Creates an undecorated modal dialog with a bold title row; returns the content box.
GtkWidget* makeDialog(GtkWindow* parent, const char* title, GtkWidget** content);

bool askConfirm(GtkWindow* parent, const std::string& title, const std::string& message, const char* okLabel);
enum class ReplaceChoice { Replace, Skip, Cancel };
ReplaceChoice askReplace(GtkWindow* parent, const std::string& fileName);
std::optional<std::pair<int, int>> askResize(GtkWindow* parent, int w, int h);

std::string chooseOpenFile(GtkWindow* parent, const std::string& startDir);
std::string chooseSaveFile(GtkWindow* parent, const std::string& suggestedPath);
std::string chooseFolder(GtkWindow* parent, const std::string& title, const std::string& startDir);
std::string chooseExecutable(GtkWindow* parent);

// Key capture + action selection. Returns (sequence, action).
std::optional<std::pair<std::string, std::string>> askShortcut(GtkWindow* parent, const std::string& seq,
                                                               const std::string& action);
// Name + script editor.
std::optional<std::pair<std::string, Script>> askScript(GtkWindow* parent, const std::string& name,
                                                        const Script& script);

}  // namespace qimgv
