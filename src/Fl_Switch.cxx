//
// Switch widget for the FLTK Plus widget set.
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

#include <FL/Fl.H>
#include <FL/Fl_Switch.H>
#include <FL/fl_draw.H>

Fl_Switch::Fl_Switch(int X, int Y, int W, int H, const char *L)
  : Fl_Button(X, Y, W, H, L), shown_(0) {
  type(FL_TOGGLE_BUTTON);
  box(FL_NO_BOX);
  selection_color(fl_rgb_color(46, 130, 220));
  align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
}

Fl_Switch::~Fl_Switch() {
  Fl::remove_timeout(tick_cb, this);
}

void Fl_Switch::tick_cb(void *widget) {
  static_cast<Fl_Switch *>(widget)->tick();
}

void Fl_Switch::tick() {
  double target = value() ? 1.0 : 0.0;
  if (shown_ < target) {
    shown_ += 0.25;
    if (shown_ > target) shown_ = target;
  } else if (shown_ > target) {
    shown_ -= 0.25;
    if (shown_ < target) shown_ = target;
  }
  redraw();
  if (shown_ != target) Fl::repeat_timeout(0.02, tick_cb, this);
}

void Fl_Switch::draw() {
  double target = value() ? 1.0 : 0.0;
  if (shown_ != target && !Fl::has_timeout(tick_cb, this))
    Fl::add_timeout(0.02, tick_cb, this);

  int track_h = h();
  if (track_h > 22) track_h = 22;
  if (track_h < 8) track_h = h();
  int track_w = track_h * 2;
  if (track_w > w()) track_w = w();
  int tx = x();
  int ty = y() + (h() - track_h) / 2;
  Fl_Color track = value() ? selection_color() : fl_color_average(FL_BLACK, color(), 0.25f);
  if (!active_r()) track = fl_inactive(track);
  fl_color(track);
  fl_rounded_rectf(tx, ty, track_w, track_h, track_h / 2);

  int inset = track_h > 8 ? 2 : 1;
  int diameter = track_h - inset * 2;
  if (diameter < 2) diameter = 2;
  int travel = track_w - inset * 2 - diameter;
  if (travel < 0) travel = 0;
  int thumb_x = tx + inset + (int)(shown_ * travel + 0.5);
  fl_color(active_r() ? FL_WHITE : fl_inactive(FL_WHITE));
  fl_pie(thumb_x, ty + inset, diameter, diameter, 0, 360);

  if (Fl::focus() == this) {
    fl_color(FL_FOREGROUND_COLOR);
    fl_rounded_rect(tx, ty, track_w, track_h, track_h / 2);
  }

  const char *caption = label();
  if (caption && caption[0]) {
    int left = tx + track_w + 6;
    int text_w = x() + w() - left;
    if (text_w > 0) {
      fl_font(labelfont(), labelsize());
      fl_color(active_r() ? labelcolor() : fl_inactive(labelcolor()));
      fl_draw(caption, left, y(), text_w, h(), FL_ALIGN_LEFT | FL_ALIGN_CLIP);
    }
  }
}
