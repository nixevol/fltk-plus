//
// Color button widget for the FLTK Plus widget set.
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
#include <FL/Fl_Color_Button.H>
#include <FL/Fl_Color_Chooser.H>
#include <FL/Fl_Menu_Window.H>
#include <FL/fl_draw.H>

#include <cctype>
#include <cstdio>
#include <cstdlib>

static const unsigned char kDefaultRed = 51;
static const unsigned char kDefaultGreen = 102;
static const unsigned char kDefaultBlue = 204;

class Fl_Color_Button_Popup : public Fl_Menu_Window {
  Fl_Color_Button *owner_;
  Fl_Color_Chooser *chooser_;
  int changed_;

  static void chooser_cb(Fl_Widget *widget, void *data) {
    Fl_Color_Chooser *chooser = static_cast<Fl_Color_Chooser *>(widget);
    Fl_Color_Button_Popup *popup = static_cast<Fl_Color_Button_Popup *>(data);
    int red = (int)(chooser->r() * 255.0 + 0.5);
    int green = (int)(chooser->g() * 255.0 + 0.5);
    int blue = (int)(chooser->b() * 255.0 + 0.5);
    if (red < 0) red = 0;
    if (green < 0) green = 0;
    if (blue < 0) blue = 0;
    if (red > 255) red = 255;
    if (green > 255) green = 255;
    if (blue > 255) blue = 255;
    popup->changed_ = 1;
    popup->owner_->rgb((unsigned char)red, (unsigned char)green, (unsigned char)blue);
    if (popup->owner_->when() & FL_WHEN_CHANGED)
      popup->owner_->do_callback(FL_REASON_CHANGED);
  }

public:
  Fl_Color_Button_Popup(Fl_Color_Button *owner)
    : Fl_Menu_Window(0, 0, 199, 119), owner_(owner), chooser_(0), changed_(0) {
    set_override();
    box(FL_UP_BOX);
    begin();
    chooser_ = new Fl_Color_Chooser(2, 2, 195, 115);
    chooser_->mode(1);
    chooser_->callback(chooser_cb, this);
    end();
  }

  void prepare() {
    chooser_->rgb(owner_->red() / 255.0, owner_->green() / 255.0, owner_->blue() / 255.0);
  }

  int handle(int event) override {
    if (event == FL_KEYBOARD && Fl::event_key() == FL_Escape) {
      hide();
      return 1;
    }
    // event_x/event_y are relative to this window. Fl::event_inside(this)
    // treats a window's x/y as child coordinates and rejects every click.
    if (event == FL_PUSH &&
        (Fl::event_x() < 0 || Fl::event_y() < 0 || Fl::event_x() >= w() || Fl::event_y() >= h())) {
      hide();
      return 1;
    }
    return Fl_Menu_Window::handle(event);
  }

  void hide() override {
    int notify = shown() && changed_ && (owner_->when() & FL_WHEN_RELEASE);
    changed_ = 0;
    if (Fl::grab() == this) Fl::grab(0);
    Fl_Menu_Window::hide();
    if (notify) owner_->do_callback(FL_REASON_RELEASED);
  }
};

Fl_Color_Button::Fl_Color_Button(int X, int Y, int W, int H, const char *L)
  : Fl_Button(X, Y, W, H, L),
    r_(kDefaultRed), g_(kDefaultGreen), b_(kDefaultBlue), popup_(0) {
  box(FL_DOWN_BOX);
}

Fl_Color_Button::~Fl_Color_Button() {
  close_popup();
  delete popup_;
  popup_ = 0;
}

void Fl_Color_Button::rgb(unsigned char red, unsigned char green, unsigned char blue) {
  if (r_ == red && g_ == green && b_ == blue) return;
  r_ = red;
  g_ = green;
  b_ = blue;
  redraw();
}

void Fl_Color_Button::get_rgb(unsigned char &red, unsigned char &green, unsigned char &blue) const {
  red = r_;
  green = g_;
  blue = b_;
}

void Fl_Color_Button::format(char *buffer) const {
  if (!buffer) return;
  snprintf(buffer, 8, "#%02X%02X%02X", (int)r_, (int)g_, (int)b_);
}

static int hex_digit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

int Fl_Color_Button::rgb(const char *text) {
  if (!text) return 0;
  while (*text && std::isspace((unsigned char)*text)) text++;
  if (*text == '#') text++;
  int digits[6];
  int count = 0;
  const char *p = text;
  while (count < 6 && hex_digit(*p) >= 0) {
    digits[count++] = hex_digit(*p);
    p++;
  }
  while (*p && std::isspace((unsigned char)*p)) p++;
  if (count == 6 && *p == 0) {
    rgb((unsigned char)(digits[0] * 16 + digits[1]),
        (unsigned char)(digits[2] * 16 + digits[3]),
        (unsigned char)(digits[4] * 16 + digits[5]));
    return 1;
  }
  char *end = 0;
  long red = strtol(text, &end, 10);
  if (end == text) return 0;
  while (*end && std::isspace((unsigned char)*end)) end++;
  if (*end == ',') end++;
  char *next = 0;
  long green = strtol(end, &next, 10);
  if (next == end) return 0;
  while (*next && std::isspace((unsigned char)*next)) next++;
  if (*next == ',') next++;
  long blue = strtol(next, &end, 10);
  if (end == next) return 0;
  while (*end && std::isspace((unsigned char)*end)) end++;
  if (*end != 0) return 0;
  if (red < 0 || red > 255 || green < 0 || green > 255 || blue < 0 || blue > 255) return 0;
  rgb((unsigned char)red, (unsigned char)green, (unsigned char)blue);
  return 1;
}

void Fl_Color_Button::close_popup() {
  if (!popup_) return;
  popup_->hide();
}

void Fl_Color_Button::open_popup() {
  if (!active()) return;
  if (popup_ && popup_->shown()) {
    close_popup();
    return;
  }
  if (!popup_) {
    Fl_Group *saved = Fl_Group::current();
    Fl_Group::current(0);
    popup_ = new Fl_Color_Button_Popup(this);
    Fl_Group::current(saved);
  }
  popup_->prepare();
  // Child coordinates are already relative to the enclosing window.
  int sx = x();
  int sy = y();
  for (Fl_Window *win = window(); win; win = win->window()) {
    sx += win->x();
    sy += win->y();
  }
  int area_x, area_y, area_w, area_h;
  int screen = 0;
  if (window()) screen = window()->screen_num();
  Fl::screen_work_area(area_x, area_y, area_w, area_h, screen);
  int px = sx;
  int py = sy + h();
  if (py + popup_->h() > area_y + area_h) py = sy - popup_->h();
  if (px + popup_->w() > area_x + area_w) px = area_x + area_w - popup_->w();
  if (px < area_x) px = area_x;
  if (py < area_y) py = area_y;
  popup_->position(px, py);
  popup_->show();
  Fl::grab(popup_);
}

void Fl_Color_Button::draw() {
  draw_box();
  int dx = Fl::box_dx(box());
  int dy = Fl::box_dy(box());
  int dw = Fl::box_dw(box());
  int dh = Fl::box_dh(box());
  int sw = w() - dw;
  int sh = h() - dh;
  if (sw < 1) sw = 1;
  if (sh < 1) sh = 1;
  Fl_Color swatch = fl_rgb_color(r_, g_, b_);
  if (!active_r()) swatch = fl_inactive(swatch);
  fl_color(swatch);
  fl_rectf(x() + dx, y() + dy, sw, sh);
  if (Fl::focus() == this) {
    fl_color(FL_FOREGROUND_COLOR);
    fl_rect(x() + dx, y() + dy, sw, sh);
  }
  if (label() && label()[0]) draw_label();
}

int Fl_Color_Button::handle(int event) {
  if (event == FL_PUSH && Fl::event_button() == FL_LEFT_MOUSE) {
    take_focus();
    open_popup();
    return 1;
  }
  if (event == FL_KEYBOARD) {
    int key = Fl::event_key();
    if (key == ' ' || key == FL_Enter || key == FL_KP_Enter) {
      open_popup();
      return 1;
    }
  }
  if (event == FL_FOCUS || event == FL_UNFOCUS) return 1;
  return Fl_Button::handle(event);
}
