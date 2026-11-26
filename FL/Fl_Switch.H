//
// Switch header for the FLTK Plus widget set.
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
   Fl_Switch widget. */

#ifndef Fl_Switch_H
#define Fl_Switch_H

#include <FL/Fl_Button.H>

/**
  An on/off switch.

  value() is 0 when off and 1 when on. The track uses color() when off and
  selection_color() when on. The label, if any, is drawn to the right of
  the track. Clicking or pressing Space toggles the value.

  The control is drawn with FLTK, so the same code runs on Windows, X11,
  macOS, and Wayland.
*/
class FL_EXPORT Fl_Switch : public Fl_Button {
  double shown_;
  static void tick_cb(void *widget);
  void tick();

protected:
  void draw() override;

public:
  Fl_Switch(int x, int y, int w, int h, const char *label = 0);
  ~Fl_Switch();
};

#endif
