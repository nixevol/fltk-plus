//
// Fluid Plus designer shell. The original fluid directory does not use this file.

#ifndef FLUID_PLUS_DESIGNER_SHELL_H
#define FLUID_PLUS_DESIGNER_SHELL_H

class Fl_Widget;
class Fl_Window;
class Node;
class Window_Node;

namespace fluid {

// Call once after the main window exists and the language pack is loaded.
void install_designer_shell();

// True when the shell has been installed.
bool designer_ready();

// Embed a design window in the gray desktop and show it.
// The design window is a child of the desktop: the desktop clips it and it moves with the main window.
// The frame image comes from the system; see frame_skin.h.
void designer_show(Window_Node *window, bool select_in_tree);

// Rebuild window tabs from the project and sync the design area, project tree, and property page.
// Call after nodes are added or removed.
// While undo recording is suspended (loading a project, creating a widget, grouping), only the request is stored.
void designer_refresh();

// A window name or title changed. Update the tab text only; do not rebuild the property page.
void designer_refresh_tabs();

// While a name is being typed, show this text on the tab. After commit, use designer_refresh_tabs.
void designer_preview_tab(Window_Node *window, const char *text);

// Call while undo recording is suspended: a refresh is needed when it resumes. No-op otherwise.
void designer_request_refresh();

// Call when undo recording resumes. Apply a refresh that was requested while it was suspended.
void designer_resumed();

// Call before undo or redo deletes every node, so stale window pointers are dropped.
void designer_release_windows();

// When the selection changes, show the window that contains it.
void designer_reveal(Node *node);

// After a double-click in the project tree, switch to the property page and show this node.
void designer_edit_node(Node *node);

// After the design window's size, title, or border flag changes, update the frame.
// Stored project coordinates are left alone.
void designer_note_geometry();

// Once a window is embedded, its project coordinates and its position on the desktop are stored separately.
// This reads the project coordinates.
bool designer_stored_xy(const Fl_Widget *widget, int &x, int &y);

// The desktop owns the embedded window's position. When something else resizes it, replace the position
// with the one the desktop assigned.
bool designer_adjust_resize(Fl_Widget *widget, int &x, int &y);

// Change the coordinates stored in the project. Do not move the window on the desktop.
void designer_set_stored_xy(Fl_Widget *widget, int x, int y);

// Coordinates written into the project for a newly created window. The shell must already be installed.
void designer_new_window_xy(int &x, int &y);

// If the pointer is over the design desktop, place this window's frame at that point.
// Returns false when the pointer is outside the desktop.
bool designer_drop_window(Fl_Window *window);

// Place the next new window's frame at this point in the desktop viewport.
// The coordinates are viewport coordinates; the function adds the scroll offset.
void designer_expect_frame(int viewport_x, int viewport_y);
void designer_clear_expect_frame();

// Convert a user-drawn outer frame size into the window client size. Frame insets are subtracted.
void designer_drawn_window_size(int outer_w, int outer_h, int &client_w, int &client_h);

// The design window currently embedded, and the gray desktop's viewport. Null when there is none.
Fl_Window *designer_visible_form();
Fl_Window *designer_desk_viewport();

// Show a panel and bring it to the front. id is "tree", "props", "toolbox", or "code".
void designer_show_panel(const char *id);

// The design area and the code view share the center. Code mode folds the widget bin, properties, and code bar.
void designer_set_code_mode(bool on);
void designer_toggle_code_mode();
bool designer_code_mode();

// Show or hide the widget bin. Returns whether it is visible after the toggle.
bool designer_toggle_toolbox();

// Put every panel back at its default position.
void designer_reset_layout();

// Store the panel layout in preferences. Call before exit, including positions of floating windows
// moved by the system title bar.
void designer_save_layout();

}

#endif
