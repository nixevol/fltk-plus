//
// The four Compile-menu actions. Generated files go only to the directory the user chose.
// The code path configured in the project is not changed.

#include "app/export_actions.h"

#include "Fluid.h"
#include "app/designer_shell.h"
#include "app/ui_locale.h"
#include "io/Code_Writer.h"
#include "io/String_Writer.h"
#include "message.h"
#include "nodes/Widget_Node.h"
#include "nodes/Window_Node.h"

#include <FL/Fl.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Window.H>
#include <FL/filename.H>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <string>
#include <vector>

namespace fluid {

namespace {

std::vector<Fl_Window *> g_preview;

std::string saved_directory(const char *key) {
  std::string saved;
  Fluid.preferences.get(key, saved, "");
  if (!saved.empty()) return saved;
  saved = Fluid.proj.projectfile_path();
  if (!saved.empty()) return saved;
  return Fluid.history.latest_project_path();
}

void remember_directory(const char *key, const std::string &directory) {
  Fluid.preferences.set(key, directory.c_str());
  Fluid.preferences.flush();
}

// Every platform stores '/'. The Windows directory chooser returns '\', so normalize before remembering.
std::string portable_directory(const std::string &directory) {
  if (directory.empty()) return directory;
  return fluid::end_with_slash(fluid::fix_separators(fl_filename_absolute_str(directory)));
}

std::string choose_directory(const char *pref_key, const char *title) {
  std::string preset = portable_directory(saved_directory(pref_key));
  const char *picked = fl_dir_chooser(ui_text(title), preset.empty() ? nullptr : preset.c_str(), 0);
  if (!picked || !picked[0]) return std::string();
  std::string directory = portable_directory(picked);
  remember_directory(pref_key, directory);
  return directory;
}

bool prepare_project() {
  Fluid.flush_text_widgets();
  if (!Fluid.proj.proj_filename.empty()) return true;
  Fluid.proj.save();
  return !Fluid.proj.proj_filename.empty();
}

void clear_live_pointers(Window_Node *window) {
  for (Node *node = window; node; node = node->next) {
    if (node != window && node->level <= window->level) break;
    if (auto *widget = dynamic_cast<Widget_Node *>(node)) widget->detach_live_widget();
  }
}

void close_preview() {
  std::vector<Fl_Window *> open = g_preview;
  g_preview.clear();
  for (Fl_Window *window : open) {
    window->hide();
    Fl::delete_widget(window);
  }
}

void preview_close_cb(Fl_Widget *widget, void *) {
  auto *window = static_cast<Fl_Window *>(widget);
  window->hide();
  auto found = std::find(g_preview.begin(), g_preview.end(), window);
  if (found != g_preview.end()) g_preview.erase(found);
  Fl::delete_widget(window);
}

Window_Node *shown_design_window() {
  Fl_Window *form = designer_visible_form();
  if (!form) return nullptr;
  for (Node *node = Fluid.proj.tree.first; node; node = node->next) {
    auto *window = dynamic_cast<Window_Node *>(node);
    if (window && window->o == form) return window;
  }
  return nullptr;
}

void show_preview(Window_Node *window, int index) {
  Fl_Group::current(nullptr);
  Fl_Widget *live = window->enter_live_mode();
  clear_live_pointers(window);
  Fl_Group::current(nullptr);
  auto *preview = dynamic_cast<Fl_Window *>(live);
  if (!preview) {
    if (live) Fl::delete_widget(live);
    return;
  }
  int screen = 0;
  if (Fluid.main_window)
    screen = Fl::screen_num(Fluid.main_window->x(), Fluid.main_window->y());
  int sx = 0, sy = 0, sw = 0, sh = 0;
  Fl::screen_work_area(sx, sy, sw, sh, screen);
  int x = sx + 48 + index * 28;
  int y = sy + 48 + index * 28;
  if (x + preview->w() > sx + sw) x = sx + sw - preview->w();
  if (y + preview->h() > sy + sh) y = sy + sh - preview->h();
  if (x < sx) x = sx;
  if (y < sy) y = sy;
  preview->position(x, y);
  preview->callback(preview_close_cb);
  preview->show();
  g_preview.push_back(preview);
}

bool write_code_into(const std::string &directory, bool announce) {
  std::string code_name = Fluid.proj.codefile_name();
  std::string header_name = Fluid.proj.headerfile_name();
  if (code_name.empty() || header_name.empty()) return false;
  fluid::io::Code_Writer writer(Fluid.proj);
  std::string code = directory + code_name;
  std::string header = directory + header_name;
  Fluid.proj.enter_project_dir();
  int ok = writer.write_code(code, header);
  Fluid.proj.leave_project_dir();
  if (!ok) {
    fluid_alert(ui_text("Can't write %s or %s: %s"), code.c_str(), header.c_str(), strerror(errno));
    return false;
  }
  if (announce) fluid_message(ui_text("Wrote %s and %s"), code_name.c_str(), header_name.c_str());
  return true;
}

bool write_strings_into(const std::string &directory, bool announce) {
  std::string name = Fluid.proj.stringsfile_name();
  if (name.empty()) return false;
  std::string filename = directory + name;
  if (fluid::io::write_strings(Fluid.proj, filename)) {
    fluid_alert(ui_text("Can't write %s: %s"), filename.c_str(), strerror(errno));
    return false;
  }
  if (announce) fluid_message(ui_text("Wrote %s"), name.c_str());
  return true;
}

}  // namespace

void export_code_files() {
  if (!prepare_project()) return;
  std::string directory = choose_directory("compile_code_dir", "Choose a folder for the source files");
  if (directory.empty()) return;
  write_code_into(directory, true);
}

void export_string_file() {
  if (!prepare_project()) return;
  std::string directory = choose_directory("compile_strings_dir", "Choose a folder for the strings file");
  if (directory.empty()) return;
  write_strings_into(directory, true);
}

void export_interface_bundle() {
  if (!prepare_project()) return;
  std::string directory = choose_directory("compile_bundle_dir", "Choose a folder for the interface files");
  if (directory.empty()) return;
  bool code_ok = write_code_into(directory, false);
  bool strings_ok = write_strings_into(directory, false);
  if (code_ok && strings_ok)
    fluid_message(ui_text("Wrote the interface files to %s"), directory.c_str());
}

void preview_design_window(Window_Node *window) {
  if (!window || !window->o) {
    fluid_message(ui_text("This project has no window to preview."));
    return;
  }
  close_preview();
  show_preview(window, 0);
}

void preview_project_windows() {
  // A context menu grabs the pointer. Until that grab is released, a preview window's show() is invisible.
  if (Fl::grab()) Fl::grab(nullptr);
  close_preview();
  std::vector<Window_Node *> windows;
  if (!designer_code_mode()) {
    if (Window_Node *current = shown_design_window()) windows.push_back(current);
  }
  if (windows.empty()) {
    for (Node *node = Fluid.proj.tree.first; node; node = node->next) {
      if (auto *window = dynamic_cast<Window_Node *>(node)) {
        if (window->o) windows.push_back(window);
      }
    }
  }
  if (windows.empty()) {
    fluid_message(ui_text("This project has no window to preview."));
    return;
  }
  for (int i = 0; i < (int)windows.size(); i++) show_preview(windows[i], i);
}

}  // namespace fluid
