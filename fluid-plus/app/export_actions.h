//
// Compile menu: write the interface into a chosen directory, and preview windows with real widgets.

#ifndef FLUID_PLUS_EXPORT_ACTIONS_H
#define FLUID_PLUS_EXPORT_ACTIONS_H

class Window_Node;

#include <string>

namespace fluid {

// Choose a directory and write the project's source and header there.
// The project's own code path is not changed.
void export_code_files();

// Choose a directory and write the strings file there.
void export_string_file();

// Choose one directory and write the source, header, and strings file together.
void export_interface_bundle();

// Open a preview built from real widgets.
// Design mode previews the current window. Code mode previews every window.
void preview_project_windows();

// Preview this one design window. The window-tab context menu uses this.
void preview_design_window(Window_Node *window);

// Show the live preview, write a PNG, and close the preview.
// Returns false when the window cannot be shown or the file cannot be written.
bool write_preview_png(Window_Node *window, const std::string &path);

}

#endif
