//
// Date input header for the FLTK Plus widget set.
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
   Fl_Date_Input widget. */

#ifndef Fl_Date_Input_H
#define Fl_Date_Input_H

#include <FL/Fl_Group.H>

/**
  A text field plus a button that opens a calendar.

  The value is a year, month, and day. There is no time and no time zone.
  An empty value is allowed unless allow_empty() is turned off. The text
  form is always \c YYYY-MM-DD. Typed text may also use slashes, dots, or
  eight digits.

  The calendar is drawn with FLTK. This widget does not call platform date
  controls, so the same code runs on Windows, X11, macOS, and Wayland.
*/
class FL_EXPORT Fl_Date_Input : public Fl_Group {
  class Date_Field;
  class Calendar_Button;
  class Date_Popup;

  Date_Field *field_;
  Calendar_Button *button_;
  Date_Popup *popup_;
  int year_;
  int month_;
  int day_;
  int minimum_year_;
  int minimum_month_;
  int minimum_day_;
  int maximum_year_;
  int maximum_month_;
  int maximum_day_;
  int has_date_;
  int has_minimum_;
  int has_maximum_;
  int allow_empty_;
  char text_[16];
  char minimum_text_[16];
  char maximum_text_[16];

  void layout_children();
  void refresh_text();
  void commit_text();
  void open_popup();
  void close_popup();
  int apply_date(int year, int month, int day, int notify);
  int in_range(int year, int month, int day) const;

  friend class Date_Field;
  friend class Calendar_Button;
  friend class Date_Popup;

public:
  Fl_Date_Input(int X, int Y, int W, int H, const char *L = 0);
  ~Fl_Date_Input() override;

  /** Set the date. Month is 1..12. Returns 0 when the date is invalid or outside the limits. */
  int value(int year, int month, int day);
  /**
    Parse \c YYYY-MM-DD, \c YYYY/MM/DD, \c YYYY.MM.DD, or \c YYYYMMDD.
    An empty string clears the date when that is allowed.
    Returns 0 when the text is not a usable date.
  */
  int value(const char *text);
  /** Clear the date. Does nothing when empty dates are not allowed. */
  void clear_date();
  /** Returns 1 when a date is set. */
  int has_date() const { return has_date_; }
  /** Read the current date. Returns 0 when it is empty. */
  int get_date(int &year, int &month, int &day) const;
  /** \c YYYY-MM-DD, or an empty string when no date is set. */
  const char *text() const { return text_; }

  /** 1 when the field may be cleared. */
  int allow_empty() const { return allow_empty_; }
  void allow_empty(int allow);

  /**
    Set the earliest selectable date. \c 0,0,0 removes the limit.
    Returns 0 when the date is invalid.
  */
  int minimum_date(int year, int month, int day);
  int maximum_date(int year, int month, int day);
  /** Same text forms as value(). An empty string removes the limit. */
  int minimum_date(const char *text);
  int maximum_date(const char *text);
  int has_minimum() const { return has_minimum_; }
  int has_maximum() const { return has_maximum_; }
  int get_minimum(int &year, int &month, int &day) const;
  int get_maximum(int &year, int &month, int &day) const;
  const char *minimum_text() const { return minimum_text_; }
  const char *maximum_text() const { return maximum_text_; }

  Fl_Font textfont() const;
  void textfont(Fl_Font font);
  int textsize() const;
  void textsize(int size);
  Fl_Color textcolor() const;
  void textcolor(Fl_Color color);

  /** The local calendar date. */
  static void today(int &year, int &month, int &day);

protected:
  void draw() override;
  void resize(int X, int Y, int W, int H) override;
};

#endif
