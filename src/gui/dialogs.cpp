#include "gui/dialogs.hpp"

#include <cmath>

#include "actions.hpp"
#include "core/fileops.hpp"
#include "gui/style.hpp"
#include "image/formats.hpp"

namespace qimgv {

GtkWidget* makeDialog(GtkWindow* parent, const char* title, GtkWidget** content) {
    GtkWidget* d = gtk_dialog_new();
    gtk_window_set_transient_for(GTK_WINDOW(d), parent);
    gtk_window_set_modal(GTK_WINDOW(d), TRUE);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(d), TRUE);
    gtk_window_set_decorated(GTK_WINDOW(d), FALSE);
    gtk_window_set_title(GTK_WINDOW(d), title);
    gtk_window_set_position(GTK_WINDOW(d), GTK_WIN_POS_CENTER_ON_PARENT);
    addClass(d, "qimgv");
    GtkWidget* area = gtk_dialog_get_content_area(GTK_DIALOG(d));
    gtk_container_set_border_width(GTK_CONTAINER(area), 0);
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(box), 16);
    GtkWidget* t = gtk_label_new(nullptr);
    gchar* markup = g_markup_printf_escaped("<b>%s</b>", title);
    gtk_label_set_markup(GTK_LABEL(t), markup);
    g_free(markup);
    gtk_label_set_xalign(GTK_LABEL(t), 0);
    gtk_box_pack_start(GTK_BOX(box), t, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(area), box, TRUE, TRUE, 0);
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    GtkWidget* actions = gtk_dialog_get_action_area(GTK_DIALOG(d));
    G_GNUC_END_IGNORE_DEPRECATIONS
    addClass(actions, "qimgv-dialog-buttons");
    *content = box;
    return d;
}

static int run(GtkWidget* d) {
    gtk_widget_show_all(d);
    int r = gtk_dialog_run(GTK_DIALOG(d));
    return r;
}

bool askConfirm(GtkWindow* parent, const std::string& title, const std::string& message, const char* okLabel) {
    GtkWidget* box;
    GtkWidget* d = makeDialog(parent, title.c_str(), &box);
    GtkWidget* l = gtk_label_new(message.c_str());
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_label_set_line_wrap(GTK_LABEL(l), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(l), 60);
    gtk_box_pack_start(GTK_BOX(box), l, FALSE, FALSE, 0);
    gtk_dialog_add_button(GTK_DIALOG(d), "Cancel", GTK_RESPONSE_CANCEL);
    GtkWidget* ok = gtk_dialog_add_button(GTK_DIALOG(d), okLabel, GTK_RESPONSE_OK);
    addClass(ok, "destructive-action");
    gtk_dialog_set_default_response(GTK_DIALOG(d), GTK_RESPONSE_OK);
    bool r = run(d) == GTK_RESPONSE_OK;
    gtk_widget_destroy(d);
    return r;
}

ReplaceChoice askReplace(GtkWindow* parent, const std::string& fileName) {
    GtkWidget* box;
    GtkWidget* d = makeDialog(parent, "File already exists", &box);
    std::string msg = "\"" + fileName + "\" already exists in the destination folder. Replace it?";
    GtkWidget* l = gtk_label_new(msg.c_str());
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_label_set_line_wrap(GTK_LABEL(l), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(l), 60);
    gtk_box_pack_start(GTK_BOX(box), l, FALSE, FALSE, 0);
    gtk_dialog_add_button(GTK_DIALOG(d), "Cancel", GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(d), "Skip", 2);
    gtk_dialog_add_button(GTK_DIALOG(d), "Replace", GTK_RESPONSE_OK);
    int r = run(d);
    gtk_widget_destroy(d);
    if(r == GTK_RESPONSE_OK) return ReplaceChoice::Replace;
    if(r == 2) return ReplaceChoice::Skip;
    return ReplaceChoice::Cancel;
}

namespace {
struct ResizeState {
    GtkWidget *w, *h, *keep, *pct;
    int origW, origH;
    bool block = false;
};
}  // namespace

std::optional<std::pair<int, int>> askResize(GtkWindow* parent, int w, int h) {
    GtkWidget* box;
    GtkWidget* d = makeDialog(parent, "Resize image", &box);
    ResizeState st;
    st.origW = w;
    st.origH = h;
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    st.w = gtk_spin_button_new_with_range(1, 65535, 1);
    st.h = gtk_spin_button_new_with_range(1, 65535, 1);
    st.pct = gtk_spin_button_new_with_range(1, 1000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(st.w), w);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(st.h), h);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(st.pct), 100);
    gtk_entry_set_activates_default(GTK_ENTRY(st.w), TRUE);
    gtk_entry_set_activates_default(GTK_ENTRY(st.h), TRUE);
    st.keep = gtk_check_button_new_with_label("Keep aspect ratio");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(st.keep), TRUE);
    char orig[64];
    snprintf(orig, sizeof(orig), "Original: %d x %d", w, h);
    GtkWidget* ol = gtk_label_new(orig);
    addClass(ol, "dim-label");
    gtk_label_set_xalign(GTK_LABEL(ol), 0);
    auto label = [](const char* t) {
        GtkWidget* l = gtk_label_new(t);
        gtk_label_set_xalign(GTK_LABEL(l), 1);
        return l;
    };
    gtk_grid_attach(GTK_GRID(grid), label("Width"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), st.w, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), label("Height"), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), st.h, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), label("Percent"), 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), st.pct, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), st.keep, 1, 3, 1, 1);
    gtk_box_pack_start(GTK_BOX(box), ol, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), grid, FALSE, FALSE, 0);
    g_signal_connect(st.w, "value-changed", gcb(+[](GtkSpinButton* s, gpointer p) {
                         auto* st = static_cast<ResizeState*>(p);
                         if(st->block) return;
                         st->block = true;
                         double v = gtk_spin_button_get_value(s);
                         if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(st->keep)))
                             gtk_spin_button_set_value(GTK_SPIN_BUTTON(st->h), std::max(1.0, std::round(v * st->origH / st->origW)));
                         gtk_spin_button_set_value(GTK_SPIN_BUTTON(st->pct), std::round(v * 100.0 / st->origW));
                         st->block = false;
                     }),
                     &st);
    g_signal_connect(st.h, "value-changed", gcb(+[](GtkSpinButton* s, gpointer p) {
                         auto* st = static_cast<ResizeState*>(p);
                         if(st->block) return;
                         st->block = true;
                         double v = gtk_spin_button_get_value(s);
                         if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(st->keep)))
                             gtk_spin_button_set_value(GTK_SPIN_BUTTON(st->w), std::max(1.0, std::round(v * st->origW / st->origH)));
                         st->block = false;
                     }),
                     &st);
    g_signal_connect(st.pct, "value-changed", gcb(+[](GtkSpinButton* s, gpointer p) {
                         auto* st = static_cast<ResizeState*>(p);
                         if(st->block) return;
                         st->block = true;
                         double k = gtk_spin_button_get_value(s) / 100.0;
                         gtk_spin_button_set_value(GTK_SPIN_BUTTON(st->w), std::max(1.0, std::round(st->origW * k)));
                         gtk_spin_button_set_value(GTK_SPIN_BUTTON(st->h), std::max(1.0, std::round(st->origH * k)));
                         st->block = false;
                     }),
                     &st);
    gtk_dialog_add_button(GTK_DIALOG(d), "Cancel", GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(d), "Resize", GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(GTK_DIALOG(d), GTK_RESPONSE_OK);
    std::optional<std::pair<int, int>> result;
    if(run(d) == GTK_RESPONSE_OK) {
        gtk_spin_button_update(GTK_SPIN_BUTTON(st.w));
        gtk_spin_button_update(GTK_SPIN_BUTTON(st.h));
        result = std::make_pair(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(st.w)),
                                gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(st.h)));
    }
    gtk_widget_destroy(d);
    return result;
}

// ---- file choosers ----------------------------------------------------------

static std::string runChooser(GtkFileChooserNative* fc) {
    std::string out;
    if(gtk_native_dialog_run(GTK_NATIVE_DIALOG(fc)) == GTK_RESPONSE_ACCEPT) {
        gchar* f = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc));
        if(f) out = f;
        g_free(f);
    }
    g_object_unref(fc);
    return out;
}

std::string chooseOpenFile(GtkWindow* parent, const std::string& startDir) {
    GtkFileChooserNative* fc =
        gtk_file_chooser_native_new("Open image", parent, GTK_FILE_CHOOSER_ACTION_OPEN, "_Open", "_Cancel");
    GtkFileFilter* f = gtk_file_filter_new();
    gtk_file_filter_set_name(f, "Supported images");
    for(auto& ext : supportedExtensions()) {
        std::string pat = "*." + ext;
        gtk_file_filter_add_pattern(f, pat.c_str());
        gchar* up = g_ascii_strup(pat.c_str(), -1);
        gtk_file_filter_add_pattern(f, up);
        g_free(up);
    }
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc), f);
    GtkFileFilter* all = gtk_file_filter_new();
    gtk_file_filter_set_name(all, "All files");
    gtk_file_filter_add_pattern(all, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc), all);
    if(!startDir.empty()) gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(fc), startDir.c_str());
    return runChooser(fc);
}

std::string chooseSaveFile(GtkWindow* parent, const std::string& suggestedPath) {
    GtkFileChooserNative* fc =
        gtk_file_chooser_native_new("Save as", parent, GTK_FILE_CHOOSER_ACTION_SAVE, "_Save", "_Cancel");
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(fc), TRUE);
    if(!suggestedPath.empty()) {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(fc), dirName(suggestedPath).c_str());
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(fc), baseName(suggestedPath).c_str());
    }
    return runChooser(fc);
}

std::string chooseFolder(GtkWindow* parent, const std::string& title, const std::string& startDir) {
    GtkFileChooserNative* fc = gtk_file_chooser_native_new(title.c_str(), parent, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                                           "_Select", "_Cancel");
    if(!startDir.empty()) gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(fc), startDir.c_str());
    return runChooser(fc);
}

std::string chooseExecutable(GtkWindow* parent) {
    GtkFileChooserNative* fc =
        gtk_file_chooser_native_new("Select script", parent, GTK_FILE_CHOOSER_ACTION_OPEN, "_Select", "_Cancel");
    return runChooser(fc);
}

// ---- shortcut editor --------------------------------------------------------

namespace {
struct CaptureState {
    GtkWidget* label;
    std::string seq;
};
}  // namespace

static void setCaptureLabel(CaptureState* st) {
    gchar* m = g_markup_printf_escaped("<big><b>%s</b></big>", st->seq.empty() ? "Press a key combination"
                                                                                 : st->seq.c_str());
    gtk_label_set_markup(GTK_LABEL(st->label), m);
    g_free(m);
}

std::optional<std::pair<std::string, std::string>> askShortcut(GtkWindow* parent, const std::string& seq,
                                                               const std::string& action) {
    GtkWidget* box;
    GtkWidget* d = makeDialog(parent, "Shortcut", &box);
    gtk_widget_set_size_request(d, 420, -1);
    GtkWidget* combo = gtk_combo_box_text_new();
    int active = 0, i = 0;
    for(auto& a : allActions()) {
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), a.name, a.label);
        if(action == a.name) active = i;
        i++;
    }
    for(auto& [name, s] : settings().scripts) {
        std::string id = "s:" + name;
        std::string lbl = "Script: " + name;
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), id.c_str(), lbl.c_str());
        if(action == id) active = i;
        i++;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), active);
    gtk_widget_set_can_focus(combo, FALSE);
    GtkWidget* l = gtk_label_new("Action");
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_box_pack_start(GTK_BOX(box), l, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), combo, FALSE, FALSE, 0);

    CaptureState st;
    st.seq = seq;
    GtkWidget* ev = gtk_event_box_new();
    addClass(ev, "qimgv-keycapture");
    st.label = gtk_label_new("");
    setCaptureLabel(&st);
    gtk_container_add(GTK_CONTAINER(ev), st.label);
    gtk_widget_add_events(ev, GDK_BUTTON_PRESS_MASK | GDK_SCROLL_MASK);
    GtkWidget* hint = gtk_label_new("Press keys, or click / scroll in the box for mouse buttons and wheel.");
    addClass(hint, "dim-label");
    gtk_label_set_line_wrap(GTK_LABEL(hint), TRUE);
    gtk_label_set_xalign(GTK_LABEL(hint), 0);
    gtk_box_pack_start(GTK_BOX(box), ev, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box), hint, FALSE, FALSE, 0);
    g_signal_connect(ev, "button-press-event", gcb(+[](GtkWidget*, GdkEventButton* e, gpointer p) -> gboolean {
                         auto* st = static_cast<CaptureState*>(p);
                         std::string s = buttonSequence(e->button, e->state, e->type == GDK_2BUTTON_PRESS);
                         if(!s.empty()) {
                             st->seq = s;
                             setCaptureLabel(st);
                         }
                         return TRUE;
                     }),
                     &st);
    g_signal_connect(ev, "scroll-event", gcb(+[](GtkWidget*, GdkEventScroll* e, gpointer p) -> gboolean {
                         auto* st = static_cast<CaptureState*>(p);
                         GdkScrollDirection dir = e->direction;
                         if(dir == GDK_SCROLL_SMOOTH) {
                             if(e->delta_y == 0) return TRUE;
                             dir = e->delta_y > 0 ? GDK_SCROLL_DOWN : GDK_SCROLL_UP;
                         }
                         std::string s = scrollSequence(dir, e->state);
                         if(!s.empty()) {
                             st->seq = s;
                             setCaptureLabel(st);
                         }
                         return TRUE;
                     }),
                     &st);
    g_signal_connect(d, "key-press-event", gcb(+[](GtkWidget*, GdkEventKey* e, gpointer p) -> gboolean {
                         auto* st = static_cast<CaptureState*>(p);
                         if(isModifierKey(e)) return TRUE;
                         std::string s = keySequence(e);
                         if(!s.empty()) {
                             st->seq = s;
                             setCaptureLabel(st);
                         }
                         return TRUE;  // swallow everything, including Esc / Enter
                     }),
                     &st);
    GtkWidget* cancel = gtk_dialog_add_button(GTK_DIALOG(d), "Cancel", GTK_RESPONSE_CANCEL);
    GtkWidget* ok = gtk_dialog_add_button(GTK_DIALOG(d), "OK", GTK_RESPONSE_OK);
    gtk_widget_set_can_focus(cancel, FALSE);
    gtk_widget_set_can_focus(ok, FALSE);
    std::optional<std::pair<std::string, std::string>> result;
    if(run(d) == GTK_RESPONSE_OK && !st.seq.empty()) {
        const char* id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(combo));
        if(id) result = std::make_pair(st.seq, std::string(id));
    }
    gtk_widget_destroy(d);
    return result;
}

// ---- script editor ----------------------------------------------------------

std::optional<std::pair<std::string, Script>> askScript(GtkWindow* parent, const std::string& name,
                                                        const Script& script) {
    GtkWidget* box;
    GtkWidget* d = makeDialog(parent, "Script", &box);
    gtk_widget_set_size_request(d, 480, -1);
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    GtkWidget* nameEntry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(nameEntry), name.c_str());
    gtk_widget_set_hexpand(nameEntry, TRUE);
    GtkWidget* typeCmd = gtk_radio_button_new_with_label(nullptr, "Shell command");
    GtkWidget* typeFile = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(typeCmd), "Script file");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(script.isFile ? typeFile : typeCmd), TRUE);
    GtkWidget* cmd = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(cmd), script.command.c_str());
    gtk_entry_set_placeholder_text(GTK_ENTRY(cmd), "e.g. gimp %file%");
    GtkWidget* browse = gtk_button_new_with_label("Browse...");
    g_object_set_data(G_OBJECT(browse), "entry", cmd);
    g_signal_connect(browse, "clicked", gcb(+[](GtkButton* b, gpointer p) {
                         std::string f = chooseExecutable(GTK_WINDOW(p));
                         if(!f.empty())
                             gtk_entry_set_text(GTK_ENTRY(g_object_get_data(G_OBJECT(b), "entry")), f.c_str());
                     }),
                     d);
    GtkWidget* cmdRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(cmdRow), cmd, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(cmdRow), browse, FALSE, FALSE, 0);
    GtkWidget* typeRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(typeRow), typeCmd, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(typeRow), typeFile, FALSE, FALSE, 0);
    auto label = [](const char* t) {
        GtkWidget* l = gtk_label_new(t);
        gtk_label_set_xalign(GTK_LABEL(l), 1);
        return l;
    };
    gtk_grid_attach(GTK_GRID(grid), label("Name"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), nameEntry, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), label("Type"), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), typeRow, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), label("Command"), 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), cmdRow, 1, 2, 1, 1);
    gtk_box_pack_start(GTK_BOX(box), grid, FALSE, FALSE, 0);
    GtkWidget* hint = gtk_label_new(
        "Commands: %file% is replaced with the image path.\nScript files get the path as $1 and must be executable.");
    addClass(hint, "dim-label");
    gtk_label_set_xalign(GTK_LABEL(hint), 0);
    gtk_box_pack_start(GTK_BOX(box), hint, FALSE, FALSE, 0);
    gtk_dialog_add_button(GTK_DIALOG(d), "Cancel", GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(d), "OK", GTK_RESPONSE_OK);
    std::optional<std::pair<std::string, Script>> result;
    while(run(d) == GTK_RESPONSE_OK) {
        std::string n = gtk_entry_get_text(GTK_ENTRY(nameEntry));
        Script s;
        s.command = gtk_entry_get_text(GTK_ENTRY(cmd));
        s.isFile = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(typeFile));
        if(n.empty() || s.command.empty()) continue;
        result = std::make_pair(n, s);
        break;
    }
    gtk_widget_destroy(d);
    return result;
}

}  // namespace qimgv
