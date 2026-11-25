//
// Fluid Plus widget bin: a search field and a grouped list.
// The original fluid directory does not use this file.
//
// Each row is an icon plus the widget name. Groups can be folded.
// The search field filters by name or class name.
// Except for "Code", the first row of an expanded group is the pointer.
//   Click a widget that occupies space   enter place mode and drag a red rectangle on the design window
//   Double-click                         insert at the default size into the current container
//   Click the pointer, or right-click before placing   cancel place mode
// Kinds, names, and icons come from the original widget-bin definitions, so they follow additions and removals there.

#ifndef FLUID_PLUS_TOOLBOX_H
#define FLUID_PLUS_TOOLBOX_H

class Fl_Group;
class Window_Node;

namespace fluid {

Fl_Group *make_toolbox(int x, int y, int w, int h);

// Pointer events on a design window. Returns 1 when the event was consumed.
int toolbox_form_event(int event, Window_Node *form);

// Pointer events on the design desktop. Window widgets draw their rectangle on the gray desktop.
int toolbox_desk_event(int event);

// The red rectangle currently being drawn. The design-window overlay and the desktop each draw the part they can see.
void toolbox_draw_form_band(Window_Node *form);
void toolbox_draw_desk_band();

}

#endif
