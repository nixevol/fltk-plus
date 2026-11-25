//
// Code panel. Its tabs match the original C++ page.
// It is a dockable panel; the dock owns the caption and folding.

#include "app/code_console.h"

#include "Fluid.h"
#include "app/ui_locale.h"
#include "nodes/Node.h"
#include "nodes/Widget_Node.h"
#include "widgets/Code_Editor.h"

#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/fl_draw.H>

#include <cstdio>
#include <cstdlib>
#include <string>

namespace fluid {

namespace {

const int kTabH = 24;
const char *kTabKeys[6] = {
  "// comment", "#include", "declare", "setup", "final", "callback"
};

class Code_Console : public Fl_Group {
public:
  Fl_Tabs *tabs = nullptr;
  Fl_Group *pages[6] = {};
  fluid::widget::Code_Editor *editors[6] = {};
  Node *node = nullptr;
  bool loading = false;

  Code_Console(int x, int y, int w, int h);
  void resize(int x, int y, int w, int h) override;
  void load(Node *next);
  void flush();

private:
  void style_tabs();
  static void modify_cb(int, int, int, int, const char *, void *);
};

Code_Console *g_console = nullptr;

std::string buffer_text(Fl_Text_Buffer *buffer) {
  if (!buffer) return std::string();
  char *text = buffer->text();
  std::string value = text ? text : "";
  std::free(text);
  return value;
}

void flush_later(void *) {
  if (g_console) g_console->flush();
}

void Code_Console::modify_cb(int, int, int, int, const char *, void *) {
  if (!g_console || g_console->loading) return;
  Fl::remove_timeout(flush_later);
  Fl::add_timeout(0.4, flush_later);
}

Code_Console::Code_Console(int x, int y, int w, int h) : Fl_Group(x, y, w, h) {
  box(FL_FLAT_BOX);
  color(FL_BACKGROUND_COLOR);
  tabs = new Fl_Tabs(x, y, w, std::max(1, h));
  for (int i = 0; i < 6; i++) {
    pages[i] = new Fl_Group(x, y + kTabH, w, std::max(1, h - kTabH), kTabKeys[i]);
    pages[i]->copy_label(ui_text(kTabKeys[i]));
    pages[i]->labelsize(12);
    editors[i] = new fluid::widget::Code_Editor(x + 2, y + kTabH + 2, std::max(20, w - 4), std::max(20, h - kTabH - 4));
    editors[i]->textsize(12);
    editors[i]->buffer()->add_modify_callback(modify_cb, this);
    pages[i]->resizable(editors[i]);
    pages[i]->end();
  }
  tabs->end();
  end();
  resizable(tabs);
}

void Code_Console::resize(int x, int y, int w, int h) {
  // Do not use Fl_Group::resize. Place the tabs and editors at the new size directly.
  Fl_Widget::resize(x, y, w, h);
  if (!tabs) return;
  tabs->resize(x, y, w, std::max(1, h));
  int page_y = y + kTabH;
  int page_h = std::max(1, h - kTabH);
  for (int i = 0; i < 6; i++) {
    if (!pages[i]) continue;
    pages[i]->resize(x, page_y, w, page_h);
    if (editors[i])
      editors[i]->resize(x + 2, page_y + 2, std::max(20, w - 4), std::max(20, page_h - 4));
  }
  redraw();
}

void Code_Console::style_tabs() {
  for (int i = 0; i < 6; i++) {
    std::string text = buffer_text(editors[i]->buffer());
    pages[i]->labelfont(text.empty() ? FL_HELVETICA : FL_HELVETICA_BOLD);
  }
  if (tabs) tabs->redraw();
}

bool node_alive(Node *node) {
  if (!node) return false;
  for (Node *item = Fluid.proj.tree.first; item; item = item->next)
    if (item == node) return true;
  return false;
}

void Code_Console::flush() {
  if (loading || !node_alive(node)) {
    if (!node_alive(node)) node = nullptr;
    return;
  }
  std::string comment = buffer_text(editors[0]->buffer());
  std::string include = buffer_text(editors[1]->buffer());
  std::string declare = buffer_text(editors[2]->buffer());
  std::string setup = buffer_text(editors[3]->buffer());
  std::string final_code = buffer_text(editors[4]->buffer());
  std::string callback = buffer_text(editors[5]->buffer());
  auto *widget = dynamic_cast<Widget_Node *>(node);
  bool changed = comment != node->comment() || callback != node->callback();
  if (widget) {
    changed = changed
      || include != widget->extra_code(0)
      || declare != widget->extra_code(1)
      || setup != widget->extra_code(2)
      || final_code != widget->extra_code(3);
  }
  if (!changed) {
    style_tabs();
    return;
  }
  Fluid.proj.undo.checkpoint();
  node->comment(comment);
  node->callback(callback);
  if (widget) {
    widget->extra_code(0, include);
    widget->extra_code(1, declare);
    widget->extra_code(2, setup);
    widget->extra_code(3, final_code);
  }
  Fluid.proj.set_modflag(1);
  style_tabs();
}

void Code_Console::load(Node *next) {
  flush();
  loading = true;
  node = next;
  auto *widget = dynamic_cast<Widget_Node *>(next);
  const char *comment = next ? next->comment().c_str() : "";
  const char *callback = next ? next->callback().c_str() : "";
  editors[0]->buffer()->text(comment);
  for (int i = 0; i < 4; i++) {
    const char *code = widget ? widget->extra_code(i).c_str() : "";
    editors[i + 1]->buffer()->text(code);
    if (widget) editors[i + 1]->activate();
    else editors[i + 1]->deactivate();
  }
  editors[5]->buffer()->text(callback);
  if (next) {
    editors[0]->activate();
    editors[5]->activate();
  } else {
    editors[0]->deactivate();
    editors[5]->deactivate();
  }
  loading = false;
  style_tabs();
}

}  // namespace

Fl_Group *make_code_console(int x, int y, int w, int h) {
  g_console = new Code_Console(x, y, w, h);
  return g_console;
}

void code_console_load(Node *node) {
  if (g_console) g_console->load(node);
}

void code_console_reset() {
  if (!g_console) return;
  g_console->loading = true;
  g_console->node = nullptr;
  g_console->loading = false;
}

void code_console_apply_language() {
  if (!g_console) return;
  for (int i = 0; i < 6; i++) {
    if (g_console->pages[i]) g_console->pages[i]->copy_label(ui_text(kTabKeys[i]));
  }
}

}  // namespace fluid
