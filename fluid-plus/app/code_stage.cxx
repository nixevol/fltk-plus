//
// Editor area for code mode, laid out like Visual Studio / VS Code:
// file tabs on the top row, with Find at the right of that row; the editor in the middle; a status bar below.

#include "app/code_stage.h"

#include "Fluid.h"
#include "app/ui_locale.h"
#include "panels/codeview_panel.h"

#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>

#include <algorithm>

namespace fluid {

namespace {

const int kFileBar = 30;
const int kStatus = 28;
const int kFindH = 24;

class Code_Stage : public Fl_Group {
public:
  explicit Code_Stage(int x, int y, int w, int h);
  void open();
  void refresh();
  void resize(int x, int y, int w, int h) override;

private:
  Fl_Button *files_[4] = {};
  int file_ = 0;
  bool ready_ = false;

  void show_file(int index);
  void place();
  static void file_cb(Fl_Widget *widget, void *stage);
};

Code_Stage *g_stage = nullptr;

void take(Fl_Group *parent, Fl_Widget *child) {
  if (!child || !parent || child->parent() == parent) return;
  if (child->parent()) child->parent()->remove(*child);
  parent->add(child);
}

int text_width(const char *text) {
  fl_font(FL_HELVETICA, 12);
  return (int)fl_width(text ? text : "") + 22;
}

int text_pixels(Fl_Widget *widget) {
  if (!widget || !widget->label() || !widget->label()[0]) return 0;
  fl_font(widget->labelfont(), widget->labelsize());
  return (int)fl_width(widget->label());
}

void place_find(int x, int y, int w) {
  Fl_Group *row = cv_find_row;
  if (!row) return;
  row->resize(x, y, std::max(1, w), kFindH);
  int box_h = 22;
  int top = y + 1;
  int right = x + w;
  Fl_Widget *reveal = row->child(4);
  Fl_Widget *next = row->child(3);
  Fl_Widget *prev = row->child(2);
  Fl_Widget *input = row->child(1);
  Fl_Widget *match = row->child(0);
  Fl_Widget *spring = row->child(5);
  if (spring) spring->hide();
  if (reveal) {
    reveal->resize(right - 52, top, 52, box_h);
    right -= 56;
  }
  if (next) {
    next->resize(right - 24, top, 24, box_h);
    right -= 26;
  }
  if (prev) {
    prev->resize(right - 24, top, 24, box_h);
    right -= 26;
  }
  if (match) {
    match->resize(right - 28, top, 28, box_h);
    right -= 32;
  }
  int label_w = text_pixels(input);
  if (label_w) label_w += 8;
  if (input) input->resize(x + label_w, top, std::max(40, right - x - label_w), box_h);
  row->show();
  row->redraw();
}

void place_status(int x, int y, int w) {
  Fl_Group *row = cv_settings_row;
  if (!row) return;
  row->resize(x, y, std::max(1, w), kStatus);
  int top = y + 3;
  int at = x + 6;
  auto put = [&](Fl_Widget *widget, int width) {
    if (!widget) return;
    widget->resize(at, top, width, 22);
    widget->show();
    at += width + 6;
  };
  put(row->child(0), 64);
  put(row->child(1), 96);
  put(row->child(2), 96);
  put(row->child(3), 120);
  if (row->child(4)) row->child(4)->hide();
  if (row->child(5)) row->child(5)->hide();
  row->show();
  row->redraw();
}

Code_Stage::Code_Stage(int x, int y, int w, int h) : Fl_Group(x, y, w, h) {
  box(FL_FLAT_BOX);
  color(FL_BACKGROUND2_COLOR);
  const char *keys[4] = {"Source", "Header", "Strings", "Project"};
  for (int i = 0; i < 4; i++) {
    const char *label = ui_text(keys[i]);
    files_[i] = new Fl_Button(x, y, text_width(label), 24, keys[i]);
    files_[i]->copy_label(label);
    files_[i]->box(FL_FLAT_BOX);
    files_[i]->down_box(FL_FLAT_BOX);
    files_[i]->labelsize(12);
    files_[i]->callback(file_cb, this);
  }
  end();
}

void Code_Stage::file_cb(Fl_Widget *widget, void *stage) {
  auto *self = static_cast<Code_Stage *>(stage);
  for (int i = 0; i < 4; i++) {
    if (self->files_[i] == widget) self->show_file(i);
  }
}

void Code_Stage::show_file(int index) {
  if (index < 0 || index > 3) index = 0;
  file_ = index;
  Fl_Widget *views[4] = {cv_source, cv_header, cv_strings, cv_project};
  for (int i = 0; i < 4; i++) {
    if (!views[i]) continue;
    if (i == index) views[i]->show();
    else views[i]->hide();
    bool on = i == index;
    files_[i]->color(on ? FL_SELECTION_COLOR : FL_BACKGROUND_COLOR);
    files_[i]->labelcolor(on ? FL_WHITE : FL_FOREGROUND_COLOR);
  }
  if (cv_tab && index < cv_tab->children()) cv_tab->value(cv_tab->child(index));
  place();
  update_codeview_cb(nullptr, nullptr);
  update_codeview_position();
  redraw();
}

void Code_Stage::open() {
  codeview_ensure();
  codeview_set_embedded(true);
  if (codeview_panel && codeview_panel->visible()) codeview_panel->hide();
  take(this, cv_source);
  take(this, cv_header);
  take(this, cv_strings);
  take(this, cv_project);
  take(this, cv_find_row);
  take(this, cv_settings_row);
  if (cv_find_text) {
    cv_find_text->label("");
    cv_find_text->copy_tooltip(ui_text("Find:"));
  }
  if (cv_find_row && cv_find_row->child(2)) cv_find_row->child(2)->copy_label("\xe2\x86\x91");
  if (cv_find_row && cv_find_row->child(3)) cv_find_row->child(3)->copy_label("\xe2\x86\x93");
  if (cv_find_row && cv_find_row->child(4)) cv_find_row->child(4)->copy_label(ui_text("Reveal"));
  if (cv_settings_row && cv_settings_row->child(0))
    cv_settings_row->child(0)->copy_label(ui_text("Refresh"));
  int saved = 0;
  if (cv_tab && cv_tab->value()) saved = std::max(0, cv_tab->find(cv_tab->value()));
  if (!ready_) file_ = saved;
  ready_ = true;
  refresh();
  show_file(file_);
}

void Code_Stage::refresh() {
  const char *keys[4] = {"Source", "Header", "Strings", "Project"};
  for (int i = 0; i < 4; i++) {
    if (!files_[i]) continue;
    const char *text = ui_text(keys[i]);
    files_[i]->copy_label(text);
    files_[i]->size(text_pixels(files_[i]) + 22, files_[i]->h());
  }
  if (cv_find_text) fluid::set_widget_label(cv_find_text, "Find:");
  if (cv_autorefresh) fluid::set_widget_label(cv_autorefresh, "Auto-Refresh");
  if (cv_autoposition) fluid::set_widget_label(cv_autoposition, "Auto-Position");
  if (cv_find_row && cv_find_row->child(4)) fluid::set_widget_label(cv_find_row->child(4), "Reveal");
  if (cv_settings_row && cv_settings_row->child(0)) fluid::set_widget_label(cv_settings_row->child(0), "Refresh");
  if (ready_) place();
}

void Code_Stage::place() {
  int x = this->x();
  int y = this->y();
  int w = this->w();
  int h = this->h();
  int used = 8;
  for (int i = 0; i < 4; i++) {
    if (!files_[i]) continue;
    int width = text_pixels(files_[i]) + 22;
    files_[i]->resize(x + used, y + 3, width, 24);
    used += width + 4;
  }
  int find_label = text_pixels(cv_find_text) + 8;
  bool stack = w < used + find_label + 220;
  int find_y = stack ? y + kFileBar : y + 3;
  int find_x = stack ? x + 8 : x + used + 8;
  int find_w = stack ? std::max(40, w - 16) : std::max(40, w - (used + 16));
  place_find(find_x, find_y, find_w);
  int editor_y = y + kFileBar + (stack ? kFindH + 2 : 0);
  int editor_h = std::max(40, h - (editor_y - y) - kStatus);
  Fl_Widget *views[4] = {cv_source, cv_header, cv_strings, cv_project};
  for (int i = 0; i < 4; i++) {
    if (views[i]) views[i]->resize(x, editor_y, std::max(1, w), editor_h);
  }
  place_status(x, y + h - kStatus, w);
}

void Code_Stage::resize(int x, int y, int w, int h) {
  Fl_Widget::resize(x, y, w, h);
  if (ready_) place();
  redraw();
}

}  // namespace

Fl_Group *make_code_stage(int x, int y, int w, int h) {
  g_stage = new Code_Stage(x, y, w, h);
  g_stage->hide();
  return g_stage;
}

void code_stage_refresh() {
  if (g_stage) g_stage->refresh();
}

void code_stage_set_active(bool on) {
  if (!g_stage) return;
  if (on) {
    g_stage->show();
    g_stage->open();
  } else {
    codeview_set_embedded(false);
    g_stage->hide();
  }
}

}  // namespace fluid
