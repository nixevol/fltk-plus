//
// Frame image for a design window. The original fluid directory does not use this file.
//
// A design window sits inside the design area as a borderless child, so the area clips it
// and it moves with the main window.
// The frame is not drawn by hand. It is captured from a real system window, so each system
// shows its own title bar and border:
//   Windows      a real window is hidden and the system paints it, title bar included, into memory.
//                The user never sees that window.
//   Other systems  FLTK's per-platform capture. A real window is shown briefly.
//   If neither works  a neutral frame is drawn so there is always a border.
//
// FLUID_PLUS_FRAME=plain forces the drawn frame. =fltk forces FLTK capture.
// Use these on one machine to compare the three sources.

#ifndef FLUID_PLUS_FRAME_SKIN_H
#define FLUID_PLUS_FRAME_SKIN_H

#include <string>

class Fl_Window;

namespace fluid {

struct Frame_Request {
  std::string title;          // design window title
  int w = 0;                  // design window client size, in FLTK units
  int h = 0;
  int screen_x = 0;           // screen position of the design window's client origin
  int screen_y = 0;           // where a temporary window appears, exactly over the frame
  Fl_Window *owner = nullptr; // main window, used for the screen and scale
};

// Ask for a frame with the same title and size as the design window.
// It may be ready immediately or later. When it arrives later, the function registered
// with frame_skin_on_change is called.
void frame_skin_request(const Frame_Request &request);

// Extra width the frame adds around the client area, in FLTK units.
void frame_skin_insets(int &left, int &top, int &right, int &bottom);

// Draw the frame at x,y,w,h. w and h include the frame.
void frame_skin_draw(int x, int y, int w, int h, const char *title);

// px is the distance from the left of the frame along the title bar.
// Returns true when it lands on the minimize, maximize, or close button.
bool frame_skin_inert(int px, int w);

// True when capturing a frame neither shows a window nor takes long.
// While resizing, every step can capture a new frame.
bool frame_skin_live();

// True when the system appearance changed (theme, colors, dark mode). Clears the flag.
bool frame_skin_take_stale();

// Called when a frame that was requested earlier becomes ready.
void frame_skin_on_change(void (*changed)());

}

#endif
