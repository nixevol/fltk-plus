//
// Time input widget for the FLTK Plus widget set.
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

#include <FL/Fl_Time_Input.H>

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Menu_Window.H>
#include <FL/fl_draw.H>

#include <cstdio>
#include <cstring>
#include <ctime>

static int valid_time(int hour, int minute, int second) {
  return hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59 && second >= 0 && second <= 59;
}

static int parse_time(const char *text, int &hour, int &minute, int &second, int &saw_second) {
  if (!text) return 0;
  while (*text == ' ' || *text == '\t') text++;
  if (!*text) return 0;
  int h = 0, m = 0, s = 0;
  char extra = 0;
  if (std::sscanf(text, "%d:%d:%d %c", &h, &m, &s, &extra) == 3) {
    hour = h;
    minute = m;
    second = s;
    saw_second = 1;
    return valid_time(hour, minute, second);
  }
  if (std::sscanf(text, "%d:%d %c", &h, &m, &extra) == 2) {
    hour = h;
    minute = m;
    second = 0;
    saw_second = 0;
    return valid_time(hour, minute, second);
  }
  if (std::sscanf(text, "%2d%2d%2d %c", &h, &m, &s, &extra) == 3 && std::strlen(text) >= 6) {
    hour = h;
    minute = m;
    second = s;
    saw_second = 1;
    return valid_time(hour, minute, second);
  }
  if (std::sscanf(text, "%2d%2d %c", &h, &m, &extra) == 2 && std::strlen(text) >= 4) {
    hour = h;
    minute = m;
    second = 0;
    saw_second = 0;
    return valid_time(hour, minute, second);
  }
  return 0;
}

static void format_time(char *dest, int hour, int minute, int second, int show_seconds) {
  if (show_seconds) std::snprintf(dest, 16, "%02d:%02d:%02d", hour, minute, second);
  else std::snprintf(dest, 16, "%02d:%02d", hour, minute);
}

class Fl_Time_Input::Time_Field : public Fl_Input {
  Fl_Time_Input *owner_;
  int section_at(int cursor) const {
    if (cursor <= 2) return 0;
    if (!owner_->show_seconds() || cursor <= 5) return 1;
    return 2;
  }
  void select_section(int section) {
    if (section <= 0) position(0, 2);
    else if (section == 1) position(3, 5);
    else position(6, 8);
  }
  void step_section(int direction) {
    int hour, minute, second;
    if (!owner_->get_time(hour, minute, second)) owner_->now(hour, minute, second);
    int section = section_at(position());
    if (section == 0) hour = (hour + direction + 24) % 24;
    else if (section == 1) minute = (minute + direction + 60) % 60;
    else second = (second + direction + 60) % 60;
    if (!owner_->apply_time(hour, minute, second, 1)) return;
    select_section(section);
  }
public:
  Time_Field(Fl_Time_Input *owner, int X, int Y, int W, int H)
    : Fl_Input(X, Y, W, H), owner_(owner) {
    when(FL_WHEN_NEVER);
    box(FL_FLAT_BOX);
  }
  int handle(int event) override {
    if (event == FL_KEYBOARD && (Fl::event_key() == FL_Up || Fl::event_key() == FL_Down)) {
      step_section(Fl::event_key() == FL_Up ? 1 : -1);
      return 1;
    }
    int used = Fl_Input::handle(event);
    if (event == FL_RELEASE) select_section(section_at(position()));
    if (event == FL_UNFOCUS ||
        (event == FL_KEYBOARD && (Fl::event_key() == FL_Enter || Fl::event_key() == FL_KP_Enter)))
      owner_->commit_text();
    return used;
  }
};

class Fl_Time_Input::Clock_Button : public Fl_Button {
  Fl_Time_Input *owner_;
  void draw_glyph(int X, int Y, int W, int H) {
    int side = W < H ? W : H;
    if (side < 8) return;
    int ox = X + (W - side) / 2;
    int oy = Y + (H - side) / 2;
    fl_color(FL_BLACK);
    fl_arc(ox, oy, side, side, 0, 360);
    fl_color(color());
    int inset = side / 8;
    if (inset < 1) inset = 1;
    fl_pie(ox + inset, oy + inset, side - inset * 2, side - inset * 2, 0, 360);
    fl_color(FL_BLACK);
    int cx = ox + side / 2;
    int cy = oy + side / 2;
    fl_line(cx, cy, cx - side / 5, cy - side / 6);
    fl_line(cx, cy, cx + side / 4, cy - side / 5);
  }
public:
  Clock_Button(Fl_Time_Input *owner, int X, int Y, int W, int H)
    : Fl_Button(X, Y, W, H), owner_(owner) {
    box(FL_FLAT_BOX);
  }
  void draw() override {
    Fl_Button::draw();
    draw_glyph(x() + 2, y() + 2, w() - 4, h() - 4);
  }
  int handle(int event) override {
    if (event == FL_RELEASE && Fl::event_x() >= x() && Fl::event_x() < x() + w() &&
        Fl::event_y() >= y() && Fl::event_y() < y() + h()) {
      value(0);
      if (active()) owner_->open_popup();
      return 1;
    }
    return Fl_Button::handle(event);
  }
};

class Fl_Time_Input::Time_Popup : public Fl_Menu_Window {
  Fl_Time_Input *owner_;
  Fl_Box *hour_heading_;
  Fl_Box *minute_heading_;
  Fl_Box *second_heading_;
  Fl_Hold_Browser *hour_;
  Fl_Hold_Browser *minute_;
  Fl_Hold_Browser *second_;
  Fl_Button *now_;
  int updating_;

  static void hour_cb(Fl_Widget *, void *self) { ((Time_Popup *)self)->pick_from_lists(); }
  static void minute_cb(Fl_Widget *, void *self) { ((Time_Popup *)self)->pick_from_lists(); }
  static void second_cb(Fl_Widget *, void *self) { ((Time_Popup *)self)->pick_from_lists(); }
  static void now_cb(Fl_Widget *, void *self) {
    int hour, minute, second;
    Fl_Time_Input::now(hour, minute, second);
    Time_Popup *popup = (Time_Popup *)self;
    if (popup->owner_->apply_time(hour, minute, second, 1)) popup->hide();
  }

  void fill(Fl_Hold_Browser *browser, int count) {
    browser->clear();
    for (int i = 0; i < count; i++) {
      char item[8];
      std::snprintf(item, sizeof(item), "%02d", i);
      browser->add(item);
    }
  }

  void pick_from_lists() {
    if (updating_) return;
    int hour = hour_->value() - 1;
    int minute = minute_->value() - 1;
    int second = owner_->show_seconds() ? second_->value() - 1 : 0;
    if (hour < 0 || minute < 0 || second < 0) return;
    int old_h = 0, old_m = 0, old_s = 0;
    if (!owner_->get_time(old_h, old_m, old_s)) owner_->now(old_h, old_m, old_s);
    if (!owner_->apply_time(hour, minute, second, 1)) {
      updating_ = 1;
      hour_->value(old_h + 1);
      minute_->value(old_m + 1);
      second_->value(old_s + 1);
      updating_ = 0;
    }
  }

public:
  Time_Popup(Fl_Time_Input *owner)
    : Fl_Menu_Window(0, 0, 210, 220), owner_(owner), updating_(0) {
    set_override();
    box(FL_UP_BOX);
    color(FL_BACKGROUND_COLOR);
    begin();
    hour_heading_ = new Fl_Box(8, 6, 60, 18, "Hour");
    minute_heading_ = new Fl_Box(74, 6, 60, 18, "Minute");
    second_heading_ = new Fl_Box(140, 6, 60, 18, "Second");
    hour_ = new Fl_Hold_Browser(8, 26, 60, 150);
    minute_ = new Fl_Hold_Browser(74, 26, 60, 150);
    second_ = new Fl_Hold_Browser(140, 26, 60, 150);
    hour_->textsize(12);
    minute_->textsize(12);
    second_->textsize(12);
    hour_->callback(hour_cb, this);
    minute_->callback(minute_cb, this);
    second_->callback(second_cb, this);
    fill(hour_, 24);
    fill(minute_, 60);
    fill(second_, 60);
    now_ = new Fl_Button(8, 184, 194, 26, "Now");
    now_->callback(now_cb, this);
    end();
  }

  ~Time_Popup() override {
    if (Fl::grab() == this) Fl::grab(nullptr);
  }

  void refresh() {
    int hour = 0, minute = 0, second = 0;
    if (!owner_->get_time(hour, minute, second)) owner_->now(hour, minute, second);
    updating_ = 1;
    hour_->value(hour + 1);
    minute_->value(minute + 1);
    second_->value(second + 1);
    hour_->topline(hour + 1);
    minute_->topline(minute > 2 ? minute - 1 : 1);
    second_->topline(second > 2 ? second - 1 : 1);
    updating_ = 0;
    hour_heading_->copy_label(owner_->hour_heading());
    minute_heading_->copy_label(owner_->minute_heading());
    second_heading_->copy_label(owner_->second_heading());
    now_->copy_label(owner_->now_label());
    int show = owner_->show_seconds();
    if (show) {
      second_heading_->show();
      second_->show();
      size(210, 220);
      hour_heading_->resize(8, 6, 60, 18);
      minute_heading_->resize(74, 6, 60, 18);
      hour_->resize(8, 26, 60, 150);
      minute_->resize(74, 26, 60, 150);
      now_->resize(8, 184, 194, 26);
    } else {
      second_heading_->hide();
      second_->hide();
      size(144, 220);
      hour_heading_->resize(8, 6, 60, 18);
      minute_heading_->resize(74, 6, 60, 18);
      hour_->resize(8, 26, 60, 150);
      minute_->resize(74, 26, 60, 150);
      now_->resize(8, 184, 128, 26);
    }
    int nh, nm, ns;
    owner_->now(nh, nm, ns);
    if (owner_->in_range(nh, nm, ns)) now_->activate();
    else now_->deactivate();
    redraw();
  }

  int handle(int event) override {
    if (event == FL_KEYBOARD && Fl::event_key() == FL_Escape) {
      hide();
      return 1;
    }
    if (event == FL_PUSH &&
        (Fl::event_x() < 0 || Fl::event_y() < 0 || Fl::event_x() >= w() || Fl::event_y() >= h())) {
      hide();
      return 1;
    }
    return Fl_Menu_Window::handle(event);
  }

  void hide() override {
    if (Fl::grab() == this) Fl::grab(nullptr);
    Fl_Menu_Window::hide();
  }
};

static char default_now_text[32] = "Now";
static char default_hour_text[32] = "Hour";
static char default_minute_text[32] = "Minute";
static char default_second_text[32] = "Second";

static void store_default(char *dest, int size, const char *text) {
  if (!text) return;
  std::snprintf(dest, size, "%s", text);
}

void Fl_Time_Input::default_now_label(const char *text) { store_default(default_now_text, 32, text); }
const char *Fl_Time_Input::default_now_label() { return default_now_text; }
void Fl_Time_Input::default_hour_heading(const char *text) { store_default(default_hour_text, 32, text); }
const char *Fl_Time_Input::default_hour_heading() { return default_hour_text; }
void Fl_Time_Input::default_minute_heading(const char *text) { store_default(default_minute_text, 32, text); }
const char *Fl_Time_Input::default_minute_heading() { return default_minute_text; }
void Fl_Time_Input::default_second_heading(const char *text) { store_default(default_second_text, 32, text); }
const char *Fl_Time_Input::default_second_heading() { return default_second_text; }

void Fl_Time_Input::now(int &hour, int &minute, int &second) {
  std::time_t clock = std::time(nullptr);
  std::tm local;
#if defined(_WIN32)
  localtime_s(&local, &clock);
#else
  localtime_r(&clock, &local);
#endif
  hour = local.tm_hour;
  minute = local.tm_min;
  second = local.tm_sec;
}

Fl_Time_Input::Fl_Time_Input(int X, int Y, int W, int H, const char *L)
  : Fl_Group(X, Y, W, H, L),
    popup_(nullptr),
    hour_(0), minute_(0), second_(0),
    minimum_hour_(0), minimum_minute_(0), minimum_second_(0),
    maximum_hour_(0), maximum_minute_(0), maximum_second_(0),
    has_time_(0), has_minimum_(0), has_maximum_(0),
    allow_empty_(1), show_seconds_(1),
    now_set_(0), hour_set_(0), minute_set_(0), second_set_(0) {
  text_[0] = 0;
  minimum_text_[0] = 0;
  maximum_text_[0] = 0;
  box(FL_DOWN_BOX);
  color(FL_BACKGROUND2_COLOR);
  align(FL_ALIGN_LEFT);
  begin();
  int button_w = H < W ? H : W / 4;
  if (button_w < 8) button_w = 8;
  if (button_w > W) button_w = W;
  field_ = new Time_Field(this, X, Y, W - button_w, H);
  field_->color(FL_BACKGROUND2_COLOR);
  button_ = new Clock_Button(this, X + W - button_w, Y, button_w, H);
  end();
  layout_children();
  int hour, minute, second;
  now(hour, minute, second);
  value(hour, minute, second);
}

Fl_Time_Input::~Fl_Time_Input() {
  close_popup();
  delete popup_;
  popup_ = nullptr;
}

void Fl_Time_Input::layout_children() {
  int dx = Fl::box_dx(box());
  int dy = Fl::box_dy(box());
  int inner_w = w() - Fl::box_dw(box());
  int inner_h = h() - Fl::box_dh(box());
  if (inner_w < 1) inner_w = 1;
  if (inner_h < 1) inner_h = 1;
  int button_w = inner_h;
  if (button_w > inner_w / 2) button_w = inner_w / 2;
  if (button_w < 8) button_w = inner_w < 8 ? inner_w : 8;
  field_->resize(x() + dx, y() + dy, inner_w - button_w, inner_h);
  button_->resize(x() + dx + inner_w - button_w, y() + dy, button_w, inner_h);
}

void Fl_Time_Input::refresh_text() {
  if (has_time_) format_time(text_, hour_, minute_, second_, show_seconds_);
  else text_[0] = 0;
  field_->value(text_);
}

int Fl_Time_Input::to_seconds(int hour, int minute, int second) {
  return hour * 3600 + minute * 60 + second;
}

int Fl_Time_Input::in_range(int hour, int minute, int second) const {
  if (!valid_time(hour, minute, second)) return 0;
  int value = to_seconds(hour, minute, second);
  if (has_minimum_ && value < to_seconds(minimum_hour_, minimum_minute_, minimum_second_)) return 0;
  if (has_maximum_ && value > to_seconds(maximum_hour_, maximum_minute_, maximum_second_)) return 0;
  return 1;
}

int Fl_Time_Input::apply_time(int hour, int minute, int second, int notify) {
  int old_has = has_time_;
  int old_hour = hour_, old_minute = minute_, old_second = second_;
  if (!value(hour, minute, second)) return 0;
  if (notify && (old_has != has_time_ || old_hour != hour_ || old_minute != minute_ || old_second != second_))
    do_callback();
  return 1;
}

int Fl_Time_Input::value(int hour, int minute, int second) {
  if (!show_seconds_) second = 0;
  if (!in_range(hour, minute, second)) return 0;
  hour_ = hour;
  minute_ = minute;
  second_ = second;
  has_time_ = 1;
  refresh_text();
  redraw();
  if (popup_ && popup_->shown()) popup_->refresh();
  return 1;
}

int Fl_Time_Input::value(const char *text) {
  if (!text) text = "";
  while (*text == ' ' || *text == '\t') text++;
  if (!*text) {
    if (!allow_empty_) return 0;
    has_time_ = 0;
    refresh_text();
    redraw();
    return 1;
  }
  int hour, minute, second, saw_second = 0;
  if (!parse_time(text, hour, minute, second, saw_second)) return 0;
  if (!show_seconds_) second = 0;
  return value(hour, minute, second);
}

void Fl_Time_Input::clear_time() {
  if (!allow_empty_ || !has_time_) return;
  has_time_ = 0;
  refresh_text();
  redraw();
}

int Fl_Time_Input::get_time(int &hour, int &minute, int &second) const {
  if (!has_time_) return 0;
  hour = hour_;
  minute = minute_;
  second = second_;
  return 1;
}

void Fl_Time_Input::allow_empty(int allow) { allow_empty_ = allow ? 1 : 0; }

void Fl_Time_Input::show_seconds(int show) {
  show_seconds_ = show ? 1 : 0;
  if (!show_seconds_) second_ = 0;
  refresh_text();
  if (popup_ && popup_->shown()) popup_->refresh();
  redraw();
}

int Fl_Time_Input::minimum_time(int hour, int minute, int second) {
  if (!valid_time(hour, minute, second)) return 0;
  if (has_maximum_ && to_seconds(hour, minute, second) > to_seconds(maximum_hour_, maximum_minute_, maximum_second_))
    return 0;
  minimum_hour_ = hour;
  minimum_minute_ = minute;
  minimum_second_ = second;
  format_time(minimum_text_, hour, minute, second, 1);
  has_minimum_ = 1;
  if (has_time_ && !in_range(hour_, minute_, second_)) {
    if (allow_empty_) clear_time();
    else value(hour, minute, second);
  }
  if (popup_ && popup_->shown()) popup_->refresh();
  return 1;
}

int Fl_Time_Input::maximum_time(int hour, int minute, int second) {
  if (!valid_time(hour, minute, second)) return 0;
  if (has_minimum_ && to_seconds(hour, minute, second) < to_seconds(minimum_hour_, minimum_minute_, minimum_second_))
    return 0;
  maximum_hour_ = hour;
  maximum_minute_ = minute;
  maximum_second_ = second;
  format_time(maximum_text_, hour, minute, second, 1);
  has_maximum_ = 1;
  if (has_time_ && !in_range(hour_, minute_, second_)) {
    if (allow_empty_) clear_time();
    else value(hour, minute, second);
  }
  if (popup_ && popup_->shown()) popup_->refresh();
  return 1;
}

int Fl_Time_Input::minimum_time(const char *text) {
  if (!text) text = "";
  while (*text == ' ' || *text == '\t') text++;
  if (!*text) {
    has_minimum_ = 0;
    minimum_text_[0] = 0;
    return 1;
  }
  int hour, minute, second, saw = 0;
  if (!parse_time(text, hour, minute, second, saw)) return 0;
  return minimum_time(hour, minute, second);
}

int Fl_Time_Input::maximum_time(const char *text) {
  if (!text) text = "";
  while (*text == ' ' || *text == '\t') text++;
  if (!*text) {
    has_maximum_ = 0;
    maximum_text_[0] = 0;
    return 1;
  }
  int hour, minute, second, saw = 0;
  if (!parse_time(text, hour, minute, second, saw)) return 0;
  return maximum_time(hour, minute, second);
}

int Fl_Time_Input::get_minimum(int &hour, int &minute, int &second) const {
  if (!has_minimum_) return 0;
  hour = minimum_hour_;
  minute = minimum_minute_;
  second = minimum_second_;
  return 1;
}

int Fl_Time_Input::get_maximum(int &hour, int &minute, int &second) const {
  if (!has_maximum_) return 0;
  hour = maximum_hour_;
  minute = maximum_minute_;
  second = maximum_second_;
  return 1;
}

static void set_label(int &flag, char *dest, int size, const char *text) {
  if (!text) flag = 0;
  else {
    std::snprintf(dest, size, "%s", text);
    flag = 1;
  }
}

static const char *label_or_default(int flag, const char *own, const char *fallback) {
  return flag ? own : fallback;
}

void Fl_Time_Input::now_label(const char *text) {
  set_label(now_set_, now_text_, 32, text);
  if (popup_ && popup_->shown()) popup_->refresh();
}
const char *Fl_Time_Input::now_label() const { return label_or_default(now_set_, now_text_, default_now_label()); }
void Fl_Time_Input::hour_heading(const char *text) {
  set_label(hour_set_, hour_text_, 32, text);
  if (popup_ && popup_->shown()) popup_->refresh();
}
const char *Fl_Time_Input::hour_heading() const { return label_or_default(hour_set_, hour_text_, default_hour_heading()); }
void Fl_Time_Input::minute_heading(const char *text) {
  set_label(minute_set_, minute_text_, 32, text);
  if (popup_ && popup_->shown()) popup_->refresh();
}
const char *Fl_Time_Input::minute_heading() const { return label_or_default(minute_set_, minute_text_, default_minute_heading()); }
void Fl_Time_Input::second_heading(const char *text) {
  set_label(second_set_, second_text_, 32, text);
  if (popup_ && popup_->shown()) popup_->refresh();
}
const char *Fl_Time_Input::second_heading() const { return label_or_default(second_set_, second_text_, default_second_heading()); }

Fl_Font Fl_Time_Input::textfont() const { return field_->textfont(); }
void Fl_Time_Input::textfont(Fl_Font font) { field_->textfont(font); }
int Fl_Time_Input::textsize() const { return field_->textsize(); }
void Fl_Time_Input::textsize(int size) { field_->textsize(size); }
Fl_Color Fl_Time_Input::textcolor() const { return field_->textcolor(); }
void Fl_Time_Input::textcolor(Fl_Color color) { field_->textcolor(color); }

void Fl_Time_Input::commit_text() {
  int old_has = has_time_;
  int old_hour = hour_, old_minute = minute_, old_second = second_;
  if (!value(field_->value())) {
    field_->value(text_);
    return;
  }
  if (old_has != has_time_ || old_hour != hour_ || old_minute != minute_ || old_second != second_)
    do_callback();
}

void Fl_Time_Input::open_popup() {
  if (!active()) return;
  if (!popup_) {
    Fl_Group *saved = Fl_Group::current();
    Fl_Group::current(nullptr);
    popup_ = new Time_Popup(this);
    Fl_Group::current(saved);
  }
  popup_->refresh();
  int sx = x();
  int sy = y();
  for (Fl_Window *win = window(); win; win = win->window()) {
    sx += win->x();
    sy += win->y();
  }
  int area_x, area_y, area_w, area_h;
  int screen = window() ? window()->screen_num() : 0;
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

void Fl_Time_Input::close_popup() {
  if (popup_) popup_->hide();
}

void Fl_Time_Input::draw() {
  layout_children();
  field_->color(color());
  field_->textcolor(textcolor());
  button_->color(color());
  button_->box(FL_FLAT_BOX);
  Fl_Group::draw();
}

void Fl_Time_Input::resize(int X, int Y, int W, int H) {
  Fl_Group::resize(X, Y, W, H);
  layout_children();
}
