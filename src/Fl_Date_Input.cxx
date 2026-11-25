//
// Date input widget for the FLTK Plus widget set.
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

#include <FL/Fl_Date_Input.H>

#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Menu_Window.H>
#include <FL/fl_draw.H>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

// Gregorian dates as a day count, so month arithmetic does not depend on a
// platform calendar. 1970-01-01 is day 0.
static int days_in_month(int year, int month) {
  static const int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) return 0;
  if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) return 29;
  return days[month];
}

static int valid_date(int year, int month, int day) {
  if (year < 1 || year > 9999) return 0;
  if (day < 1 || day > days_in_month(year, month)) return 0;
  return 1;
}

static int to_serial(int year, int month, int day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = (unsigned)(year - era * 400);
  const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int)doe - 719468;
}

static void from_serial(int serial, int &year, int &month, int &day) {
  serial += 719468;
  const int era = (serial >= 0 ? serial : serial - 146096) / 146097;
  const unsigned doe = (unsigned)(serial - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  year = (int)yoe + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  day = (int)(doy - (153 * mp + 2) / 5 + 1);
  month = (int)(mp < 10 ? mp + 3 : mp - 9);
  year += month <= 2;
}

static void add_days(int &year, int &month, int &day, int delta) {
  from_serial(to_serial(year, month, day) + delta, year, month, day);
}

static void add_months(int &year, int &month, int &day, int delta) {
  int index = year * 12 + (month - 1) + delta;
  year = index / 12;
  month = index % 12;
  if (month < 0) {
    month += 12;
    year -= 1;
  }
  month += 1;
  int limit = days_in_month(year, month);
  if (day > limit) day = limit;
}

static int monday_index(int year, int month, int day) {
  int serial = to_serial(year, month, day);
  int index = (serial + 3) % 7;
  if (index < 0) index += 7;
  return index;
}

static void format_date(char *dest, int year, int month, int day) {
  std::snprintf(dest, 16, "%04d-%02d-%02d", year, month, day);
}

static int parse_date(const char *text, int &year, int &month, int &day) {
  if (!text) return 0;
  while (*text == ' ' || *text == '\t') text++;
  if (!*text) return 0;
  int y = 0, m = 0, d = 0;
  char extra = 0;
  if (std::sscanf(text, "%d-%d-%d %c", &y, &m, &d, &extra) == 3 ||
      std::sscanf(text, "%d/%d/%d %c", &y, &m, &d, &extra) == 3 ||
      std::sscanf(text, "%d.%d.%d %c", &y, &m, &d, &extra) == 3) {
    year = y;
    month = m;
    day = d;
    return valid_date(year, month, day);
  }
  if (std::sscanf(text, "%4d%2d%2d %c", &y, &m, &d, &extra) == 3) {
    year = y;
    month = m;
    day = d;
    return valid_date(year, month, day);
  }
  return 0;
}

class Fl_Date_Input::Date_Field : public Fl_Input {
  Fl_Date_Input *owner_;
public:
  Date_Field(Fl_Date_Input *owner, int X, int Y, int W, int H)
    : Fl_Input(X, Y, W, H), owner_(owner) {
    when(FL_WHEN_NEVER);
    box(FL_FLAT_BOX);
  }
  int handle(int event) override {
    int used = Fl_Input::handle(event);
    if (event == FL_UNFOCUS ||
        (event == FL_KEYBOARD && (Fl::event_key() == FL_Enter || Fl::event_key() == FL_KP_Enter)))
      owner_->commit_text();
    return used;
  }
};

class Fl_Date_Input::Calendar_Button : public Fl_Button {
  Fl_Date_Input *owner_;
  void draw_glyph(int X, int Y, int W, int H) {
    int side = W < H ? W : H;
    int ox = X + (W - side) / 2;
    int oy = Y + (H - side) / 2;
    int margin = side / 8;
    if (margin < 1) margin = 1;
    int ring_h = side / 7;
    if (ring_h < 2) ring_h = 2;
    int body_x = ox + margin;
    int body_y = oy + margin + ring_h - 1;
    int body_w = side - margin * 2;
    int body_h = side - (body_y - oy) - margin;
    if (body_w < 4 || body_h < 4) return;
    int ring_w = body_w / 5;
    if (ring_w < 2) ring_w = 2;
    fl_color(FL_BLACK);
    fl_rect(body_x + ring_w, body_y - ring_h + 1, ring_w, ring_h);
    fl_rect(body_x + body_w - ring_w * 2, body_y - ring_h + 1, ring_w, ring_h);
    fl_rect(body_x, body_y, body_w, body_h);
    int head = body_h / 4;
    if (head < 2) head = 2;
    fl_color(fl_rgb_color(208, 32, 32));
    fl_rectf(body_x + 1, body_y + 1, body_w - 2, head);
    fl_color(fl_rgb_color(70, 70, 70));
    int grid_y = body_y + head + 2;
    int grid_h = body_y + body_h - grid_y - 2;
    int cell_w = (body_w - 6) / 3;
    int cell_h = grid_h / 3;
    if (cell_w < 1 || cell_h < 1) return;
    for (int row = 0; row < 3; row++) {
      for (int col = 0; col < 3; col++) {
        fl_rectf(body_x + 2 + col * cell_w, grid_y + row * cell_h, cell_w - 1, cell_h - 1);
      }
    }
  }
public:
  Calendar_Button(Fl_Date_Input *owner, int X, int Y, int W, int H)
    : Fl_Button(X, Y, W, H), owner_(owner) {
    box(FL_UP_BOX);
    color(FL_BACKGROUND_COLOR);
  }
  void draw() override {
    Fl_Button::draw();
    draw_glyph(x() + 3, y() + 3, w() - 6, h() - 6);
  }
  int handle(int event) override {
    if (event == FL_RELEASE && Fl::event_inside(this)) {
      value(0);
      if (active()) owner_->open_popup();
      return 1;
    }
    return Fl_Button::handle(event);
  }
};

class Fl_Date_Input::Date_Popup : public Fl_Menu_Window {
  static const int kCell = 28;
  static const int kPad = 6;
  static const int kHead = 26;
  static const int kWeek = 20;
  static const int kToday = 26;

  class Grid : public Fl_Widget {
  public:
    struct Cell {
      int year, month, day;
      int in_month;
      int enabled;
    };
    Cell cells_[42];
    int cursor_serial_;
    Fl_Date_Input *owner_;
    Grid(int X, int Y, int W, int H, Fl_Date_Input *owner)
      : Fl_Widget(X, Y, W, H), cursor_serial_(-1), owner_(owner) {
      std::memset(cells_, 0, sizeof(cells_));
    }
    void draw() override {
      fl_color(FL_BACKGROUND2_COLOR);
      fl_rectf(x(), y(), w(), h());
      int cell_w = w() / 7;
      int cell_h = (h() - kWeek) / 6;
      static const char *names[] = {"一", "二", "三", "四", "五", "六", "日"};
      fl_font(labelfont(), labelsize());
      fl_color(FL_FOREGROUND_COLOR);
      for (int col = 0; col < 7; col++) {
        fl_draw(names[col], x() + col * cell_w, y(), cell_w, kWeek, FL_ALIGN_CENTER);
      }
      int selected = -1;
      int sy, sm, sd;
      if (owner_->get_date(sy, sm, sd)) selected = to_serial(sy, sm, sd);
      int ty, tm, td;
      Fl_Date_Input::today(ty, tm, td);
      int today_serial = to_serial(ty, tm, td);
      for (int i = 0; i < 42; i++) {
        int col = i % 7;
        int row = i / 7;
        int cx = x() + col * cell_w;
        int cy = y() + kWeek + row * cell_h;
        const Cell &cell = cells_[i];
        int serial = cell.year ? to_serial(cell.year, cell.month, cell.day) : -1;
        if (serial == selected && cell.enabled) {
          fl_color(owner_->selection_color());
          fl_rectf(cx + 1, cy + 1, cell_w - 2, cell_h - 2);
          fl_color(fl_contrast(FL_FOREGROUND_COLOR, owner_->selection_color()));
        } else if (!cell.in_month || !cell.enabled) {
          fl_color(FL_INACTIVE_COLOR);
        } else {
          fl_color(FL_FOREGROUND_COLOR);
        }
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%d", cell.day);
        fl_font(labelfont(), labelsize());
        fl_draw(buf, cx, cy, cell_w, cell_h, FL_ALIGN_CENTER);
        if (serial == today_serial) {
          fl_color(FL_FOREGROUND_COLOR);
          fl_rect(cx + 2, cy + 2, cell_w - 4, cell_h - 4);
        }
        if (serial == cursor_serial_ && serial != selected) {
          fl_color(FL_SELECTION_COLOR);
          fl_rect(cx + 1, cy + 1, cell_w - 2, cell_h - 2);
        }
      }
    }
    int cell_at(int px, int py) const {
      if (py < y() + kWeek) return -1;
      int cell_w = w() / 7;
      int cell_h = (h() - kWeek) / 6;
      if (cell_w < 1 || cell_h < 1) return -1;
      int col = (px - x()) / cell_w;
      int row = (py - (y() + kWeek)) / cell_h;
      if (col < 0 || col > 6 || row < 0 || row > 5) return -1;
      return row * 7 + col;
    }
    int handle(int event) override {
      if (event != FL_PUSH && event != FL_RELEASE) return 0;
      int index = cell_at(Fl::event_x(), Fl::event_y());
      if (index < 0 || !cells_[index].enabled) return 1;
      if (event == FL_RELEASE) {
        Date_Popup *popup = (Date_Popup *)window();
        popup->choose(cells_[index].year, cells_[index].month, cells_[index].day);
      }
      return 1;
    }
  };

  Fl_Date_Input *owner_;
  Fl_Button *prev_year_;
  Fl_Button *prev_month_;
  Fl_Button *title_;
  Fl_Button *next_month_;
  Fl_Button *next_year_;
  Fl_Button *today_;
  Fl_Button *months_[12];
  Grid *grid_;
  int view_year_;
  int view_month_;
  int picking_month_;

  static void prev_year_cb(Fl_Widget *, void *self) {
    Date_Popup *popup = (Date_Popup *)self;
    popup->shift_view(popup->picking_month_ ? -10 : -1, 0);
  }
  static void next_year_cb(Fl_Widget *, void *self) {
    Date_Popup *popup = (Date_Popup *)self;
    popup->shift_view(popup->picking_month_ ? 10 : 1, 0);
  }
  static void prev_month_cb(Fl_Widget *, void *self) {
    Date_Popup *popup = (Date_Popup *)self;
    if (popup->picking_month_) popup->shift_view(-1, 0);
    else popup->shift_view(0, -1);
  }
  static void next_month_cb(Fl_Widget *, void *self) {
    Date_Popup *popup = (Date_Popup *)self;
    if (popup->picking_month_) popup->shift_view(1, 0);
    else popup->shift_view(0, 1);
  }
  static void title_cb(Fl_Widget *, void *self) {
    Date_Popup *popup = (Date_Popup *)self;
    popup->picking_month_ = !popup->picking_month_;
    popup->refresh();
  }
  static void month_cb(Fl_Widget *widget, void *self) {
    Date_Popup *popup = (Date_Popup *)self;
    popup->view_month_ = std::atoi(widget->label());
    popup->picking_month_ = 0;
    popup->refresh();
  }
  static void today_cb(Fl_Widget *, void *self) {
    int year, month, day;
    Fl_Date_Input::today(year, month, day);
    ((Date_Popup *)self)->choose(year, month, day);
  }

  void shift_view(int years, int months) {
    int day = 1;
    if (years) add_months(view_year_, view_month_, day, years * 12);
    if (months) add_months(view_year_, view_month_, day, months);
    if (view_year_ < 1) {
      view_year_ = 1;
      view_month_ = 1;
    }
    if (view_year_ > 9999) {
      view_year_ = 9999;
      view_month_ = 12;
    }
    refresh();
  }

public:
  Date_Popup(Fl_Date_Input *owner)
    : Fl_Menu_Window(0, 0, kPad * 2 + kCell * 7, kPad + kHead + kWeek + kCell * 6 + kToday + kPad),
      owner_(owner), view_year_(1), view_month_(1), picking_month_(0) {
    set_override();
    box(FL_UP_BOX);
    color(FL_BACKGROUND_COLOR);
    int width = w();
    int side = 26;
    int gap = 2;
    int title_w = width - kPad * 2 - side * 4 - gap * 4;
    int x = kPad;
    int y = kPad;
    begin();
    prev_year_ = new Fl_Button(x, y, side, kHead - 2, "<<");
    x += side + gap;
    prev_month_ = new Fl_Button(x, y, side, kHead - 2, "<");
    x += side + gap;
    title_ = new Fl_Button(x, y, title_w, kHead - 2);
    x += title_w + gap;
    next_month_ = new Fl_Button(x, y, side, kHead - 2, ">");
    x += side + gap;
    next_year_ = new Fl_Button(x, y, side, kHead - 2, ">>");
    prev_year_->callback(prev_year_cb, this);
    prev_month_->callback(prev_month_cb, this);
    title_->callback(title_cb, this);
    next_month_->callback(next_month_cb, this);
    next_year_->callback(next_year_cb, this);
    int grid_y = kPad + kHead;
    int grid_w = kCell * 7;
    int grid_h = kWeek + kCell * 6;
    grid_ = new Grid(kPad, grid_y, grid_w, grid_h, owner_);
    int button_w = grid_w / 4;
    int button_h = grid_h / 3;
    for (int i = 0; i < 12; i++) {
      int col = i % 4;
      int row = i / 4;
      static const char *labels[] = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12"
      };
      months_[i] = new Fl_Button(kPad + col * button_w, grid_y + row * button_h,
                                 button_w, button_h, labels[i]);
      months_[i]->callback(month_cb, this);
      months_[i]->hide();
    }
    today_ = new Fl_Button(kPad, grid_y + grid_h + 2, grid_w, kToday - 4, "今天");
    today_->callback(today_cb, this);
    end();
  }

  ~Date_Popup() override {
    if (Fl::grab() == this) Fl::grab(nullptr);
  }

  void choose(int year, int month, int day) {
    if (!owner_->in_range(year, month, day) || !valid_date(year, month, day)) return;
    owner_->apply_date(year, month, day, 1);
    hide();
  }

  void show_month(int year, int month) {
    view_year_ = year;
    view_month_ = month;
    refresh();
  }

  void prepare() {
    picking_month_ = 0;
    int year, month, day;
    if (!owner_->get_date(year, month, day)) Fl_Date_Input::today(year, month, day);
    view_year_ = year;
    view_month_ = month;
    grid_->cursor_serial_ = -1;
    refresh();
  }

  void refresh() {
    char title[16];
    if (picking_month_) std::snprintf(title, sizeof(title), "%04d", view_year_);
    else std::snprintf(title, sizeof(title), "%04d-%02d", view_year_, view_month_);
    title_->copy_label(title);
    int first = monday_index(view_year_, view_month_, 1);
    int start_serial = to_serial(view_year_, view_month_, 1) - first;
    for (int i = 0; i < 42; i++) {
      int year, month, day;
      from_serial(start_serial + i, year, month, day);
      grid_->cells_[i].year = year;
      grid_->cells_[i].month = month;
      grid_->cells_[i].day = day;
      grid_->cells_[i].in_month = month == view_month_ && year == view_year_;
      grid_->cells_[i].enabled = owner_->in_range(year, month, day);
    }
    Fl_Font font = owner_->textfont();
    int size = owner_->textsize();
    grid_->labelfont(font);
    grid_->labelsize(size > 0 ? size : 14);
    if (picking_month_) grid_->hide();
    else grid_->show();
    for (int i = 0; i < 12; i++) {
      if (picking_month_) months_[i]->show();
      else months_[i]->hide();
    }
    int ty, tm, td;
    Fl_Date_Input::today(ty, tm, td);
    if (owner_->in_range(ty, tm, td)) today_->activate();
    else today_->deactivate();
    redraw();
  }

  void move_month(int months) {
    int year, month, day;
    if (grid_->cursor_serial_ >= 0) from_serial(grid_->cursor_serial_, year, month, day);
    else if (!owner_->get_date(year, month, day)) Fl_Date_Input::today(year, month, day);
    add_months(year, month, day, months);
    if (!owner_->in_range(year, month, day)) return;
    grid_->cursor_serial_ = to_serial(year, month, day);
    view_year_ = year;
    view_month_ = month;
    refresh();
    grid_->cursor_serial_ = to_serial(year, month, day);
  }

  void move_cursor(int days) {
    int year, month, day;
    if (grid_->cursor_serial_ >= 0) from_serial(grid_->cursor_serial_, year, month, day);
    else if (!owner_->get_date(year, month, day)) Fl_Date_Input::today(year, month, day);
    add_days(year, month, day, days);
    if (!owner_->in_range(year, month, day)) return;
    grid_->cursor_serial_ = to_serial(year, month, day);
    view_year_ = year;
    view_month_ = month;
    refresh();
    grid_->cursor_serial_ = to_serial(year, month, day);
  }

  void commit_cursor() {
    if (grid_->cursor_serial_ < 0) {
      hide();
      return;
    }
    int year, month, day;
    from_serial(grid_->cursor_serial_, year, month, day);
    choose(year, month, day);
  }

  int handle(int event) override {
    if (event == FL_KEYBOARD) {
      int key = Fl::event_key();
      if (key == FL_Escape) {
        hide();
        return 1;
      }
      if (!picking_month_) {
        if (key == FL_Left) { move_cursor(-1); return 1; }
        if (key == FL_Right) { move_cursor(1); return 1; }
        if (key == FL_Up) { move_cursor(-7); return 1; }
        if (key == FL_Down) { move_cursor(7); return 1; }
        if (key == FL_Page_Up) { move_month(-1); return 1; }
        if (key == FL_Page_Down) { move_month(1); return 1; }
        if (key == FL_Enter || key == FL_KP_Enter) { commit_cursor(); return 1; }
      }
    }
    if (event == FL_PUSH && !Fl::event_inside(this)) {
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

void Fl_Date_Input::today(int &year, int &month, int &day) {
  std::time_t now = std::time(nullptr);
  std::tm local;
#if defined(_WIN32)
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  year = local.tm_year + 1900;
  month = local.tm_mon + 1;
  day = local.tm_mday;
}

Fl_Date_Input::Fl_Date_Input(int X, int Y, int W, int H, const char *L)
  : Fl_Group(X, Y, W, H, L),
    popup_(nullptr),
    year_(0), month_(0), day_(0),
    minimum_year_(0), minimum_month_(0), minimum_day_(0),
    maximum_year_(0), maximum_month_(0), maximum_day_(0),
    has_date_(0), has_minimum_(0), has_maximum_(0), allow_empty_(1) {
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
  field_ = new Date_Field(this, X, Y, W - button_w, H);
  field_->color(FL_BACKGROUND2_COLOR);
  button_ = new Calendar_Button(this, X + W - button_w, Y, button_w, H);
  end();
}

Fl_Date_Input::~Fl_Date_Input() {
  close_popup();
  delete popup_;
  popup_ = nullptr;
}

void Fl_Date_Input::layout_children() {
  int button_w = h();
  if (button_w > w() / 2) button_w = w() / 2;
  if (button_w < 8) button_w = 8;
  if (button_w > w()) button_w = w();
  field_->resize(x(), y(), w() - button_w, h());
  button_->resize(x() + w() - button_w, y(), button_w, h());
}

void Fl_Date_Input::refresh_text() {
  if (has_date_) format_date(text_, year_, month_, day_);
  else text_[0] = 0;
  field_->value(text_);
}

int Fl_Date_Input::in_range(int year, int month, int day) const {
  if (!valid_date(year, month, day)) return 0;
  int serial = to_serial(year, month, day);
  if (has_minimum_ && serial < to_serial(minimum_year_, minimum_month_, minimum_day_)) return 0;
  if (has_maximum_ && serial > to_serial(maximum_year_, maximum_month_, maximum_day_)) return 0;
  return 1;
}

int Fl_Date_Input::apply_date(int year, int month, int day, int notify) {
  int old_has = has_date_;
  int old_year = year_, old_month = month_, old_day = day_;
  if (!value(year, month, day)) return 0;
  if (notify && (old_has != has_date_ || old_year != year_ || old_month != month_ || old_day != day_))
    do_callback();
  return 1;
}

int Fl_Date_Input::value(int year, int month, int day) {
  if (!in_range(year, month, day)) return 0;
  year_ = year;
  month_ = month;
  day_ = day;
  has_date_ = 1;
  refresh_text();
  redraw();
  if (popup_ && popup_->shown()) popup_->show_month(year_, month_);
  return 1;
}

int Fl_Date_Input::value(const char *text) {
  if (!text) text = "";
  while (*text == ' ' || *text == '\t') text++;
  if (!*text) {
    if (!allow_empty_) return 0;
    int changed = has_date_;
    has_date_ = 0;
    refresh_text();
    redraw();
    return changed || 1;
  }
  int year, month, day;
  if (!parse_date(text, year, month, day)) return 0;
  return value(year, month, day);
}

void Fl_Date_Input::clear_date() {
  if (!allow_empty_ || !has_date_) return;
  has_date_ = 0;
  refresh_text();
  redraw();
}

int Fl_Date_Input::get_date(int &year, int &month, int &day) const {
  if (!has_date_) return 0;
  year = year_;
  month = month_;
  day = day_;
  return 1;
}

void Fl_Date_Input::allow_empty(int allow) {
  allow_empty_ = allow ? 1 : 0;
}

int Fl_Date_Input::minimum_date(int year, int month, int day) {
  if (year == 0 && month == 0 && day == 0) {
    has_minimum_ = 0;
    minimum_text_[0] = 0;
    return 1;
  }
  if (!valid_date(year, month, day)) return 0;
  if (has_maximum_ && to_serial(year, month, day) > to_serial(maximum_year_, maximum_month_, maximum_day_))
    return 0;
  has_minimum_ = 1;
  minimum_year_ = year;
  minimum_month_ = month;
  minimum_day_ = day;
  format_date(minimum_text_, year, month, day);
  if (has_date_ && !in_range(year_, month_, day_)) {
    if (allow_empty_) clear_date();
    else value(year, month, day);
  }
  if (popup_ && popup_->shown()) popup_->refresh();
  return 1;
}

int Fl_Date_Input::maximum_date(int year, int month, int day) {
  if (year == 0 && month == 0 && day == 0) {
    has_maximum_ = 0;
    maximum_text_[0] = 0;
    return 1;
  }
  if (!valid_date(year, month, day)) return 0;
  if (has_minimum_ && to_serial(year, month, day) < to_serial(minimum_year_, minimum_month_, minimum_day_))
    return 0;
  has_maximum_ = 1;
  maximum_year_ = year;
  maximum_month_ = month;
  maximum_day_ = day;
  format_date(maximum_text_, year, month, day);
  if (has_date_ && !in_range(year_, month_, day_)) {
    if (allow_empty_) clear_date();
    else value(year, month, day);
  }
  if (popup_ && popup_->shown()) popup_->refresh();
  return 1;
}

int Fl_Date_Input::minimum_date(const char *text) {
  if (!text) text = "";
  while (*text == ' ' || *text == '\t') text++;
  if (!*text) return minimum_date(0, 0, 0);
  int year, month, day;
  if (!parse_date(text, year, month, day)) return 0;
  return minimum_date(year, month, day);
}

int Fl_Date_Input::maximum_date(const char *text) {
  if (!text) text = "";
  while (*text == ' ' || *text == '\t') text++;
  if (!*text) return maximum_date(0, 0, 0);
  int year, month, day;
  if (!parse_date(text, year, month, day)) return 0;
  return maximum_date(year, month, day);
}

int Fl_Date_Input::get_minimum(int &year, int &month, int &day) const {
  if (!has_minimum_) return 0;
  year = minimum_year_;
  month = minimum_month_;
  day = minimum_day_;
  return 1;
}

int Fl_Date_Input::get_maximum(int &year, int &month, int &day) const {
  if (!has_maximum_) return 0;
  year = maximum_year_;
  month = maximum_month_;
  day = maximum_day_;
  return 1;
}

Fl_Font Fl_Date_Input::textfont() const { return field_->textfont(); }
void Fl_Date_Input::textfont(Fl_Font font) { field_->textfont(font); }
int Fl_Date_Input::textsize() const { return field_->textsize(); }
void Fl_Date_Input::textsize(int size) { field_->textsize(size); }
Fl_Color Fl_Date_Input::textcolor() const { return field_->textcolor(); }
void Fl_Date_Input::textcolor(Fl_Color color) { field_->textcolor(color); }

void Fl_Date_Input::commit_text() {
  int old_has = has_date_;
  int old_year = year_, old_month = month_, old_day = day_;
  if (!value(field_->value())) {
    field_->value(text_);
    return;
  }
  if (old_has != has_date_ || old_year != year_ || old_month != month_ || old_day != day_)
    do_callback();
}

void Fl_Date_Input::open_popup() {
  if (!active()) return;
  if (!popup_) {
    Fl_Group *saved = Fl_Group::current();
    Fl_Group::current(nullptr);
    popup_ = new Date_Popup(this);
    Fl_Group::current(saved);
  }
  popup_->prepare();
  int sx = x();
  int sy = y();
  for (Fl_Widget *parent = this->parent(); parent; parent = parent->parent()) {
    sx += parent->x();
    sy += parent->y();
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

void Fl_Date_Input::close_popup() {
  if (!popup_) return;
  popup_->hide();
}

void Fl_Date_Input::draw() {
  field_->color(color());
  field_->textcolor(textcolor());
  Fl_Group::draw();
}

void Fl_Date_Input::resize(int X, int Y, int W, int H) {
  Fl_Group::resize(X, Y, W, H);
  layout_children();
}
