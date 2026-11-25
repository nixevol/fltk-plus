//
// Dockable panels. The original fluid directory does not use this file.
//
// The host is a split tree. A leaf is either the fixed center (the design area) or a panel group.
// A group has a caption. When it holds more than one panel, tabs sit below the content.
//
//   Drag the caption          move the whole group
//   Drag a tab                move that one panel
//   Drop on the middle        join that group as a tab
//   Drop on an edge           dock on that side
//   Drop on a host edge       span that whole edge
//   Drop elsewhere            become a floating window
//   Double-click the caption  toggle between docked and floating
//   Caption fold button       collapse to the edge, click again to expand.
//                             Left and right collapse to a vertical strip; top and bottom keep the caption.
//
// A floating window is an ordinary system window, moved and resized by the system title bar.
// Dragging a panel caption inside it can dock it again, and closing the window docks it too.
// Everything uses portable FLTK interfaces. Wayland does not tell the program where a window is
// on the screen, so a float cannot be dragged back onto the main window there. Double-click the
// caption or close the floating window instead.

#ifndef FLUID_PLUS_DOCK_H
#define FLUID_PLUS_DOCK_H

#include <FL/Fl_Group.H>

#include <string>

namespace fluid {

namespace dock {
struct Model;
}

enum class Dock_Side { LEFT, RIGHT, TOP, BOTTOM };

// Where a panel goes by default. Reset, closing a float, and showing a hidden panel return here.
struct Dock_Home {
  enum Where {
    ROOT,    // along one edge of the host
    CENTER,  // along one edge of the center
    TAB      // grouped with another panel
  };
  Where where = ROOT;
  Dock_Side side = Dock_Side::LEFT;  // where a TAB panel goes when its partner is absent
  const char *with = nullptr;        // the other panel's id when where is TAB
  int size = 240;                    // extent along the dock direction, in FLTK units
};

class Dock_Host : public Fl_Group {
public:
  Dock_Host(int x, int y, int w, int h);

  // The fixed center. It cannot be dragged away.
  void center(Fl_Widget *widget);

  // Register a panel. id is stored in the saved layout. title is the English source and is translated for display.
  // Call restore after registering before the panel is placed.
  void add_panel(const char *id, const char *title, Fl_Widget *content, const Dock_Home &home);

  // Arrange panels from a saved layout. An empty or unreadable string uses the default layout.
  void restore(const std::string &saved);
  std::string save() const;
  void reset();

  // Show the panel and make it the current page of its group.
  // A hidden panel returns to its default place. A floating one is raised.
  void show_panel(const char *id);
  void hide_panel(const char *id);
  // The panel is not hidden.
  bool panel_shown(const char *id) const;
  // The panel is in front: not hidden, not collapsed, and the current page of its group.
  bool panel_front(const char *id) const;
  // Whether this group is collapsed to an edge strip. False when the panel is not shown.
  bool panel_collapsed(const char *id) const;
  // Collapse or expand a group that is already shown.
  void fold_panel(const char *id, bool on);

  // Called after the layout changes. Used to save the layout.
  void on_change(void (*changed)());

  void resize(int x, int y, int w, int h) override;
  int handle(int event) override;

private:
  dock::Model *model_;
};

}

#endif
