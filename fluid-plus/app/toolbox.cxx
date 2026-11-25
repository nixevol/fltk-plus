//
// Fluid Plus widget bin. See toolbox.h.
// Click a widget that occupies space, then drag a red dashed rectangle on the design window to create it. Double-click still inserts it at the default size.

#include "app/toolbox.h"

#include "Fluid.h"
#include "app/designer_shell.h"
#include "app/ui_locale.h"
#include "nodes/Window_Node.h"
#include "nodes/factory.h"
#include "panels/function_panel.h"
#include "widgets/Bin_Button.h"
#include "widgets/Node_Browser.h"

#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Image.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Overlay_Window.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Tooltip.H>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace fluid {

namespace {

const int kHead = 24;       // Height of one group-heading row.
const int kRow = 24;        // Height of one widget row.
const int kSearchArea = 36; // Height of the search-field strip.
const int kClick = 8;       // A drag shorter than this many pixels counts as a click and uses the default size.

class Tool_List;

// The widget selected in the bin that has not been drawn yet.
struct Arm {
  bool on = false;
  bool window = false;
  int section = -1;
  std::string type;
};

// The dashed rectangle being dragged. A window uses desktop viewport coordinates; other widgets use design-window coordinates.
struct Band {
  bool down = false;
  bool window = false;
  Window_Node *form = nullptr;
  int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
};

Arm g_arm;
Band g_band;
Tool_List *g_list = nullptr;

void normalize_band(int &x, int &y, int &w, int &h) {
  x = std::min(g_band.x0, g_band.x1);
  y = std::min(g_band.y0, g_band.y1);
  w = std::abs(g_band.x1 - g_band.x0);
  h = std::abs(g_band.y1 - g_band.y0);
}

bool band_is_click() {
  return std::abs(g_band.x1 - g_band.x0) < kClick && std::abs(g_band.y1 - g_band.y0) < kClick;
}

void dash_span(int x0, int y0, int x1, int y1) {
  const int on = 4;
  const int off = 3;
  if (y0 == y1) {
    if (x1 < x0) std::swap(x0, x1);
    for (int x = x0; x < x1; x += on + off) fl_xyline(x, y0, std::min(x + on, x1));
  } else if (x0 == x1) {
    if (y1 < y0) std::swap(y0, y1);
    for (int y = y0; y < y1; y += on + off) fl_yxline(x0, y, std::min(y + on, y1));
  }
}

void dash_rect(int x, int y, int w, int h) {
  if (w < 0) { x += w; w = -w; }
  if (h < 0) { y += h; h = -h; }
  if (w < 1 || h < 1) return;
  fl_color(FL_RED);
  dash_span(x, y, x + w, y);
  dash_span(x, y + h - 1, x + w, y + h - 1);
  dash_span(x, y, x, y + h);
  dash_span(x + w - 1, y, x + w - 1, y + h);
}

void redraw_bands() {
  if (Fl_Window *desk = designer_desk_viewport()) desk->redraw();
  if (auto *overlay = dynamic_cast<Fl_Overlay_Window *>(designer_visible_form()))
    overlay->redraw_overlay();
}

void reset_cursors() {
  if (Fl_Window *form = designer_visible_form()) form->cursor(FL_CURSOR_DEFAULT);
  if (Fl_Window *desk = designer_desk_viewport()) desk->cursor(FL_CURSOR_DEFAULT);
}

std::string lower(const char *text) {
  std::string out = text ? text : "";
  for (char &c : out)
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
  return out;
}

Fl_Color muted(Fl_Color on) {
  return fl_color_average(FL_FOREGROUND_COLOR, on, 0.5f);
}

// Grouped list. It draws itself and manages its own scrolling.
class Tool_List : public Fl_Group {
public:
  Tool_List(int x, int y, int w, int h);
  void filter(const char *text);
  void resize(int x, int y, int w, int h) override;
  void draw() override;
  int handle(int event) override;

private:
  // Type, name, and icon come from the button in the original widget bin, so the name follows the interface language.
  struct Tool {
    Fl_Widget *source = nullptr;
    bool window = false;     // Draws a new window on the gray design area.
    bool placeable = false;  // Occupies space; a click draws a rectangle. The Code group and menu items do not.
    const char *type() const { return (const char *)source->user_data(); }
    const char *name() const {
      const char *tip = source->tooltip();
      return (tip && *tip) ? tip : type();
    }
  };
  struct Section {
    Fl_Widget *source = nullptr;   // The group in the original widget bin; the heading comes from it.
    bool open = true;
    bool code = false;             // The "Code" group adds no pointer, and a click does not draw a rectangle.
    std::vector<Tool> tools;
    const char *title() const { return source->label() ? source->label() : ""; }
  };
  struct Row {
    int section;
    int tool;    // -1 is a group heading, -2 is the pointer, and 0 or greater is a widget.
    int count;   // Group heading: how many widgets are under it.
  };

  void load();
  void rebuild();
  void arrange();
  int total() const;
  int view_w() const;
  int row_height(const Row &row) const { return row.tool == -1 ? kHead : kRow; }
  int row_top(int index) const;
  int row_at(int my) const;
  const Tool *tool_at(int row) const;
  int selected_row() const;
  bool matches(const Tool &tool) const;
  void draw_row(int index, int X, int Y, int W);
  void hover(int row);
  void scroll_to(int top);
  void reveal(int row);
  void step(int by);
  void add(const Tool &tool);
  void draw_arrow(int x, int y, Fl_Color ink);

public:
  void show_pointer(int section);
  void disarm();
  void arm(int section, const Tool &tool);
  static void bar_cb(Fl_Widget *, void *self);

  std::vector<Section> sections_;
  std::vector<Row> rows_;
  std::string filter_;
  std::string tip_;
  Fl_Scrollbar *bar_ = nullptr;
  int top_ = 0;
  int hover_ = -1;
  int sel_section_ = -1;
  int sel_tool_ = -1;
};

Tool_List::Tool_List(int x, int y, int w, int h) : Fl_Group(x, y, w, h) {
  box(FL_NO_BOX);
  bar_ = new Fl_Scrollbar(x + w - Fl::scrollbar_size(), y, Fl::scrollbar_size(), h);
  bar_->callback(bar_cb, this);
  bar_->hide();
  end();
  g_list = this;
  load();
  rebuild();
}

void Tool_List::load() {
  Fl_Group *current = Fl_Group::current();
  Fl_Group::current(nullptr);
  if (!widgetbin_panel) make_widgetbin();
  Fl_Group::current(current);
  if (!widgetbin_panel) return;
  for (int i = 0; i < widgetbin_panel->children(); i++) {
    Fl_Group *group = widgetbin_panel->child(i)->as_group();
    if (!group) continue;
    Section section;
    section.source = group;
    section.code = group->label() && std::strcmp(group->label(), "Code") == 0;
    for (int j = 0; j < group->children(); j++) {
      Fl_Widget *button = group->child(j);
      if (!button->user_data()) continue;
      Tool tool;
      tool.source = button;
      tool.window = dynamic_cast<fluid::widget::Bin_Window_Button *>(button) != nullptr;
      Node *proto = typename_to_prototype(std::string(tool.type()));
      tool.placeable = !section.code && proto && proto->is_true_widget();
      section.tools.push_back(tool);
    }
    if (!section.tools.empty()) sections_.push_back(section);
  }
}

bool Tool_List::matches(const Tool &tool) const {
  if (filter_.empty()) return true;
  return lower(tool.name()).find(filter_) != std::string::npos ||
         lower(tool.type()).find(filter_) != std::string::npos;
}

void Tool_List::rebuild() {
  rows_.clear();
  bool filtering = !filter_.empty();
  for (int s = 0; s < (int)sections_.size(); s++) {
    const Section &section = sections_[s];
    std::vector<int> shown;
    for (int t = 0; t < (int)section.tools.size(); t++)
      if (matches(section.tools[t])) shown.push_back(t);
    if (filtering && shown.empty()) continue;
    rows_.push_back(Row{s, -1, (int)shown.size()});
    // While searching, expand every group that has a match.
    if (!section.open && !filtering) continue;
    if (!section.code) rows_.push_back(Row{s, -2, 0});
    for (int t : shown) rows_.push_back(Row{s, t, 0});
  }
  hover_ = -1;
  arrange();
  redraw();
}

void Tool_List::filter(const char *text) {
  std::string next = lower(text);
  if (next == filter_) return;
  filter_ = next;
  top_ = 0;
  rebuild();
}

int Tool_List::total() const {
  int all = 0;
  for (const Row &row : rows_) all += row_height(row);
  return all;
}

int Tool_List::view_w() const {
  return std::max(1, w() - (bar_ && bar_->visible() ? bar_->w() : 0));
}

void Tool_List::arrange() {
  int all = total();
  int bar_w = Fl::scrollbar_size();
  if (all > h()) {
    top_ = std::max(0, std::min(top_, all - h()));
    bar_->resize(x() + w() - bar_w, y(), bar_w, h());
    bar_->value(top_, h(), 0, all);
    bar_->linesize(kRow);
    if (!bar_->visible()) bar_->show();
  } else {
    top_ = 0;
    if (bar_->visible()) bar_->hide();
  }
}

void Tool_List::resize(int x, int y, int w, int h) {
  Fl_Widget::resize(x, y, w, h);
  arrange();
  redraw();
}

int Tool_List::row_top(int index) const {
  int at = y() - top_;
  for (int i = 0; i < index && i < (int)rows_.size(); i++) at += row_height(rows_[i]);
  return at;
}

int Tool_List::row_at(int my) const {
  int at = y() - top_;
  for (int i = 0; i < (int)rows_.size(); i++) {
    int next = at + row_height(rows_[i]);
    if (my >= at && my < next) return i;
    at = next;
  }
  return -1;
}

const Tool_List::Tool *Tool_List::tool_at(int row) const {
  if (row < 0 || row >= (int)rows_.size() || rows_[row].tool < 0) return nullptr;
  return &sections_[rows_[row].section].tools[rows_[row].tool];
}

int Tool_List::selected_row() const {
  if (sel_section_ < 0) return -1;
  for (int i = 0; i < (int)rows_.size(); i++) {
    if (rows_[i].section == sel_section_ && rows_[i].tool == sel_tool_) return i;
  }
  return -1;
}

void Tool_List::draw_row(int index, int X, int Y, int W) {
  const Row &row = rows_[index];
  const Section &section = sections_[row.section];
  if (row.tool == -1) {
    Fl_Color head = fl_color_average(FL_BACKGROUND_COLOR, FL_BACKGROUND2_COLOR, 0.75f);
    fl_rectf(X, Y, W, kHead, index == hover_ ? fl_color_average(head, FL_FOREGROUND_COLOR, 0.93f) : head);
    fl_color(fl_color_average(FL_BACKGROUND_COLOR, FL_FOREGROUND_COLOR, 0.80f));
    fl_xyline(X, Y + kHead - 1, X + W - 1);
    int cx = X + 12;
    int cy = Y + kHead / 2;
    fl_color(fl_color_average(FL_FOREGROUND_COLOR, head, 0.75f));
    if (section.open || !filter_.empty()) fl_polygon(cx - 4, cy - 2, cx + 4, cy - 2, cx, cy + 3);
    else fl_polygon(cx - 2, cy - 4, cx + 3, cy, cx - 2, cy + 4);
    std::string count = std::to_string(row.count);
    fl_font(FL_HELVETICA, 11);
    int count_w = (int)fl_width(count.c_str());
    fl_color(muted(head));
    fl_draw(count.c_str(), X + W - count_w - 8, Y, count_w, kHead, FL_ALIGN_LEFT, nullptr, 0);
    fl_font(FL_HELVETICA_BOLD, 12);
    fl_color(FL_FOREGROUND_COLOR);
    fl_draw(section.title() ? ui_text(section.title()) : "", X + 24, Y, std::max(1, W - 24 - count_w - 16), kHead,
            FL_ALIGN_LEFT | FL_ALIGN_CLIP, nullptr, 0);
    return;
  }
  bool chosen = row.section == sel_section_ && row.tool == sel_tool_;
  Fl_Color back = FL_BACKGROUND2_COLOR;
  if (chosen) back = FL_SELECTION_COLOR;
  else if (index == hover_) back = fl_color_average(FL_SELECTION_COLOR, FL_BACKGROUND2_COLOR, 0.14f);
  if (back != FL_BACKGROUND2_COLOR) fl_rectf(X, Y, W, kRow, back);
  Fl_Color ink = chosen ? fl_contrast(FL_FOREGROUND_COLOR, FL_SELECTION_COLOR) : FL_FOREGROUND_COLOR;
  if (row.tool == -2) {
    draw_arrow(X + 28, Y + 4, ink);
    fl_font(FL_HELVETICA, 12);
    fl_color(ink);
    fl_draw(ui_text("Pointer"), X + 50, Y, std::max(1, W - 56), kRow, FL_ALIGN_LEFT | FL_ALIGN_CLIP, nullptr, 0);
    return;
  }
  const Tool &tool = section.tools[row.tool];
  if (Fl_Image *icon = tool.source->image()) icon->draw(X + 26, Y + (kRow - icon->h()) / 2);
  int text_x = X + 50;
  fl_font(FL_HELVETICA, 12);
  int name_w = (int)fl_width(tool.name());
  fl_color(ink);
  fl_draw(tool.name(), text_x, Y, std::max(1, X + W - text_x - 6), kRow, FL_ALIGN_LEFT | FL_ALIGN_CLIP, nullptr, 0);
  // When there is room, write the class name on the right so someone looking up a widget by its English name can see it.
  const char *type = tool.type();
  if (std::strncmp(type, "Fl_", 3) == 0) {
    fl_font(FL_HELVETICA, 11);
    int type_w = (int)fl_width(type);
    if (text_x + name_w + 16 + type_w <= X + W - 8) {
      fl_color(chosen ? ink : muted(back));
      fl_draw(type, X + W - 8 - type_w, Y, type_w, kRow, FL_ALIGN_LEFT, nullptr, 0);
    }
  }
}

void Tool_List::draw() {
  if (damage() & ~FL_DAMAGE_CHILD) {
    int W = view_w();
    fl_push_clip(x(), y(), W, h());
    fl_rectf(x(), y(), W, h(), FL_BACKGROUND2_COLOR);
    int at = y() - top_;
    for (int i = 0; i < (int)rows_.size(); i++) {
      int height = row_height(rows_[i]);
      if (at + height > y() && at < y() + h()) draw_row(i, x(), at, W);
      at += height;
    }
    if (rows_.empty()) {
      fl_font(FL_HELVETICA, 12);
      fl_color(muted(FL_BACKGROUND2_COLOR));
      fl_draw(ui_text("No matching widgets"), x(), y() + 12, W, 24, FL_ALIGN_CENTER | FL_ALIGN_CLIP, nullptr, 0);
    }
    fl_pop_clip();
    if (bar_->visible()) draw_child(*bar_);
  } else if (bar_->visible()) {
    update_child(*bar_);
  }
}

void Tool_List::hover(int row) {
  if (row == hover_) return;
  hover_ = row;
  const Tool *tool = tool_at(row);
  if (row >= 0 && rows_[row].tool == -2) {
    tip_ = ui_text("Click to choose widgets, or to cancel placement.");
    Fl_Tooltip::enter_area(this, x(), row_top(row), view_w(), kRow, tip_.c_str());
  } else if (tool) {
    tip_ = std::string(tool->name()) + "  " + tool->type();
    Fl_Tooltip::enter_area(this, x(), row_top(row), view_w(), kRow, tip_.c_str());
  } else {
    Fl_Tooltip::enter_area(this, 0, 0, 0, 0, nullptr);
  }
  redraw();
}

void Tool_List::scroll_to(int top) {
  int most = std::max(0, total() - h());
  top = std::max(0, std::min(top, most));
  if (top == top_) return;
  top_ = top;
  if (bar_->visible()) bar_->value(top_, h(), 0, total());
  hover_ = -1;
  redraw();
}

void Tool_List::reveal(int row) {
  if (row < 0 || row >= (int)rows_.size()) return;
  int from = row_top(row) - y() + top_;
  int to = from + row_height(rows_[row]);
  if (from < top_) scroll_to(from);
  else if (to > top_ + h()) scroll_to(to - h());
}

void Tool_List::step(int by) {
  int row = selected_row();
  int count = (int)rows_.size();
  if (row < 0) row = by > 0 ? -1 : count;
  for (row += by; row >= 0 && row < count; row += by) {
    if (rows_[row].tool == -1) continue;
    sel_section_ = rows_[row].section;
    sel_tool_ = rows_[row].tool;
    reveal(row);
    redraw();
    return;
  }
}

// Add it to the currently selected container, the same as clicking a button in the original widget bin.
void Tool_List::add(const Tool &tool) {
  type_make_cb(nullptr, (void *)tool.type());
}

void Tool_List::show_pointer(int section) {
  sel_section_ = section;
  sel_tool_ = -2;
  redraw();
}

void Tool_List::disarm() {
  g_arm.on = false;
  g_band.down = false;
  reset_cursors();
  if (g_arm.section >= 0) show_pointer(g_arm.section);
  else redraw();
  redraw_bands();
}

void Tool_List::arm(int section, const Tool &tool) {
  g_arm.on = true;
  g_arm.window = tool.window;
  g_arm.section = section;
  g_arm.type = tool.type();
  g_band.down = false;
  redraw_bands();
}

void Tool_List::draw_arrow(int x, int y, Fl_Color ink) {
  fl_color(ink);
  fl_begin_complex_polygon();
  fl_vertex(x + 0.5, y + 0.5);
  fl_vertex(x + 0.5, y + 13.5);
  fl_vertex(x + 4.2, y + 10.2);
  fl_vertex(x + 6.6, y + 14.5);
  fl_vertex(x + 8.4, y + 13.6);
  fl_vertex(x + 6.0, y + 9.2);
  fl_vertex(x + 10.5, y + 9.2);
  fl_end_complex_polygon();
}

void Tool_List::bar_cb(Fl_Widget *, void *self) {
  auto *list = static_cast<Tool_List *>(self);
  list->top_ = list->bar_->value();
  list->hover_ = -1;
  list->redraw();
}

int Tool_List::handle(int event) {
  bool on_bar = bar_->visible() && Fl::event_inside(bar_);
  switch (event) {
    case FL_ENTER:
    case FL_MOVE:
      if (on_bar) {
        hover(-1);
        break;
      }
      hover(row_at(Fl::event_y()));
      return 1;
    case FL_LEAVE:
      hover(-1);
      return 1;
    case FL_MOUSEWHEEL:
      if (!bar_->visible()) return 0;
      scroll_to(top_ + Fl::event_dy() * kRow * 2);
      return 1;
    case FL_FOCUS:
    case FL_UNFOCUS:
      return 1;
    case FL_PUSH: {
      if (on_bar) break;
      if (Fl::event_button() != FL_LEFT_MOUSE) return 1;
      Fl::focus(this);
      int row = row_at(Fl::event_y());
      if (row < 0) return 1;
      if (rows_[row].tool == -1) {
        Section &section = sections_[rows_[row].section];
        section.open = !section.open;
        rebuild();
        return 1;
      }
      sel_section_ = rows_[row].section;
      sel_tool_ = rows_[row].tool;
      redraw();
      if (rows_[row].tool == -2) {
        // Click the pointer: cancel place mode and restore the normal pointer.
        g_arm.on = false;
        g_band.down = false;
        reset_cursors();
        redraw_bands();
        return 1;
      }
      const Tool *tool = tool_at(row);
      if (!tool) return 1;
      if (!tool->placeable) {
        // Code and menu items do not occupy space, so a click adds them to the current container. The second click of a double-click does not add another.
        g_arm.on = false;
        g_band.down = false;
        reset_cursors();
        if (Fl::event_clicks() > 0) return 1;
        Tool chosen = *tool;
        add(chosen);
        return 1;
      }
      if (Fl::event_clicks() > 0) {
        Tool chosen = *tool;
        add(chosen);
        disarm();
        return 1;
      }
      arm(rows_[row].section, *tool);
      return 1;
    }
    case FL_DRAG:
    case FL_RELEASE:
      if (on_bar) break;
      return 1;
    case FL_KEYBOARD:
      switch (Fl::event_key()) {
        case FL_Escape:
          if (g_arm.on || g_band.down) {
            disarm();
            return 1;
          }
          break;
        case FL_Up:
          step(-1);
          return 1;
        case FL_Down:
          step(1);
          return 1;
        case FL_Enter:
        case FL_KP_Enter:
          if (const Tool *tool = tool_at(selected_row())) {
            Tool chosen = *tool;
            add(chosen);
            if (chosen.placeable) disarm();
          }
          return 1;
      }
      break;
  }
  return Fl_Group::handle(event);
}

// Search field. Shows a hint line when it is empty.
class Search_Input : public Fl_Input {
public:
  Search_Input(int x, int y, int w, int h) : Fl_Input(x, y, w, h) {}

  void draw() override {
    Fl_Input::draw();
    if (size() == 0 && Fl::focus() != this && (damage() & FL_DAMAGE_ALL)) {
      fl_font(textfont(), textsize());
      fl_color(muted(color()));
      fl_draw(ui_text("Search widgets"), x() + Fl::box_dx(box()) + 4, y(), std::max(1, w() - 12), h(),
              FL_ALIGN_LEFT | FL_ALIGN_CLIP, nullptr, 0);
    }
  }

  int handle(int event) override {
    if (event == FL_KEYBOARD && Fl::event_key() == FL_Escape && size() > 0) {
      value("");
      do_callback();
      return 1;
    }
    int result = Fl_Input::handle(event);
    if (event == FL_FOCUS || event == FL_UNFOCUS) redraw();
    return result;
  }
};

// Clear button to the right of the search field.
class Clear_Button : public Fl_Button {
public:
  Clear_Button(int x, int y, int w, int h) : Fl_Button(x, y, w, h) { box(FL_NO_BOX); }

  void draw() override {
    fl_rectf(x(), y(), w(), h(), FL_BACKGROUND_COLOR);
    if (value() || Fl::belowmouse() == this)
      fl_rectf(x() + 1, y() + 1, w() - 2, h() - 2,
               fl_color_average(FL_BACKGROUND_COLOR, FL_FOREGROUND_COLOR, value() ? 0.72f : 0.84f));
    int cx = x() + w() / 2;
    int cy = y() + h() / 2;
    fl_color(FL_FOREGROUND_COLOR);
    fl_line(cx - 4, cy - 4, cx + 4, cy + 4);
    fl_line(cx - 4, cy + 4, cx + 4, cy - 4);
  }

  int handle(int event) override {
    if (event == FL_ENTER || event == FL_LEAVE) redraw();
    return Fl_Button::handle(event);
  }
};

class Toolbox : public Fl_Group {
public:
  Toolbox(int x, int y, int w, int h) : Fl_Group(x, y, w, h) {
    box(FL_FLAT_BOX);
    color(FL_BACKGROUND_COLOR);
    search_ = new Search_Input(x + 6, y + 6, std::max(20, w - 12), 24);
    search_->textsize(12);
    search_->when(FL_WHEN_CHANGED);
    search_->callback(search_cb, this);
    clear_ = new Clear_Button(x + w - 28, y + 7, 22, 22);
    clear_->callback(clear_cb, this);
    clear_->hide();
    list_ = new Tool_List(x, y + kSearchArea, w, std::max(1, h - kSearchArea));
    end();
  }

  void resize(int x, int y, int w, int h) override {
    Fl_Widget::resize(x, y, w, h);
    place();
    redraw();
  }

private:
  void place() {
    int spare = clear_->visible() ? 26 : 0;
    search_->resize(x() + 6, y() + 6, std::max(20, w() - 12 - spare), 24);
    clear_->resize(x() + w() - 6 - 22, y() + 7, 22, 22);
    list_->resize(x(), y() + kSearchArea, w(), std::max(1, h() - kSearchArea));
  }

  void apply() {
    list_->filter(search_->value());
    bool has_text = search_->size() > 0;
    if (has_text != (clear_->visible() != 0)) {
      if (has_text) clear_->show();
      else clear_->hide();
      place();
      redraw();
    }
  }

  static void search_cb(Fl_Widget *, void *self) {
    static_cast<Toolbox *>(self)->apply();
  }

  static void clear_cb(Fl_Widget *, void *self) {
    auto *box = static_cast<Toolbox *>(self);
    box->search_->value("");
    box->apply();
    Fl::focus(box->search_);
  }

  Search_Input *search_ = nullptr;
  Clear_Button *clear_ = nullptr;
  Tool_List *list_ = nullptr;
};

Node *container_at(Window_Node *form, int x, int y) {
  Node *target = form;
  if (!form) return nullptr;
  for (Node *node = form->next; node && node->level > form->level; node = node->next) {
    auto *group = dynamic_cast<Group_Node *>(node);
    if (!group || !group->is_true_widget() || !group->o || !group->o->visible_r()) continue;
    if (x < group->o->x() || y < group->o->y()) continue;
    if (x >= group->o->x() + group->o->w() || y >= group->o->y() + group->o->h()) continue;
    target = group;
  }
  return target;
}

void commit_widget(Window_Node *form) {
  int x = 0, y = 0, w = 0, h = 0;
  normalize_band(x, y, w, h);
  bool click = band_is_click();
  std::string type = g_arm.type;
  g_band.down = false;
  redraw_bands();
  Node *target = container_at(form, g_band.x0, g_band.y0);
  Window_Node::popupx = click ? g_band.x0 : x;
  Window_Node::popupy = click ? g_band.y0 : y;
  Window_Node::popupw = click ? 0x7FFFFFFF : std::max(1, w);
  Window_Node::popuph = click ? 0x7FFFFFFF : std::max(1, h);
  if (target) Fluid.proj.tree.current = target;
  if (target && target->can_have_children())
    add_new_widget_from_user(type, Strategy::AS_LAST_CHILD);
  else
    add_new_widget_from_user(type, Strategy::AFTER_CURRENT);
  Window_Node::popupx = 0x7FFFFFFF;
  Window_Node::popupy = 0x7FFFFFFF;
  Window_Node::popupw = 0x7FFFFFFF;
  Window_Node::popuph = 0x7FFFFFFF;
  if (g_list) g_list->disarm();
}

void commit_window() {
  int x = 0, y = 0, w = 0, h = 0;
  normalize_band(x, y, w, h);
  bool click = band_is_click();
  std::string type = g_arm.type;
  int vx = click ? g_band.x0 : x;
  int vy = click ? g_band.y0 : y;
  g_band.down = false;
  redraw_bands();
  designer_expect_frame(vx, vy);
  if (!click) {
    int client_w = 0, client_h = 0;
    designer_drawn_window_size(w, h, client_w, client_h);
    Window_Node::popupw = client_w;
    Window_Node::popuph = client_h;
  }
  Node *made = nullptr;
  if (Fluid.proj.tree.current && Fluid.proj.tree.current->can_have_children())
    made = add_new_widget_from_user(type, Strategy::AS_LAST_CHILD);
  else
    made = add_new_widget_from_user(type, Strategy::AFTER_CURRENT);
  Window_Node::popupw = 0x7FFFFFFF;
  Window_Node::popuph = 0x7FFFFFFF;
  if (!made) designer_clear_expect_frame();
  if (g_list) g_list->disarm();
}

void move_window_band(int vx, int vy) {
  g_band.x1 = vx;
  g_band.y1 = vy;
  redraw_bands();
}

int form_event(int event, Window_Node *form) {
  if (!form || !form->o) return 0;
  if (!g_arm.on && !g_band.down) return 0;
  Fl_Window *win = static_cast<Fl_Window *>(form->o);
  bool placing_window = g_arm.window || (g_band.down && g_band.window);
  if (placing_window) {
    switch (event) {
      case FL_ENTER:
      case FL_MOVE:
        win->cursor(FL_CURSOR_CROSS);
        return 1;
      case FL_LEAVE:
        if (!g_band.down) win->cursor(FL_CURSOR_DEFAULT);
        return 0;
      case FL_PUSH:
        if (Fl::event_button() == FL_RIGHT_MOUSE) {
          bool drawing = g_band.down;
          if (g_list) g_list->disarm();
          return drawing ? 1 : 0;
        }
        if (Fl::event_button() != FL_LEFT_MOUSE || !g_arm.on) return 0;
        g_band.down = true;
        g_band.window = true;
        g_band.form = form;
        g_band.x0 = g_band.x1 = win->x() + Fl::event_x();
        g_band.y0 = g_band.y1 = win->y() + Fl::event_y();
        redraw_bands();
        return 1;
      case FL_DRAG:
        if (!g_band.down) return 0;
        move_window_band(win->x() + Fl::event_x(), win->y() + Fl::event_y());
        return 1;
      case FL_RELEASE:
        if (!g_band.down || Fl::event_button() != FL_LEFT_MOUSE) return 0;
        g_band.x1 = win->x() + Fl::event_x();
        g_band.y1 = win->y() + Fl::event_y();
        commit_window();
        return 1;
      case FL_KEYBOARD:
        if (Fl::event_key() == FL_Escape) {
          if (g_list) g_list->disarm();
          return 1;
        }
        return 0;
      default:
        return 0;
    }
  }
  switch (event) {
    case FL_ENTER:
    case FL_MOVE:
      win->cursor(FL_CURSOR_CROSS);
      return 1;
    case FL_LEAVE:
      if (!g_band.down) win->cursor(FL_CURSOR_DEFAULT);
      return 0;
    case FL_PUSH:
      if (Fl::event_button() == FL_RIGHT_MOUSE) {
        bool drawing = g_band.down;
        if (g_list) g_list->disarm();
        return drawing ? 1 : 0;
      }
      if (Fl::event_button() != FL_LEFT_MOUSE || !g_arm.on) return 0;
      g_band.down = true;
      g_band.window = false;
      g_band.form = form;
      g_band.x0 = g_band.x1 = Fl::event_x();
      g_band.y0 = g_band.y1 = Fl::event_y();
      redraw_bands();
      return 1;
    case FL_DRAG:
      if (!g_band.down) return 0;
      g_band.x1 = Fl::event_x();
      g_band.y1 = Fl::event_y();
      redraw_bands();
      return 1;
    case FL_RELEASE:
      if (!g_band.down || Fl::event_button() != FL_LEFT_MOUSE) return 0;
      g_band.x1 = Fl::event_x();
      g_band.y1 = Fl::event_y();
      commit_widget(form);
      return 1;
    case FL_KEYBOARD:
      if (Fl::event_key() == FL_Escape) {
        if (g_list) g_list->disarm();
        return 1;
      }
      return 0;
    default:
      return 0;
  }
}

int desk_event(int event) {
  if (!g_arm.on && !g_band.down) return 0;
  bool placing_window = g_arm.window || (g_band.down && g_band.window);
  if (!placing_window) {
    if (event == FL_PUSH && Fl::event_button() == FL_RIGHT_MOUSE && !g_band.down && g_list)
      g_list->disarm();
    if (event == FL_KEYBOARD && Fl::event_key() == FL_Escape && g_arm.on && g_list) {
      g_list->disarm();
      return 1;
    }
    return 0;
  }
  switch (event) {
    case FL_ENTER:
    case FL_MOVE:
      if (Fl_Window *desk = designer_desk_viewport()) desk->cursor(FL_CURSOR_CROSS);
      return 1;
    case FL_PUSH:
      if (Fl::event_button() == FL_RIGHT_MOUSE) {
        bool drawing = g_band.down;
        if (g_list) g_list->disarm();
        return drawing ? 1 : 0;
      }
      if (Fl::event_button() != FL_LEFT_MOUSE || !g_arm.on) return 0;
      g_band.down = true;
      g_band.window = true;
      g_band.form = nullptr;
      g_band.x0 = g_band.x1 = Fl::event_x();
      g_band.y0 = g_band.y1 = Fl::event_y();
      redraw_bands();
      return 1;
    case FL_DRAG:
      if (!g_band.down) return 0;
      move_window_band(Fl::event_x(), Fl::event_y());
      return 1;
    case FL_RELEASE:
      if (!g_band.down || Fl::event_button() != FL_LEFT_MOUSE) return 0;
      g_band.x1 = Fl::event_x();
      g_band.y1 = Fl::event_y();
      commit_window();
      return 1;
    case FL_KEYBOARD:
      if (Fl::event_key() == FL_Escape) {
        if (g_list) g_list->disarm();
        return 1;
      }
      return 0;
    default:
      return 0;
  }
}

void draw_form_band(Window_Node *form) {
  if (!g_band.down || !form || !form->o) return;
  int x = 0, y = 0, w = 0, h = 0;
  normalize_band(x, y, w, h);
  if (g_band.window) {
    x -= form->o->x();
    y -= form->o->y();
  } else if (g_band.form != form) {
    return;
  }
  dash_rect(x, y, w, h);
}

void draw_desk_band() {
  if (!g_band.down || !g_band.window) return;
  int x = 0, y = 0, w = 0, h = 0;
  normalize_band(x, y, w, h);
  dash_rect(x, y, w, h);
}

}  // namespace

int toolbox_form_event(int event, Window_Node *form) {
  return form_event(event, form);
}

int toolbox_desk_event(int event) {
  return desk_event(event);
}

void toolbox_draw_form_band(Window_Node *form) {
  draw_form_band(form);
}

void toolbox_draw_desk_band() {
  draw_desk_band();
}

Fl_Group *make_toolbox(int x, int y, int w, int h) {
  return new Toolbox(x, y, w, h);
}

}  // namespace fluid
