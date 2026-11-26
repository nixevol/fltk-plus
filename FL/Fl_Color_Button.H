//
// Color button header for the FLTK Plus widget set.
//
// Copyright 2026 by Bill Spitzak and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file.  If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//
// Please see the following page on how to report bugs and issues:
//
//     https://www.fltk.org/bugs.php
//

/* \file
   Fl_Color_Button widget. */

#ifndef Fl_Color_Button_H
#define Fl_Color_Button_H

#include <FL/Fl_Button.H>

class Fl_Color_Button_Popup;

/**
  A button that shows a color and opens a color chooser.

  The swatch is the stored red, green, and blue. The widget color() is only
  the frame around the swatch. Clicking opens an Fl_Color_Chooser. Dragging
  in the chooser updates the swatch immediately. Clicking outside or pressing
  Escape closes it.

  The chooser is drawn with FLTK. This widget does not call a platform color
  dialog, so the same code runs on Windows, X11, macOS, and Wayland.

  The default color is #3366CC. rgb(const char *) accepts "#RRGGBB",
  "RRGGBB", or three numbers "R G B".
*/
class FL_EXPORT Fl_Color_Button : public Fl_Button {
  friend class Fl_Color_Button_Popup;
  unsigned char r_, g_, b_;
  Fl_Color_Button_Popup *popup_;

  void open_popup();
  void close_popup();

protected:
  void draw() override;
  int handle(int event) override;

public:
  Fl_Color_Button(int x, int y, int w, int h, const char *label = 0);
  ~Fl_Color_Button();

  unsigned char red() const { return r_; }
  unsigned char green() const { return g_; }
  unsigned char blue() const { return b_; }
  void rgb(unsigned char red, unsigned char green, unsigned char blue);
  int rgb(const char *text);
  void get_rgb(unsigned char &red, unsigned char &green, unsigned char &blue) const;
  /** Writes "#RRGGBB" into buffer. buffer must hold at least 8 bytes. */
  void format(char *buffer) const;
};

#endif
