//
// Widget properties on the left: a choice of widgets in the current window, then the editable items for that type.

#include "app/code_console.h"
#include "app/property_sheet.h"

#include "Fluid.h"
#include "app/designer_shell.h"
#include "app/owned_choice.h"
#include "app/ui_locale.h"
#include "nodes/Node.h"
#include "nodes/Widget_Node.h"
#include "nodes/Window_Node.h"
#include "widgets/Node_Browser.h"

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Date_Input.H>
#include <FL/Fl_Chart.H>
#include <FL/Fl_Shortcut_Button.H>
#include <FL/Fl_Time_Input.H>
#include <FL/fl_draw.H>

#include <cstdlib>
#include <FL/Fl_Int_Input.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Valuator.H>
#include <FL/Fl_Value_Input.H>
#include <FL/Fl_Input_.H>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <unordered_set>
#include <string>
#include <utility>
#include <vector>

extern Fl_Menu_Item boxmenu[];
extern Fl_Menu_Item whenmenu[];

namespace fluid {

namespace {

const int kRow = 24;
const int kMixed = -1000003;
// Do not write this onto the widget's user_data. FLTK's callback argument and user_data are the same pointer, so writing there overwrites the property id.
std::unordered_set<Fl_Widget *> mixed_editors;

void mark_mixed(Fl_Widget *editor) {
  if (editor) mixed_editors.insert(editor);
}

void clear_mixed() {
  mixed_editors.clear();
}

// A parameter declared auto is C++14. CI builds with -std=c++11.
template <class Reader>
bool fields_equal_text(const std::vector<Widget_Node *> &widgets, Reader reader, std::string &value) {
  value = reader(widgets.front());
  for (size_t i = 1; i < widgets.size(); i++) {
    if (reader(widgets[i]) != value) return false;
  }
  return true;
}

template <class Reader>
bool fields_equal_int(const std::vector<Widget_Node *> &widgets, Reader reader, int &value) {
  value = (int)reader(widgets.front());
  for (size_t i = 1; i < widgets.size(); i++) {
    if ((int)reader(widgets[i]) != value) return false;
  }
  return true;
}

bool still_mixed(Fl_Widget *editor) {
  if (!editor || !mixed_editors.count(editor)) return false;
  if (auto *choice = dynamic_cast<Fl_Choice *>(editor)) {
    const Fl_Menu_Item *item = choice->mvalue();
    if (item && (int)(fl_intptr_t)item->user_data() == kMixed) return true;
  } else if (auto *input = dynamic_cast<Fl_Input *>(editor)) {
    if (!dynamic_cast<Fl_Value_Input *>(editor) && (!input->value() || !input->value()[0])) return true;
  }
  mixed_editors.erase(editor);
  return false;
}
const int kLabelW = 112;
const int kEditW = 188;
const int kMore = 22;
const int kPad = 4;
const int kGap = 6;
const int kZeroBox = 1000;

enum Prop {
  PROP_NAME,
  PROP_CLASS,
  PROP_LABEL,
  PROP_TOOLTIP,
  PROP_COMMENT,
  PROP_X,
  PROP_Y,
  PROP_W,
  PROP_H,
  PROP_TYPE,
  PROP_BOX,
  PROP_COLOR,
  PROP_SELECTION_COLOR,
  PROP_LABEL_COLOR,
  PROP_LABEL_SIZE,
  PROP_LABEL_FONT,
  PROP_ALIGN,
  PROP_INSIDE,
  PROP_WRAP,
  PROP_VISIBLE,
  PROP_ACTIVE,
  PROP_CALLBACK,
  PROP_WHEN,
  PROP_USER_DATA,
  PROP_USER_DATA_TYPE,
  PROP_ACCESS,
  PROP_MINIMUM,
  PROP_MAXIMUM,
  PROP_STEP,
  PROP_MODAL,
  PROP_NON_MODAL,
  PROP_RESIZABLE,
  PROP_TEXT_SIZE,
  PROP_TEXT_FONT,
  PROP_TEXT_COLOR,
  PROP_MAX_SIZE,
  PROP_DATE,
  PROP_EARLIEST,
  PROP_LATEST,
  PROP_ALLOW_EMPTY,
  PROP_TODAY_LABEL,
  PROP_WEEKDAYS,
  PROP_TIME,
  PROP_EARLIEST_TIME,
  PROP_LATEST_TIME,
  PROP_SHOW_SECONDS,
  PROP_NOW_LABEL,
  PROP_HOUR_HEADING,
  PROP_MINUTE_HEADING,
  PROP_SECOND_HEADING,
  PROP_AUTOSIZE,
  PROP_LOWER_BOUND,
  PROP_UPPER_BOUND,
  PROP_RECORDED_KEY
};

struct NamedColor {
  const char *english;
  Fl_Color value;
};

const NamedColor kColors[] = {
  {"Foreground", FL_FOREGROUND_COLOR},
  {"Background", FL_BACKGROUND_COLOR},
  {"Background 2", FL_BACKGROUND2_COLOR},
  {"Selection", FL_SELECTION_COLOR},
  {"Inactive", FL_INACTIVE_COLOR},
  {"Black", FL_BLACK},
  {"Red", FL_RED},
  {"Green", FL_GREEN},
  {"Yellow", FL_YELLOW},
  {"Blue", FL_BLUE},
  {"Magenta", FL_MAGENTA},
  {"Cyan", FL_CYAN},
  {"White", FL_WHITE},
  {"Gray", FL_GRAY},
  {"Dark Red", FL_DARK_RED},
  {"Dark Green", FL_DARK_GREEN},
  {"Dark Blue", FL_DARK_BLUE},
};

struct AlignItem {
  const char *english;
  int value;
};

const AlignItem kAligns[] = {
  {"center", FL_ALIGN_CENTER},
  {"top", FL_ALIGN_TOP},
  {"bottom", FL_ALIGN_BOTTOM},
  {"left", FL_ALIGN_LEFT},
  {"right", FL_ALIGN_RIGHT},
  {"top left", FL_ALIGN_TOP_LEFT},
  {"top right", FL_ALIGN_TOP_RIGHT},
  {"bottom left", FL_ALIGN_BOTTOM_LEFT},
  {"bottom right", FL_ALIGN_BOTTOM_RIGHT},
};

class Property_Page : public Fl_Group {
public:
  Fl_Choice *picker = nullptr;
  Fl_Scroll *scroll = nullptr;
  Fl_Group *body = nullptr;
  Node *node = nullptr;
  bool loading = false;
  bool multi_ = false;
  bool batch_ = false;
  std::vector<Widget_Node *> many_;
  std::vector<std::string> picker_labels;
  std::vector<Fl_Menu_Item> picker_items;
  struct Row {
    Fl_Widget *label;
    Fl_Widget *editor;
    Fl_Widget *extra;
    // C++11 does not treat a class with default member values as an aggregate.
    Row() : label(nullptr), editor(nullptr), extra(nullptr) {}
    Row(Fl_Widget *label, Fl_Widget *editor, Fl_Widget *extra)
      : label(label), editor(editor), extra(extra) {}
  };
  std::vector<Row> rows_;

  Property_Page(int x, int y, int w, int h);
  void load(Node *next);
  void load_many(const std::vector<Widget_Node *> &widgets);
  bool same_selection(const std::vector<Widget_Node *> &widgets) const;
  void resize(int x, int y, int w, int h) override;
  void apply(Prop prop, Fl_Widget *editor);
  void apply_font(Prop prop, Fl_Font font, int size);

private:
  void layout_body();
  void place_rows(int x0, int y0);
  void clear_body();
  void fill_picker(Window_Node *window, Node *selected);
  void add_label(const char *english);
  void attach(Fl_Widget *editor, Fl_Widget *extra = nullptr);
  void add_text(Prop prop, const char *english, const std::string &value, bool mixed = false);
  void add_multiline(Prop prop, const char *english, const std::string &value, bool mixed = false);
  void add_font(Prop prop, const char *english, Fl_Font font, int size, bool mixed = false);
  void add_int(Prop prop, const char *english, int value, bool mixed = false);
  void add_real(Prop prop, const char *english, double value, bool mixed = false);
  void add_switch(Prop prop, const char *english, bool on);
  void add_choice(Prop prop, const char *english, int current, const std::vector<std::pair<std::string, int>> &items, bool mixed = false);
  void add_widget_rows(Widget_Node *widget);
  void touch(Widget_Node *widget);
};

Property_Page *g_page = nullptr;
Node *g_pending = nullptr;

bool node_alive(Node *node) {
  if (!node) return false;
  for (Node *item = Fluid.proj.tree.first; item; item = item->next)
    if (item == node) return true;
  return false;
}

void picker_later(void *) {
  Node *node = g_pending;
  g_pending = nullptr;
  if (node_alive(node)) select_only(node);
}

void picker_cb(Fl_Widget *widget, void *) {
  auto *page = static_cast<Property_Page *>(widget->user_data());
  if (!page || page->loading) return;
  const Fl_Menu_Item *item = page->picker->mvalue();
  if (!item) return;
  g_pending = static_cast<Node *>(item->user_data());
  Fl::remove_timeout(picker_later);
  Fl::add_timeout(0, picker_later);
}

void field_cb(Fl_Widget *widget, void *data) {
  if (!g_page || g_page->loading) return;
  g_page->apply(static_cast<Prop>((fl_intptr_t)data), widget);
}

std::string widget_caption(Node *node) {
  std::string type = subclassname(node);
  if (type.compare(0, 3, "Fl_") == 0) type.erase(0, 3);
  if (type.empty()) type = node->type_name();
  std::string caption = ui_text(type.c_str());
  std::string extra = node->name();
  if (extra.empty()) extra = node->label();
  for (char &c : extra) {
    if (c == '\n' || c == '\r' || c == '/' || c == '|') c = ' ';
  }
  if (!extra.empty()) caption += "  " + extra;
  return caption;
}

int menu_value(const Fl_Menu_Item *item) {
  return (int)(fl_intptr_t)item->user_data();
}

std::vector<std::pair<std::string, int>> menu_items(Fl_Menu_Item *menu, bool skip_toggles) {
  std::vector<std::pair<std::string, int>> items;
  if (!menu) return items;
  for (Fl_Menu_Item *item = menu; item->text; item++) {
    if (item->flags & FL_SUBMENU) continue;
    if (skip_toggles && (item->flags & FL_MENU_TOGGLE)) continue;
    if (!item->label() || !item->label()[0]) continue;
    items.push_back(std::make_pair(std::string(ui_text(item->label())), menu_value(item)));
  }
  return items;
}

std::vector<std::pair<std::string, int>> color_items(Fl_Color current) {
  std::vector<std::pair<std::string, int>> items;
  bool found = false;
  for (const NamedColor &color : kColors) {
    items.push_back(std::make_pair(std::string(ui_text(color.english)), (int)color.value));
    if (color.value == current) found = true;
  }
  if (!found) {
    items.insert(items.begin(), std::make_pair(std::to_string((int)current), (int)current));
  }
  return items;
}

std::vector<std::pair<std::string, int>> align_items() {
  std::vector<std::pair<std::string, int>> items;
  for (const AlignItem &item : kAligns)
    items.push_back(std::make_pair(std::string(ui_text(item.english)), item.value));
  return items;
}

std::vector<std::pair<std::string, int>> box_items() {
  std::vector<std::pair<std::string, int>> items;
  int blanks = 0;
  for (int i = 0; blanks < 2 && i < 80; i++) {
    if (!boxmenu[i].text) {
      blanks++;
      continue;
    }
    blanks = 0;
    if (boxmenu[i].flags & FL_SUBMENU) continue;
    if (!boxmenu[i].label() || !boxmenu[i].label()[0]) continue;
    int value = menu_value(&boxmenu[i]);
    if (value == kZeroBox) value = FL_NO_BOX;
    items.push_back(std::make_pair(std::string(ui_text(boxmenu[i].label())), value));
  }
  return items;
}

Property_Page::Property_Page(int x, int y, int w, int h) : Fl_Group(x, y, w, h, "Widget Properties") {
  box(FL_FLAT_BOX);
  color(FL_BACKGROUND_COLOR);
  picker = new Fl_Choice(x + 4, y + 4, std::max(20, w - 8), 24);
  picker->textsize(12);
  picker->copy_tooltip(ui_text("Select a widget in this window."));
  picker->user_data(this);
  picker->callback(picker_cb);
  scroll = new Fl_Scroll(x + 2, y + 32, std::max(20, w - 4), std::max(20, h - 36));
  scroll->type(Fl_Scroll::BOTH);
  scroll->box(FL_FLAT_BOX);
  body = new Fl_Group(scroll->x(), scroll->y(), std::max(20, scroll->w() - 16), 4);
  body->box(FL_NO_BOX);
  body->end();
  scroll->end();
  end();
  resizable(scroll);
}

void Property_Page::resize(int x, int y, int w, int h) {
  // Do not use Fl_Group::resize. It would stretch or squeeze the editors back toward their initial size.
  Fl_Widget::resize(x, y, w, h);
  if (picker) picker->resize(x + 4, y + 4, std::max(20, w - 8), 24);
  if (scroll) scroll->resize(x + 2, y + 32, std::max(20, w - 4), std::max(20, h - 36));
  layout_body();
  redraw();
}

void Property_Page::place_rows(int x0, int y0) {
  int edit_x = x0 + kPad + kLabelW + kGap;
  int y = y0;
  for (size_t i = 0; i < rows_.size(); i++) {
    Row &row = rows_[i];
    if (row.label) row.label->resize(x0 + kPad, y, kLabelW, kRow);
    int edit_w = kEditW;
    if (row.extra) {
      edit_w = kEditW - kMore - 2;
      row.extra->resize(edit_x + edit_w + 2, y, kMore, kRow);
    }
    if (row.editor) row.editor->resize(edit_x, y, edit_w, kRow);
    y += kRow;
    if (i + 1 < rows_.size()) y += 1;
  }
}

void Property_Page::layout_body() {
  if (!scroll || !body) return;
  if (scroll->xposition() || scroll->yposition())
    scroll->scroll_to(0, 0);
  int x0 = scroll->x();
  int y0 = scroll->y();
  int content_w = kPad + kLabelW + kGap + kEditW;
  int content_h = (int)rows_.size() * kRow;
  if (rows_.size() > 1) content_h += (int)rows_.size() - 1;
  content_h = std::max(1, content_h);
  int bar = Fl::scrollbar_size();
  if (bar < 8) bar = 16;
  bool need_h = content_w > scroll->w();
  bool need_v = content_h > scroll->h();
  if (need_v && content_w > scroll->w() - bar) need_h = true;
  if (need_h && content_h > scroll->h() - bar) need_v = true;
  int body_w = need_h ? content_w : std::min(content_w, scroll->w());
  int body_h = need_v ? content_h : std::min(content_h, scroll->h());
  body->resizable(nullptr);
  body->resize(x0, y0, body_w, body_h);
  place_rows(x0, y0);
  scroll->redraw();
}

void Property_Page::clear_body() {
  clear_mixed();
  if (!body) return;
  Fl_Widget *focus = Fl::focus();
  if (focus && contains(focus)) {
    if (Fl_Widget *host = window()) Fl::focus(host);
  }
  rows_.clear();
  // end() sets the current group to the parent. That parent is the property scroller and must not stay current,
  // or the next widget created with new would be parented into the property page.
  Fl_Group *saved = Fl_Group::current();
  body->begin();
  for (int i = body->children() - 1; i >= 0; i--) {
    Fl_Widget *child = body->child(i);
    body->remove(child);
    delete child;
  }
  body->end();
  Fl_Group::current(saved);
}

void Property_Page::add_label(const char *english) {
  auto *box = new Fl_Box(0, 0, 10, kRow);
  box->copy_label(ui_text(english));
  box->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_CLIP);
  box->labelsize(12);
  box->box(FL_THIN_DOWN_BOX);
  box->color(FL_BACKGROUND_COLOR);
  rows_.push_back(Row{box, nullptr, nullptr});
}

void Property_Page::attach(Fl_Widget *editor, Fl_Widget *extra) {
  if (rows_.empty()) return;
  rows_.back().editor = editor;
  rows_.back().extra = extra;
}

void Property_Page::add_text(Prop prop, const char *english, const std::string &value, bool mixed) {
  add_label(english);
  auto *input = new Fl_Input(0, 0, 10, kRow);
  input->textsize(12);
  input->when(FL_WHEN_RELEASE);
  input->value(mixed ? "" : value.c_str());
  if (mixed) {
    input->tooltip(ui_text("Multiple values"));
    mark_mixed(input);
  }
  input->callback(field_cb, (void *)(fl_intptr_t)prop);
  attach(input);
}

struct Font_Face {
  std::string name;
  int style[4];
};

std::vector<Font_Face> font_faces() {
  std::vector<Font_Face> faces;
  int count = fluid::ensure_font_catalog();
  if (count < 1) count = 16;
  for (int i = 0; i < count; i++) {
    int attr = 0;
    const char *raw = Fl::get_font_name((Fl_Font)i, &attr);
    if (!raw || !raw[0]) continue;
    int slot = 0;
    if ((attr & FL_BOLD) && (attr & FL_ITALIC)) slot = 3;
    else if (attr & FL_ITALIC) slot = 2;
    else if (attr & FL_BOLD) slot = 1;
    Font_Face *face = nullptr;
    for (Font_Face &item : faces) {
      if (item.name == raw) {
        face = &item;
        break;
      }
    }
    if (!face) {
      faces.push_back(Font_Face());
      face = &faces.back();
      face->name = raw;
      face->style[0] = face->style[1] = face->style[2] = face->style[3] = -1;
    }
    face->style[slot] = i;
  }
  return faces;
}

int font_style_slot(int attr) {
  if ((attr & FL_BOLD) && (attr & FL_ITALIC)) return 3;
  if (attr & FL_ITALIC) return 2;
  if (attr & FL_BOLD) return 1;
  return 0;
}

int font_id_for(const Font_Face &face, int slot) {
  if (slot >= 0 && slot < 4 && face.style[slot] >= 0) return face.style[slot];
  for (int id : face.style)
    if (id >= 0) return id;
  return 0;
}

std::string font_summary(Fl_Font font, int size) {
  int attr = 0;
  const char *name = Fl::get_font_name(font, &attr);
  std::string text = name && name[0] ? name : "?";
  if (attr & FL_BOLD) text += std::string(" ") + ui_text("Bold");
  if (attr & FL_ITALIC) text += std::string(" ") + ui_text("Italic");
  text += ", ";
  text += std::to_string(size);
  return text;
}

struct Font_Dialog {
  bool ok = false;
  Fl_Font font = 0;
  int size = 14;
  std::vector<Font_Face> faces;
  Fl_Hold_Browser *families = nullptr;
  Fl_Hold_Browser *styles = nullptr;
  Fl_Hold_Browser *sizes = nullptr;
  Fl_Box *preview = nullptr;
};

void font_dialog_preview(Font_Dialog *dialog) {
  int family = dialog->families->value() - 1;
  int style = dialog->styles->value() - 1;
  int size_row = dialog->sizes->value();
  if (family < 0 || family >= (int)dialog->faces.size()) return;
  if (style < 0) style = 0;
  const char *size_text = size_row > 0 ? dialog->sizes->text(size_row) : nullptr;
  int size = size_text ? std::max(1, atoi(size_text)) : dialog->size;
  dialog->font = (Fl_Font)font_id_for(dialog->faces[family], style);
  dialog->size = size;
  dialog->preview->labelfont(dialog->font);
  dialog->preview->labelsize(std::min(size, 48));
  dialog->preview->redraw();
}

void font_dialog_cb(Fl_Widget *widget, void *data) {
  auto *dialog = static_cast<Font_Dialog *>(data);
  font_dialog_preview(dialog);
  if (Fl::event_clicks()) {
    dialog->ok = true;
    widget->window()->hide();
  }
}

bool choose_font(Fl_Font &font, int &size) {
  Fl_Group *saved = Fl_Group::current();
  Fl_Group::current(nullptr);
  Font_Dialog dialog;
  dialog.font = font;
  dialog.size = std::max(1, size);
  dialog.faces = font_faces();
  int attr = 0;
  const char *current_name = Fl::get_font_name(font, &attr);
  int current_slot = font_style_slot(attr);
  Fl_Window win(560, 420, ui_text("Font"));
  win.set_modal();
  if (Fluid.main_window)
    win.position(Fluid.main_window->x() + 60, Fluid.main_window->y() + 40);
  auto *family_label = new Fl_Box(12, 8, 250, 20, ui_text("Font"));
  family_label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
  family_label->labelsize(12);
  dialog.families = new Fl_Hold_Browser(12, 30, 250, 250);
  dialog.families->textsize(12);
  auto *style_label = new Fl_Box(272, 8, 140, 20, ui_text("Font Style"));
  style_label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
  style_label->labelsize(12);
  dialog.styles = new Fl_Hold_Browser(272, 30, 140, 250);
  dialog.styles->textsize(12);
  const char *style_names[] = {"Regular", "Bold", "Italic", "Bold Italic"};
  for (const char *name : style_names) dialog.styles->add(ui_text(name));
  auto *size_label = new Fl_Box(422, 8, 126, 20, ui_text("Size"));
  size_label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
  size_label->labelsize(12);
  dialog.sizes = new Fl_Hold_Browser(422, 30, 126, 250);
  dialog.sizes->textsize(12);
  const int kSizes[] = {8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 32, 36, 48, 72};
  bool size_listed = false;
  for (int item : kSizes) {
    dialog.sizes->add(std::to_string(item).c_str());
    if (item == dialog.size) size_listed = true;
  }
  if (!size_listed) dialog.sizes->add(std::to_string(dialog.size).c_str());
  auto *preview_label = new Fl_Box(12, 288, 80, 20, ui_text("Preview"));
  preview_label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
  preview_label->labelsize(12);
  dialog.preview = new Fl_Box(12, 310, 536, 58, "AaBbYyZz");
  dialog.preview->box(FL_DOWN_BOX);
  dialog.preview->color(FL_BACKGROUND2_COLOR);
  dialog.preview->labelfont(font);
  dialog.preview->labelsize(std::min(dialog.size, 48));
  int family_index = 1;
  for (int i = 0; i < (int)dialog.faces.size(); i++) {
    dialog.families->add(dialog.faces[i].name.c_str());
    if (current_name && dialog.faces[i].name == current_name) family_index = i + 1;
  }
  if (dialog.families->size() > 0) dialog.families->value(family_index);
  dialog.styles->value(current_slot + 1);
  for (int i = 1; i <= dialog.sizes->size(); i++) {
    if (dialog.sizes->text(i) && atoi(dialog.sizes->text(i)) == dialog.size) dialog.sizes->value(i);
  }
  dialog.families->callback(font_dialog_cb, &dialog);
  dialog.styles->callback(font_dialog_cb, &dialog);
  dialog.sizes->callback(font_dialog_cb, &dialog);
  auto *ok = new Fl_Return_Button(350, 380, 90, 28, ui_text("OK"));
  auto *cancel = new Fl_Button(448, 380, 100, 28, ui_text("Cancel"));
  ok->callback([](Fl_Widget *widget, void *data) {
    auto *pick = static_cast<Font_Dialog *>(data);
    font_dialog_preview(pick);
    pick->ok = true;
    widget->window()->hide();
  }, &dialog);
  cancel->callback([](Fl_Widget *widget, void *) { widget->window()->hide(); });
  win.end();
  Fl_Group::current(saved);
  font_dialog_preview(&dialog);
  win.show();
  while (win.shown()) Fl::wait();
  if (!dialog.ok) return false;
  font = dialog.font;
  size = dialog.size;
  return true;
}

std::string property_text(Node *node, Prop prop) {
  if (!node) return {};
  switch (prop) {
    case PROP_LABEL: return node->label();
    case PROP_TOOLTIP:
      if (auto *widget = dynamic_cast<Widget_Node *>(node)) return widget->tooltip();
      return {};
    case PROP_USER_DATA: return node->user_data();
    default: return {};
  }
}

bool edit_multiline(const char *title, std::string &text) {
  Fl_Group *saved = Fl_Group::current();
  Fl_Group::current(nullptr);
  Fl_Window win(480, 320, title);
  win.set_modal();
  if (Fluid.main_window)
    win.position(Fluid.main_window->x() + 80, Fluid.main_window->y() + 60);
  Fl_Text_Buffer buffer;
  buffer.text(text.c_str());
  auto *editor = new Fl_Text_Editor(8, 8, 464, 264);
  editor->buffer(&buffer);
  editor->textsize(14);
  editor->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
  bool accepted = false;
  auto *ok = new Fl_Return_Button(278, 282, 90, 28, ui_text("OK"));
  auto *cancel = new Fl_Button(376, 282, 96, 28, ui_text("Cancel"));
  ok->callback([](Fl_Widget *widget, void *flag) {
    *static_cast<bool *>(flag) = true;
    widget->window()->hide();
  }, &accepted);
  cancel->callback([](Fl_Widget *widget, void *) { widget->window()->hide(); });
  win.end();
  Fl_Group::current(saved);
  win.show();
  while (win.shown()) Fl::wait();
  editor->buffer(nullptr);
  if (!accepted) return false;
  char *raw = buffer.text();
  text = raw ? raw : "";
  free(raw);
  return true;
}

void multiline_cb(Fl_Widget *button, void *data) {
  if (!g_page || g_page->loading || !g_page->node) return;
  auto *preview = static_cast<Fl_Output *>(button->user_data());
  Prop prop = static_cast<Prop>((fl_intptr_t)data);
  std::string text = property_text(g_page->node, prop);
  if (preview && mixed_editors.count(preview)) text.clear();
  if (!edit_multiline(button->tooltip() ? button->tooltip() : ui_text("Edit"), text)) return;
  if (preview) preview->value(text.c_str());
  field_cb(preview ? static_cast<Fl_Widget *>(preview) : button, data);
}

void font_button_cb(Fl_Widget *button, void *data) {
  if (!g_page || g_page->loading) return;
  auto *widget = dynamic_cast<Widget_Node *>(g_page->node);
  if (!widget || !widget->o) return;
  Prop prop = static_cast<Prop>((fl_intptr_t)data);
  Fl_Font font = widget->o->labelfont();
  int size = widget->o->labelsize();
  Fl_Color color = 0;
  if (prop == PROP_TEXT_FONT) {
    if (!widget->textstuff(0, font, size, color)) return;
  }
  if (!choose_font(font, size)) return;
  g_page->apply_font(prop, font, size);
  if (auto *preview = static_cast<Fl_Output *>(button->user_data()))
    preview->value(font_summary(font, size).c_str());
}

void Property_Page::add_multiline(Prop prop, const char *english, const std::string &value, bool mixed) {
  add_label(english);
  auto *preview = new Fl_Output(0, 0, 10, kRow);
  preview->textsize(12);
  preview->value(mixed ? ui_text("Multiple values") : value.c_str());
  if (mixed) mark_mixed(preview);
  preview->callback(field_cb, (void *)(fl_intptr_t)prop);
  auto *button = new Fl_Button(0, 0, 10, kRow, "...");
  button->labelsize(12);
  button->tooltip(ui_text(english));
  button->user_data(preview);
  button->callback(multiline_cb, (void *)(fl_intptr_t)prop);
  attach(preview, button);
}

void Property_Page::add_font(Prop prop, const char *english, Fl_Font font, int size, bool mixed) {
  add_label(english);
  if (mixed) {
    auto *preview = new Fl_Output(0, 0, 10, kRow);
    preview->textsize(12);
    preview->value(ui_text("Multiple values"));
    auto *button = new Fl_Button(0, 0, 10, kRow, "...");
    button->labelsize(12);
    button->tooltip(ui_text("Font"));
    button->user_data(preview);
    button->callback(font_button_cb, (void *)(fl_intptr_t)prop);
    attach(preview, button);
    return;
  }
  auto *preview = new Fl_Output(0, 0, 10, kRow);
  preview->textsize(12);
  preview->value(font_summary(font, size).c_str());
  auto *button = new Fl_Button(0, 0, 10, kRow, "...");
  button->labelsize(12);
  button->tooltip(ui_text("Font"));
  button->user_data(preview);
  button->callback(font_button_cb, (void *)(fl_intptr_t)prop);
  attach(preview, button);
}

void Property_Page::add_int(Prop prop, const char *english, int value, bool mixed) {
  add_label(english);
  auto *input = new Fl_Int_Input(0, 0, 10, kRow);
  input->textsize(12);
  input->when(FL_WHEN_RELEASE);
  input->value(mixed ? "" : std::to_string(value).c_str());
  if (mixed) {
    input->tooltip(ui_text("Multiple values"));
    mark_mixed(input);
  }
  input->callback(field_cb, (void *)(fl_intptr_t)prop);
  attach(input);
}

void Property_Page::add_real(Prop prop, const char *english, double value, bool mixed) {
  add_label(english);
  auto *input = new Fl_Value_Input(0, 0, 10, kRow);
  input->textsize(12);
  input->when(mixed ? FL_WHEN_CHANGED : FL_WHEN_RELEASE);
  input->value(value);
  if (mixed) {
    input->tooltip(ui_text("Multiple values"));
    input->textcolor(FL_INACTIVE_COLOR);
    mark_mixed(input);
  }
  input->callback(field_cb, (void *)(fl_intptr_t)prop);
  attach(input);
}

void Property_Page::add_switch(Prop prop, const char *english, bool on) {
  std::vector<std::pair<std::string, int>> items;
  items.push_back(std::make_pair(std::string(ui_text("No")), 0));
  items.push_back(std::make_pair(std::string(ui_text("Yes")), 1));
  add_choice(prop, english, on ? 1 : 0, items);
}

void Property_Page::add_choice(Prop prop, const char *english, int current,
                               const std::vector<std::pair<std::string, int>> &items, bool mixed) {
  add_label(english);
  auto *choice = new Owned_Choice(0, 0, 10, kRow);
  choice->textsize(12);
  choice->callback(field_cb, (void *)(fl_intptr_t)prop);
  std::vector<std::pair<std::string, int>> shown = items;
  int selected = current;
  if (mixed) {
    shown.insert(shown.begin(), std::make_pair(std::string(ui_text("Multiple values")), kMixed));
    selected = kMixed;
    mark_mixed(choice);
  }
  choice->set_items(shown, selected);
  attach(choice);
}

void Property_Page::touch(Widget_Node *widget) {
  if (!batch_) Fluid.proj.undo.checkpoint();
  Fluid.proj.set_modflag(1);
  if (widget) widget->redraw();
  if (widget_browser) widget_browser->redraw();
  if (dynamic_cast<Window_Node *>(widget)) designer_note_geometry();
}

void Property_Page::add_widget_rows(Widget_Node *widget) {
  Fl_Widget *box = widget->o;
  if (!box) return;
  int x = box->x();
  int y = box->y();
  designer_stored_xy(box, x, y);
  // Items every widget has come first, so they stay put when the widget changes.
  add_int(PROP_X, "X", x);
  add_int(PROP_Y, "Y", y);
  add_int(PROP_W, "Width", box->w());
  add_int(PROP_H, "Height", box->h());
  if (!widget->is_true_widget()) return;

  add_choice(PROP_BOX, "Box", box->box(), box_items());
  add_choice(PROP_COLOR, "Color", box->color(), color_items(box->color()));
  add_choice(PROP_SELECTION_COLOR, "Selection Color", box->selection_color(), color_items(box->selection_color()));
  add_choice(PROP_LABEL_COLOR, "Label Color", box->labelcolor(), color_items(box->labelcolor()));
  add_font(PROP_LABEL_FONT, "Label Font", box->labelfont(), box->labelsize());
  add_choice(PROP_ALIGN, "Align", box->align() & FL_ALIGN_POSITION_MASK, align_items());
  add_switch(PROP_INSIDE, "Inside", (box->align() & FL_ALIGN_INSIDE) != 0);
  add_switch(PROP_WRAP, "Wrap", (box->align() & FL_ALIGN_WRAP) != 0);
  if (!dynamic_cast<Window_Node *>(widget))
    add_switch(PROP_VISIBLE, "Visible", box->visible() != 0);
  add_switch(PROP_ACTIVE, "Active", box->active() != 0);
  add_switch(PROP_RESIZABLE, "Resizable", widget->resizable() != 0);
  add_multiline(PROP_TOOLTIP, "Tooltip", widget->tooltip());
  add_choice(PROP_WHEN, "When", box->when(), menu_items(whenmenu, true));
  add_multiline(PROP_USER_DATA, "User Data", widget->user_data());
  add_text(PROP_USER_DATA_TYPE, "User Data Type", widget->user_data_type());
  std::vector<std::pair<std::string, int>> access;
  access.push_back(std::make_pair(std::string(ui_text("private")), 0));
  access.push_back(std::make_pair(std::string(ui_text("public")), 1));
  access.push_back(std::make_pair(std::string(ui_text("protected")), 2));
  add_choice(PROP_ACCESS, "Access", widget->is_public(), access);

  // Items that exist only for this widget type come after those.
  Fl_Menu_Item *types = widget->subtypes();
  if (types && types->text)
    add_choice(PROP_TYPE, "Type", box->type(), menu_items(types, false));
  Fl_Font font = 0;
  int size = 0;
  Fl_Color color = 0;
  if (widget->textstuff(0, font, size, color)) {
    add_font(PROP_TEXT_FONT, "Text Font", font, size);
    add_choice(PROP_TEXT_COLOR, "Text Color", color, color_items(color));
  }
  if (auto *value = dynamic_cast<Fl_Valuator *>(box)) {
    add_real(PROP_MINIMUM, "Minimum", value->minimum());
    add_real(PROP_MAXIMUM, "Maximum", value->maximum());
    add_real(PROP_STEP, "Step", value->step());
  }
  if (auto *input = dynamic_cast<Fl_Input_ *>(box))
    add_int(PROP_MAX_SIZE, "Maximum Length", input->maximum_size());
  if (auto *date = dynamic_cast<Fl_Date_Input *>(box)) {
    add_text(PROP_DATE, "Date", date->text());
    add_text(PROP_EARLIEST, "Earliest Date", date->minimum_text());
    add_text(PROP_LATEST, "Latest Date", date->maximum_text());
    add_switch(PROP_ALLOW_EMPTY, "Allow Empty", date->allow_empty() != 0);
    add_text(PROP_TODAY_LABEL, "Today Button", date->today_label());
    add_text(PROP_WEEKDAYS, "Weekday Labels", date->weekday_labels());
  }
  if (auto *time_input = dynamic_cast<Fl_Time_Input *>(box)) {
    add_text(PROP_TIME, "Time", time_input->text());
    add_text(PROP_EARLIEST_TIME, "Earliest Time", time_input->minimum_text());
    add_text(PROP_LATEST_TIME, "Latest Time", time_input->maximum_text());
    add_switch(PROP_SHOW_SECONDS, "Show Seconds", time_input->show_seconds() != 0);
    add_switch(PROP_ALLOW_EMPTY, "Allow Empty", time_input->allow_empty() != 0);
    add_text(PROP_NOW_LABEL, "Now Button", time_input->now_label());
    add_text(PROP_HOUR_HEADING, "Hour Heading", time_input->hour_heading());
    add_text(PROP_MINUTE_HEADING, "Minute Heading", time_input->minute_heading());
    add_text(PROP_SECOND_HEADING, "Second Heading", time_input->second_heading());
  }
  if (auto *chart = dynamic_cast<Fl_Chart *>(box)) {
    add_switch(PROP_AUTOSIZE, "Autosize", chart->autosize() != 0);
    double lower = 0, upper = 0;
    chart->bounds(&lower, &upper);
    add_real(PROP_LOWER_BOUND, "Lower Bound", lower);
    add_real(PROP_UPPER_BOUND, "Upper Bound", upper);
  }
  if (auto *key = dynamic_cast<Fl_Shortcut_Button *>(box)) {
    const char *shown = fl_shortcut_label(key->value());
    add_text(PROP_RECORDED_KEY, "Recorded Shortcut", shown ? shown : "");
  }
  if (auto *window = dynamic_cast<Window_Node *>(widget)) {
    add_switch(PROP_MODAL, "Modal", window->modal != 0);
    add_switch(PROP_NON_MODAL, "Non Modal", window->non_modal != 0);
  }
}

void Property_Page::apply_font(Prop prop, Fl_Font font, int size) {
  if (multi_ && !batch_) {
    batch_ = true;
    Fluid.proj.undo.checkpoint();
    std::vector<Widget_Node *> copy = many_;
    for (Widget_Node *widget : copy) {
      if (!node_alive(widget)) continue;
      node = widget;
      apply_font(prop, font, size);
    }
    batch_ = false;
    if (widget_browser) widget_browser->redraw();
    return;
  }
  auto *widget = dynamic_cast<Widget_Node *>(node);
  if (!widget || !widget->o) return;
  size = std::max(1, size);
  if (prop == PROP_LABEL_FONT) {
    widget->o->labelfont(font);
    widget->o->labelsize(size);
  } else if (prop == PROP_TEXT_FONT) {
    Fl_Font old_font = font;
    int old_size = size;
    Fl_Color color = 0;
    if (!widget->textstuff(0, old_font, old_size, color)) return;
    widget->textstuff(1, font, size, color);
    widget->textstuff(2, font, size, color);
  }
  touch(widget);
}

void Property_Page::fill_picker(Window_Node *window, Node *selected) {
  picker->menu(nullptr);
  std::vector<Node *> nodes;
  if (window) {
    nodes.push_back(window);
    for (Node *item = window->next; item && item->level > window->level; item = item->next) {
      if (item->is_widget()) nodes.push_back(item);
    }
  }
  picker_labels.clear();
  picker_labels.reserve(nodes.size());
  for (Node *item : nodes) picker_labels.push_back(widget_caption(item));
  picker_items.assign(nodes.size() + 1, Fl_Menu_Item());
  int index = 0;
  for (size_t i = 0; i < nodes.size(); i++) {
    picker_items[i].text = picker_labels[i].c_str();
    picker_items[i].user_data(nodes[i]);
    if (nodes[i] == selected) index = (int)i;
  }
  picker->menu(picker_items.data());
  if (!nodes.empty()) picker->value(index);
}

bool Property_Page::same_selection(const std::vector<Widget_Node *> &widgets) const {
  return multi_ && many_ == widgets;
}

void Property_Page::load(Node *next) {
  loading = true;
  multi_ = false;
  many_.clear();
  if (picker) picker->activate();
  node = next;
  Window_Node *window = nullptr;
  if (next) {
    window = dynamic_cast<Window_Node *>(next);
    if (!window) window = next->window();
  }
  if (!window && Fluid.proj.tree.first) {
    for (Node *item = Fluid.proj.tree.first; item; item = item->next) {
      if (auto *candidate = dynamic_cast<Window_Node *>(item)) {
        window = candidate;
        break;
      }
    }
  }
  fill_picker(window, next);
  clear_body();
  Fl_Group *saved = Fl_Group::current();
  body->begin();
  if (!next) {
    add_label("Select a widget");
  } else if (auto *widget = dynamic_cast<Widget_Node *>(next)) {
    add_text(PROP_CLASS, "Class", widget->subclass());
    add_text(PROP_NAME, "Name", next->name());
    if (dynamic_cast<Window_Node *>(next)) {
      if (auto *input = dynamic_cast<Fl_Input *>(rows_.back().editor))
        input->when(FL_WHEN_CHANGED | FL_WHEN_RELEASE);
    }
    add_multiline(PROP_LABEL, "Label", next->label());
    add_widget_rows(widget);
  } else {
    add_text(PROP_NAME, "Name", next->name());
    add_multiline(PROP_LABEL, "Label", next->label());
  }
  body->end();
  Fl_Group::current(saved);
  layout_body();
  loading = false;
  code_console_load(next);
}

void Property_Page::load_many(const std::vector<Widget_Node *> &widgets) {
  loading = true;
  multi_ = true;
  many_ = widgets;
  node = widgets.empty() ? nullptr : widgets.front();
  char caption[80];
  snprintf(caption, sizeof(caption), ui_text("Selected widgets: %d"), (int)widgets.size());
  picker_labels.clear();
  picker_labels.push_back(caption);
  picker_items.assign(2, Fl_Menu_Item());
  std::memset(picker_items.data(), 0, picker_items.size() * sizeof(Fl_Menu_Item));
  picker_items[0].text = picker_labels[0].c_str();
  picker->menu(picker_items.data());
  picker->value(0);
  picker->deactivate();
  clear_body();
  Fl_Group *saved = Fl_Group::current();
  body->begin();

  std::string text;
  bool mixed = !fields_equal_text(widgets, [](Widget_Node *widget) { return widget->label(); }, text);
  add_multiline(PROP_LABEL, "Label", text, mixed);

  int number = 0;
  int x = 0;
  int y = 0;
  designer_stored_xy(widgets.front()->o, x, y);
  bool same_x = true;
  bool same_y = true;
  for (size_t i = 1; i < widgets.size(); i++) {
    int ix = 0;
    int iy = 0;
    designer_stored_xy(widgets[i]->o, ix, iy);
    if (ix != x) same_x = false;
    if (iy != y) same_y = false;
  }
  add_int(PROP_X, "X", x, !same_x);
  add_int(PROP_Y, "Y", y, !same_y);
  mixed = !fields_equal_int(widgets, [](Widget_Node *widget) { return widget->o->w(); }, number);
  add_int(PROP_W, "Width", number, mixed);
  mixed = !fields_equal_int(widgets, [](Widget_Node *widget) { return widget->o->h(); }, number);
  add_int(PROP_H, "Height", number, mixed);

  bool all_windows = true;
  bool no_windows = true;
  bool all_valuator = true;
  bool all_input = true;
  bool all_text = true;
  bool same_subtypes = true;
  Fl_Menu_Item *subtypes = widgets.front()->subtypes();
  for (Widget_Node *widget : widgets) {
    bool window = dynamic_cast<Window_Node *>(widget) != nullptr;
    all_windows = all_windows && window;
    no_windows = no_windows && !window;
    all_valuator = all_valuator && dynamic_cast<Fl_Valuator *>(widget->o);
    all_input = all_input && dynamic_cast<Fl_Input_ *>(widget->o);
    Fl_Font font = 0;
    int size = 0;
    Fl_Color color = 0;
    all_text = all_text && widget->textstuff(0, font, size, color);
    if (widget->subtypes() != subtypes) same_subtypes = false;
  }

  mixed = !fields_equal_int(widgets, [](Widget_Node *widget) { return widget->o->box(); }, number);
  add_choice(PROP_BOX, "Box", number, box_items(), mixed);
  mixed = !fields_equal_int(widgets, [](Widget_Node *widget) { return widget->o->color(); }, number);
  add_choice(PROP_COLOR, "Color", number, color_items(number), mixed);
  mixed = !fields_equal_int(widgets, [](Widget_Node *widget) { return widget->o->selection_color(); }, number);
  add_choice(PROP_SELECTION_COLOR, "Selection Color", number, color_items(number), mixed);
  mixed = !fields_equal_int(widgets, [](Widget_Node *widget) { return widget->o->labelcolor(); }, number);
  add_choice(PROP_LABEL_COLOR, "Label Color", number, color_items(number), mixed);
  Fl_Font font = widgets.front()->o->labelfont();
  int size = widgets.front()->o->labelsize();
  bool same_font = true;
  for (size_t i = 1; i < widgets.size(); i++) {
    if (widgets[i]->o->labelfont() != font || widgets[i]->o->labelsize() != size) same_font = false;
  }
  add_font(PROP_LABEL_FONT, "Label Font", font, size, !same_font);
  mixed = !fields_equal_int(widgets, [](Widget_Node *widget) { return widget->o->align() & FL_ALIGN_POSITION_MASK; }, number);
  add_choice(PROP_ALIGN, "Align", number, align_items(), mixed);
  mixed = !fields_equal_int(widgets, [](Widget_Node *widget) { return (widget->o->align() & FL_ALIGN_INSIDE) ? 1 : 0; }, number);
  add_choice(PROP_INSIDE, "Inside", number, std::vector<std::pair<std::string, int>>{
    {std::string(ui_text("No")), 0}, {std::string(ui_text("Yes")), 1}}, mixed);
  mixed = !fields_equal_int(widgets, [](Widget_Node *widget) { return (widget->o->align() & FL_ALIGN_WRAP) ? 1 : 0; }, number);
  add_choice(PROP_WRAP, "Wrap", number, std::vector<std::pair<std::string, int>>{
    {std::string(ui_text("No")), 0}, {std::string(ui_text("Yes")), 1}}, mixed);
  if (no_windows) {
    mixed = !fields_equal_int(widgets, [](Widget_Node *widget) { return widget->o->visible() ? 1 : 0; }, number);
    add_choice(PROP_VISIBLE, "Visible", number, std::vector<std::pair<std::string, int>>{
      {std::string(ui_text("No")), 0}, {std::string(ui_text("Yes")), 1}}, mixed);
  }
  mixed = !fields_equal_int(widgets, [](Widget_Node *widget) { return widget->o->active() ? 1 : 0; }, number);
  add_choice(PROP_ACTIVE, "Active", number, std::vector<std::pair<std::string, int>>{
    {std::string(ui_text("No")), 0}, {std::string(ui_text("Yes")), 1}}, mixed);
  mixed = !fields_equal_int(widgets, [](Widget_Node *widget) { return widget->resizable() ? 1 : 0; }, number);
  add_choice(PROP_RESIZABLE, "Resizable", number, std::vector<std::pair<std::string, int>>{
    {std::string(ui_text("No")), 0}, {std::string(ui_text("Yes")), 1}}, mixed);
  mixed = !fields_equal_text(widgets, [](Widget_Node *widget) { return widget->tooltip(); }, text);
  add_multiline(PROP_TOOLTIP, "Tooltip", text, mixed);
  mixed = !fields_equal_int(widgets, [](Widget_Node *widget) { return widget->o->when(); }, number);
  add_choice(PROP_WHEN, "When", number, menu_items(whenmenu, true), mixed);
  mixed = !fields_equal_text(widgets, [](Widget_Node *widget) { return widget->user_data(); }, text);
  add_multiline(PROP_USER_DATA, "User Data", text, mixed);
  mixed = !fields_equal_text(widgets, [](Widget_Node *widget) { return widget->user_data_type(); }, text);
  add_text(PROP_USER_DATA_TYPE, "User Data Type", text, mixed);
  mixed = !fields_equal_int(widgets, [](Widget_Node *widget) { return widget->is_public(); }, number);
  add_choice(PROP_ACCESS, "Access", number, std::vector<std::pair<std::string, int>>{
    {std::string(ui_text("private")), 0},
    {std::string(ui_text("public")), 1},
    {std::string(ui_text("protected")), 2}}, mixed);

  if (same_subtypes && subtypes && subtypes->text) {
    mixed = !fields_equal_int(widgets, [](Widget_Node *widget) { return widget->o->type(); }, number);
    add_choice(PROP_TYPE, "Type", number, menu_items(subtypes, false), mixed);
  }
  if (all_text) {
    Fl_Font text_font = 0;
    int text_size = 0;
    Fl_Color text_color = 0;
    widgets.front()->textstuff(0, text_font, text_size, text_color);
    bool same_text_font = true;
    bool same_text_color = true;
    for (size_t i = 1; i < widgets.size(); i++) {
      Fl_Font font_i = 0;
      int size_i = 0;
      Fl_Color color_i = 0;
      widgets[i]->textstuff(0, font_i, size_i, color_i);
      if (font_i != text_font || size_i != text_size) same_text_font = false;
      if (color_i != text_color) same_text_color = false;
    }
    add_font(PROP_TEXT_FONT, "Text Font", text_font, text_size, !same_text_font);
    add_choice(PROP_TEXT_COLOR, "Text Color", text_color, color_items(text_color), !same_text_color);
  }
  if (all_valuator) {
    auto *first = static_cast<Fl_Valuator *>(widgets.front()->o);
    bool same_min = true;
    bool same_max = true;
    bool same_step = true;
    for (size_t i = 1; i < widgets.size(); i++) {
      auto *value = static_cast<Fl_Valuator *>(widgets[i]->o);
      if (value->minimum() != first->minimum()) same_min = false;
      if (value->maximum() != first->maximum()) same_max = false;
      if (value->step() != first->step()) same_step = false;
    }
    add_real(PROP_MINIMUM, "Minimum", first->minimum(), !same_min);
    add_real(PROP_MAXIMUM, "Maximum", first->maximum(), !same_max);
    add_real(PROP_STEP, "Step", first->step(), !same_step);
  }
  if (all_input) {
    mixed = !fields_equal_int(widgets, [](Widget_Node *widget) {
      return static_cast<Fl_Input_ *>(widget->o)->maximum_size();
    }, number);
    add_int(PROP_MAX_SIZE, "Maximum Length", number, mixed);
  }
  if (all_windows) {
    mixed = !fields_equal_int(widgets, [](Widget_Node *widget) {
      return static_cast<Window_Node *>(widget)->modal;
    }, number);
    add_choice(PROP_MODAL, "Modal", number, std::vector<std::pair<std::string, int>>{
      {std::string(ui_text("No")), 0}, {std::string(ui_text("Yes")), 1}}, mixed);
    mixed = !fields_equal_int(widgets, [](Widget_Node *widget) {
      return static_cast<Window_Node *>(widget)->non_modal;
    }, number);
    add_choice(PROP_NON_MODAL, "Non Modal", number, std::vector<std::pair<std::string, int>>{
      {std::string(ui_text("No")), 0}, {std::string(ui_text("Yes")), 1}}, mixed);
  }

  body->end();
  Fl_Group::current(saved);
  layout_body();
  loading = false;
  code_console_load(nullptr);
}

int choice_value(Fl_Choice *choice) {
  const Fl_Menu_Item *item = choice->mvalue();
  if (!item) return 0;
  return (int)(fl_intptr_t)item->user_data();
}

void Property_Page::apply(Prop prop, Fl_Widget *editor) {
  if (loading || !node) return;
  if (multi_ && !batch_) {
    // Names differ, so a multi-selection does not change them together. The "multiple values" placeholder is not written back until the user edits it.
    if (prop == PROP_NAME || prop == PROP_CLASS) return;
    if (still_mixed(editor)) return;
    batch_ = true;
    Fluid.proj.undo.checkpoint();
    std::vector<Widget_Node *> copy = many_;
    for (Widget_Node *widget : copy) {
      if (!node_alive(widget)) continue;
      node = widget;
      apply(prop, editor);
    }
    batch_ = false;
    if (widget_browser) widget_browser->redraw();
    return;
  }
  auto *widget = dynamic_cast<Widget_Node *>(node);
  Fl_Widget *box = widget ? widget->o : nullptr;
  auto *input = dynamic_cast<Fl_Input *>(editor);
  auto *choice = dynamic_cast<Fl_Choice *>(editor);
  auto *real = dynamic_cast<Fl_Value_Input *>(editor);
  std::string text = input ? input->value() : "";
  int number = input ? atoi(text.c_str()) : (choice ? choice_value(choice) : 0);
  bool yes = number != 0;

  switch (prop) {
  case PROP_NAME: {
    auto *window = dynamic_cast<Window_Node *>(node);
    if (window && Fl::callback_reason() == FL_REASON_CHANGED) {
      designer_preview_tab(window, text.c_str());
      return;
    }
    node->name(text);
    if (!batch_) Fluid.proj.undo.checkpoint();
    Fluid.proj.set_modflag(1);
    Fl_Widget *keep = Fl::focus();
    fill_picker(node->window() ? node->window() : window, node);
    if (keep) Fl::focus(keep);
    if (window) designer_refresh_tabs();
    return;
  }
  case PROP_CLASS:
    if (widget) widget->subclass(text);
    touch(widget);
    return;
  case PROP_LABEL:
    node->label(text);
    touch(widget);
    if (dynamic_cast<Window_Node *>(node)) {
      designer_refresh_tabs();
      if (g_page) redraw();
    }
    return;
  case PROP_TOOLTIP:
    if (widget) widget->tooltip(text);
    touch(widget);
    return;
  case PROP_COMMENT:
    node->comment(text);
    if (!batch_) Fluid.proj.undo.checkpoint();
    Fluid.proj.set_modflag(1);
    return;
  case PROP_CALLBACK:
    node->callback(text);
    if (!batch_) Fluid.proj.undo.checkpoint();
    Fluid.proj.set_modflag(1);
    return;
  case PROP_USER_DATA:
    node->user_data(text);
    if (!batch_) Fluid.proj.undo.checkpoint();
    Fluid.proj.set_modflag(1);
    return;
  case PROP_USER_DATA_TYPE:
    node->user_data_type(text);
    if (!batch_) Fluid.proj.undo.checkpoint();
    Fluid.proj.set_modflag(1);
    return;
  case PROP_ACCESS:
    if (widget) widget->public_ = number;
    if (!batch_) Fluid.proj.undo.checkpoint();
    Fluid.proj.set_modflag(1);
    if (widget_browser) widget_browser->redraw();
    return;
  default:
    break;
  }
  if (!box) return;

  int x = box->x();
  int y = box->y();
  designer_stored_xy(box, x, y);
  switch (prop) {
  case PROP_X: {
    int sx = box->x();
    int sy = box->y();
    designer_stored_xy(box, sx, sy);
    designer_set_stored_xy(box, number, sy);
    touch(widget);
    return;
  }
  case PROP_Y: {
    int sx = box->x();
    int sy = box->y();
    designer_stored_xy(box, sx, sy);
    designer_set_stored_xy(box, sx, number);
    touch(widget);
    return;
  }
  case PROP_W: box->resize(box->x(), box->y(), std::max(1, number), box->h()); break;
  case PROP_H: box->resize(box->x(), box->y(), box->w(), std::max(1, number)); break;
  case PROP_TYPE: box->type((uchar)number); break;
  case PROP_BOX: box->box((Fl_Boxtype)number); break;
  case PROP_COLOR: box->color((Fl_Color)number); break;
  case PROP_SELECTION_COLOR: box->selection_color((Fl_Color)number); break;
  case PROP_LABEL_COLOR: box->labelcolor((Fl_Color)number); break;
  case PROP_LABEL_SIZE: box->labelsize(std::max(1, number)); break;
  case PROP_LABEL_FONT: box->labelfont((Fl_Font)number); break;
  case PROP_ALIGN: {
    Fl_Align keep = box->align() & ~FL_ALIGN_POSITION_MASK;
    box->align((Fl_Align)(number | keep));
    break;
  }
  case PROP_INSIDE: {
    Fl_Align align = box->align();
    if (yes) align = (Fl_Align)(align | FL_ALIGN_INSIDE);
    else align = (Fl_Align)(align & ~FL_ALIGN_INSIDE);
    box->align(align);
    break;
  }
  case PROP_WRAP: {
    Fl_Align align = box->align();
    if (yes) align = (Fl_Align)(align | FL_ALIGN_WRAP);
    else align = (Fl_Align)(align & ~FL_ALIGN_WRAP);
    box->align(align);
    break;
  }
  case PROP_VISIBLE:
    if (yes) box->show();
    else box->hide();
    break;
  case PROP_ACTIVE:
    if (yes) box->activate();
    else box->deactivate();
    break;
  case PROP_WHEN: box->when(number); break;
  case PROP_RESIZABLE:
    if (widget) widget->resizable(yes ? 1 : 0);
    break;
  case PROP_MINIMUM:
    if (real) static_cast<Fl_Valuator *>(box)->minimum(real->value());
    break;
  case PROP_MAXIMUM:
    if (real) static_cast<Fl_Valuator *>(box)->maximum(real->value());
    break;
  case PROP_STEP:
    if (real) static_cast<Fl_Valuator *>(box)->step(real->value());
    break;
  case PROP_MAX_SIZE:
    static_cast<Fl_Input_ *>(box)->maximum_size(std::max(0, number));
    break;
  case PROP_DATE:
    if (auto *date = dynamic_cast<Fl_Date_Input *>(box)) {
      if (!input || !date->value(text.c_str())) {
        if (input) input->value(date->text());
        return;
      }
    }
    break;
  case PROP_EARLIEST:
    if (auto *date = dynamic_cast<Fl_Date_Input *>(box)) {
      if (!date->minimum_date(text.c_str())) {
        if (input) input->value(date->minimum_text());
        return;
      }
    }
    break;
  case PROP_LATEST:
    if (auto *date = dynamic_cast<Fl_Date_Input *>(box)) {
      if (!date->maximum_date(text.c_str())) {
        if (input) input->value(date->maximum_text());
        return;
      }
    }
    break;
  case PROP_TODAY_LABEL:
    if (auto *date = dynamic_cast<Fl_Date_Input *>(box)) {
      if (text.empty()) date->today_label(nullptr);
      else date->today_label(text.c_str());
    }
    break;
  case PROP_TIME:
    if (auto *time_input = dynamic_cast<Fl_Time_Input *>(box)) {
      if (!input || !time_input->value(text.c_str())) {
        if (input) input->value(time_input->text());
        return;
      }
    }
    break;
  case PROP_EARLIEST_TIME:
    if (auto *time_input = dynamic_cast<Fl_Time_Input *>(box)) {
      if (!time_input->minimum_time(text.c_str())) {
        if (input) input->value(time_input->minimum_text());
        return;
      }
    }
    break;
  case PROP_LATEST_TIME:
    if (auto *time_input = dynamic_cast<Fl_Time_Input *>(box)) {
      if (!time_input->maximum_time(text.c_str())) {
        if (input) input->value(time_input->maximum_text());
        return;
      }
    }
    break;
  case PROP_SHOW_SECONDS:
    if (auto *time_input = dynamic_cast<Fl_Time_Input *>(box)) time_input->show_seconds(yes ? 1 : 0);
    break;
  case PROP_NOW_LABEL:
    if (auto *time_input = dynamic_cast<Fl_Time_Input *>(box))
      time_input->now_label(text.empty() ? nullptr : text.c_str());
    break;
  case PROP_HOUR_HEADING:
    if (auto *time_input = dynamic_cast<Fl_Time_Input *>(box))
      time_input->hour_heading(text.empty() ? nullptr : text.c_str());
    break;
  case PROP_MINUTE_HEADING:
    if (auto *time_input = dynamic_cast<Fl_Time_Input *>(box))
      time_input->minute_heading(text.empty() ? nullptr : text.c_str());
    break;
  case PROP_SECOND_HEADING:
    if (auto *time_input = dynamic_cast<Fl_Time_Input *>(box))
      time_input->second_heading(text.empty() ? nullptr : text.c_str());
    break;
  case PROP_AUTOSIZE:
    if (auto *chart = dynamic_cast<Fl_Chart *>(box)) chart->autosize(yes ? 1 : 0);
    break;
  case PROP_LOWER_BOUND:
  case PROP_UPPER_BOUND:
    if (auto *chart = dynamic_cast<Fl_Chart *>(box)) {
      if (!real) return;
      double lower = 0, upper = 0;
      chart->bounds(&lower, &upper);
      if (prop == PROP_LOWER_BOUND) lower = real->value();
      else upper = real->value();
      chart->bounds(lower, upper);
    }
    break;
  case PROP_RECORDED_KEY:
    if (auto *key = dynamic_cast<Fl_Shortcut_Button *>(box)) {
      unsigned shortcut = 0;
      if (!text.empty()) {
        if ((text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) || (text[0] >= '0' && text[0] <= '9'))
          shortcut = (unsigned)strtoul(text.c_str(), nullptr, 0);
        else
          shortcut = fl_old_shortcut(text.c_str());
      }
      key->value(shortcut);
    }
    break;
  case PROP_WEEKDAYS:
    if (auto *date = dynamic_cast<Fl_Date_Input *>(box)) {
      if (text.empty()) date->weekday_labels(nullptr);
      else if (!date->weekday_labels(text.c_str())) {
        if (input) input->value(date->weekday_labels());
        return;
      }
    }
    break;
  case PROP_ALLOW_EMPTY:
    if (auto *date = dynamic_cast<Fl_Date_Input *>(box)) {
      date->allow_empty(yes ? 1 : 0);
      if (!yes && !date->has_date()) {
        int year, month, day;
        Fl_Date_Input::today(year, month, day);
        date->value(year, month, day);
      }
    } else if (auto *time_input = dynamic_cast<Fl_Time_Input *>(box)) {
      time_input->allow_empty(yes ? 1 : 0);
      if (!yes && !time_input->has_time()) {
        int hour, minute, second;
        Fl_Time_Input::now(hour, minute, second);
        time_input->value(hour, minute, second);
      }
    }
    break;
  case PROP_MODAL:
    if (auto *window = dynamic_cast<Window_Node *>(widget)) window->modal = yes ? 1 : 0;
    break;
  case PROP_NON_MODAL:
    if (auto *window = dynamic_cast<Window_Node *>(widget)) window->non_modal = yes ? 1 : 0;
    break;
  case PROP_TEXT_FONT:
  case PROP_TEXT_SIZE:
  case PROP_TEXT_COLOR: {
    Fl_Font font = 0;
    int size = 0;
    Fl_Color color = 0;
    if (widget && widget->textstuff(0, font, size, color)) {
      if (prop == PROP_TEXT_FONT) font = (Fl_Font)number;
      if (prop == PROP_TEXT_SIZE) size = std::max(1, number);
      if (prop == PROP_TEXT_COLOR) color = (Fl_Color)number;
      int op = prop == PROP_TEXT_FONT ? 1 : (prop == PROP_TEXT_SIZE ? 2 : 3);
      widget->textstuff(op, font, size, color);
    }
    break;
  }
  default:
    return;
  }
  touch(widget);
}

}  // namespace

Fl_Group *make_property_page(int x, int y, int w, int h) {
  g_page = new Property_Page(x, y, w, h);
  return g_page;
}

void property_page_reset() {
  if (!g_page) return;
  g_page->loading = true;
  g_page->node = nullptr;
  g_page->multi_ = false;
  g_page->many_.clear();
  g_page->loading = false;
}

bool property_page_shows(const Node *node) {
  return g_page && g_page->node == node;
}

void property_page_load(Node *node, bool force) {
  if (Fluid.proj.undo.paused_) {
    // An operation is in progress, so leave the page alone. After it resumes, the designer reloads the current node.
    designer_request_refresh();
    return;
  }
  if (!g_page || g_page->loading) return;
  // The node shown on the property page may already have been deleted (ungroup, delete). Drop it and do not touch it again.
  if (g_page->node) {
    bool alive = false;
    for (Node *item = Fluid.proj.tree.first; item && !alive; item = item->next) alive = item == g_page->node;
    if (!alive) property_page_reset();
  }
  std::vector<Widget_Node *> selected;
  for (Node *item = Fluid.proj.tree.first; item; item = item->next) {
    if (!item->selected) continue;
    if (auto *widget = dynamic_cast<Widget_Node *>(item)) {
      if (widget->o) selected.push_back(widget);
    }
  }
  if (selected.size() > 1) {
    if (!force && g_page->same_selection(selected)) return;
    g_page->load_many(selected);
    return;
  }
  if (!force && !g_page->multi_ && g_page->node == node) return;
  g_page->load(node);
}

}  // namespace fluid
