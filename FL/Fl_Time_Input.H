//
// Time input header for the FLTK Plus widget set.
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
   Fl_Time_Input widget. */

#ifndef Fl_Time_Input_H
#define Fl_Time_Input_H

#include <FL/Fl_Group.H>

/**
  A text field plus a button that opens an hour, minute, and second list.

  The value is a time of day. There is no date and no time zone. A new
  widget shows the local time. The text form is \c HH:mm:ss, or \c HH:mm
  when seconds are hidden. Typed text may omit a leading zero or the seconds.

  Up and down arrows change the hour, minute, or second under the text
  cursor. The list is drawn with FLTK, so the same code runs on Windows,
  X11, macOS, and Wayland.

  Visible words default to English. Replace them on one widget, or change
  the default for every new widget. Passing null uses the default again.
*/
class FL_EXPORT Fl_Time_Input : public Fl_Group {
  class Time_Field;
  class Clock_Button;
  class Time_Popup;

  Time_Field *field_;
  Clock_Button *button_;
  Time_Popup *popup_;
  int hour_;
  int minute_;
  int second_;
  int minimum_hour_;
  int minimum_minute_;
  int minimum_second_;
  int maximum_hour_;
  int maximum_minute_;
  int maximum_second_;
  int has_time_;
  int has_minimum_;
  int has_maximum_;
  int allow_empty_;
  int show_seconds_;
  int now_set_;
  int hour_set_;
  int minute_set_;
  int second_set_;
  char now_text_[32];
  char hour_text_[32];
  char minute_text_[32];
  char second_text_[32];
  char text_[16];
  char minimum_text_[16];
  char maximum_text_[16];

  void layout_children();
  void refresh_text();
  void commit_text();
  void open_popup();
  void close_popup();
  int apply_time(int hour, int minute, int second, int notify);
  int in_range(int hour, int minute, int second) const;
  static int to_seconds(int hour, int minute, int second);

  friend class Time_Field;
  friend class Clock_Button;
  friend class Time_Popup;

public:
  Fl_Time_Input(int X, int Y, int W, int H, const char *L = 0);
  ~Fl_Time_Input() override;

  /** Set the time. Returns 0 when it is invalid or outside the limits. */
  int value(int hour, int minute, int second);
  /**
    Parse \c HH:mm:ss, \c HH:mm, \c HHMMSS, or \c HHMM.
    An empty string clears the time when that is allowed.
  */
  int value(const char *text);
  void clear_time();
  int has_time() const { return has_time_; }
  int get_time(int &hour, int &minute, int &second) const;
  const char *text() const { return text_; }

  int allow_empty() const { return allow_empty_; }
  void allow_empty(int allow);
  /** 1 shows seconds in the field and in the popup. */
  int show_seconds() const { return show_seconds_; }
  void show_seconds(int show);

  /**
    Earliest selectable time. \c 00:00:00 is a real time, so an empty
    string removes the limit. Returns 0 when the text or numbers are invalid.
  */
  int minimum_time(int hour, int minute, int second);
  int maximum_time(int hour, int minute, int second);
  int minimum_time(const char *text);
  int maximum_time(const char *text);
  int has_minimum() const { return has_minimum_; }
  int has_maximum() const { return has_maximum_; }
  int get_minimum(int &hour, int &minute, int &second) const;
  int get_maximum(int &hour, int &minute, int &second) const;
  const char *minimum_text() const { return minimum_text_; }
  const char *maximum_text() const { return maximum_text_; }

  void now_label(const char *text);
  const char *now_label() const;
  void hour_heading(const char *text);
  const char *hour_heading() const;
  void minute_heading(const char *text);
  const char *minute_heading() const;
  void second_heading(const char *text);
  const char *second_heading() const;
  static void default_now_label(const char *text);
  static const char *default_now_label();
  static void default_hour_heading(const char *text);
  static const char *default_hour_heading();
  static void default_minute_heading(const char *text);
  static const char *default_minute_heading();
  static void default_second_heading(const char *text);
  static const char *default_second_heading();

  Fl_Font textfont() const;
  void textfont(Fl_Font font);
  int textsize() const;
  void textsize(int size);
  Fl_Color textcolor() const;
  void textcolor(Fl_Color color);

  /** The local time of day. */
  static void now(int &hour, int &minute, int &second);

protected:
  void draw() override;
  void resize(int X, int Y, int W, int H) override;
};

#endif
