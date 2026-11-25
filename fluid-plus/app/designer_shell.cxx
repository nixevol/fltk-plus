//
// Fluid Plus designer shell: properties on the left, the design desktop in the center, and the widget bin on the right.

#include "app/code_console.h"
#include "app/code_stage.h"
#include "app/designer_shell.h"
#include "panels/codeview_panel.h"
#include "app/dock.h"
#include "app/export_actions.h"
#include "app/frame_skin.h"
#include "app/owned_choice.h"
#include "app/property_sheet.h"
#include "app/toolbox.h"

#include "Fluid.h"
#include "app/ui_locale.h"
#include "message.h"
#include "panels/function_panel.h"
#include "panels/widget_panel.h"
#include "widgets/Node_Browser.h"
#include "nodes/Window_Node.h"
#include "nodes/Node.h"

#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Pack.H>
#include <FL/Fl_Preferences.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>
#include "../../src/Fl_Window_Driver.H"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <utility>
#include <vector>

extern void load_panel();

namespace fluid {

namespace {

const int kBar = 16;
const int kMenu = 25;       // Height of the menu bar
const int kTabButton = 26;  // Height of a window-tab button along the bottom
const int kTabGap = 4;      // Gap between the tabs and the scrollbar
const int kChooser = 32;    // Height of the window chooser above the project tree

int tab_strip_height(int view_w);
void layout_tabs();
void show_tab_menu(Window_Node *window);
const int kMargin = 12;

// Panel ids. Saved layouts refer to them.
const char *const kTree = "tree";
const char *const kProps = "props";
const char *const kToolbox = "toolbox";
const char *const kCode = "code";
const int kGrip = 6;      // Width of the band outside the frame that can be dragged to resize
const int kTopGrip = 4;   // Height at the top of the caption that can be dragged to change the height
const int kCorner = 14;   // Distance from a corner that counts as a corner drag
const int kMinSize = 10;  // Same minimum size as the original design window
const Fl_Color kDesktop = fl_rgb_color(188, 188, 188);

typedef std::pair<int, int> Point;

// Window coordinates stored in the project. After embedding, the window's own coordinates are its position inside the design area.
std::map<const Fl_Widget *, Point> g_stored_xy;
// Top-left of the frame on the design-area canvas. Placement only; not written into the project.
std::map<const Fl_Widget *, Point> g_place;
// Order of the bottom tabs, by node uid. Stored in preferences with the project path so the next open can restore it.
std::vector<unsigned> g_tab_uids;
std::string g_tab_file;
// Canvas position for the frame the next time a new window is shown. Cleared after use.
bool g_expect_frame = false;
int g_expect_x = 0;
int g_expect_y = 0;
// Undo and redo delete every node and rebuild them. Record placement, the current tab, scroll, and keyboard focus in window order, then restore them.
std::vector<Point> g_place_backup;
Point g_scroll_backup(0, 0);
int g_shown_backup = -1;
bool g_focus_backup = false;
bool g_scope_backup = false;
bool g_have_backup = false;

bool g_pinning = false;
bool g_laying = false;
// While undo recording is suspended (loading a project, creating a widget, grouping), leave the interface alone and only remember that a refresh is needed.
bool g_pending = false;
void expose_shell();

// Clipped area with scrollbars. The viewport is a real child window; the system clips whatever extends past it.
class ClipPane : public Fl_Group {
public:
  Fl_Window *viewport = nullptr;
  Fl_Scrollbar *hbar = nullptr;
  Fl_Scrollbar *vbar = nullptr;

  ClipPane(int x, int y, int w, int h) : Fl_Group(x, y, w, h) {
    box(FL_NO_BOX);
    clip_children(1);
  }
  int scroll_x() const;
  int scroll_y() const;
  void scroll_to(int sx, int sy);
  void layout();
  void resize(int x, int y, int w, int h) override;

protected:
  // Call once after the subclass has built the viewport.
  void finish(Fl_Window *view);
  // Size of the content. Returns false when there is none.
  virtual bool extent(int &cw, int &ch) = 0;
  // Place the content using the current scroll offset.
  virtual void place() = 0;

private:
  static void scroll_cb(Fl_Widget *, void *pane);
};

void ClipPane::finish(Fl_Window *view) {
  viewport = view;
  view->box(FL_FLAT_BOX);
  view->border(0);
  view->end();
  hbar = new Fl_Scrollbar(x(), y() + std::max(0, h() - kBar), std::max(1, w()), kBar);
  hbar->type(FL_HORIZONTAL);
  hbar->callback(scroll_cb, this);
  hbar->linesize(24);
  hbar->hide();
  vbar = new Fl_Scrollbar(x() + std::max(0, w() - kBar), y(), kBar, std::max(1, h()));
  vbar->callback(scroll_cb, this);
  vbar->linesize(24);
  vbar->hide();
  end();
}

void ClipPane::scroll_cb(Fl_Widget *, void *pane) {
  static_cast<ClipPane *>(pane)->layout();
}

int ClipPane::scroll_x() const {
  if (!hbar || !hbar->visible()) return 0;
  return (int)hbar->value();
}

int ClipPane::scroll_y() const {
  if (!vbar || !vbar->visible()) return 0;
  return (int)vbar->value();
}

void ClipPane::scroll_to(int sx, int sy) {
  // layout clamps values that fall outside the range.
  if (hbar) hbar->value(std::max(0, sx));
  if (vbar) vbar->value(std::max(0, sy));
  layout();
}

void ClipPane::layout() {
  if (!viewport || g_laying) return;
  g_laying = true;
  int cw = 0;
  int ch = 0;
  if (!extent(cw, ch)) cw = ch = 0;
  bool need_v = ch > h();
  bool need_h = cw > w();
  if (need_v && cw > w() - kBar) need_h = true;
  if (need_h && ch > h() - kBar) need_v = true;
  int vw = need_v ? kBar : 0;
  int hw = need_h ? kBar : 0;
  int view_w = std::max(1, w() - vw);
  int view_h = std::max(1, h() - hw);
  viewport->resize(x(), y(), view_w, view_h);
  int max_x = std::max(0, cw - view_w);
  int max_y = std::max(0, ch - view_h);
  int sx = std::min(max_x, hbar ? (int)hbar->value() : 0);
  int sy = std::min(max_y, vbar ? (int)vbar->value() : 0);
  if (need_h) {
    hbar->resize(x(), y() + view_h, view_w, kBar);
    hbar->bounds(0, std::max(1, max_x));
    hbar->value(sx);
    hbar->slider_size(cw > 0 ? std::min(1.f, (float)view_w / (float)cw) : 1.f);
    hbar->show();
  } else if (hbar) {
    hbar->value(0);
    hbar->hide();
  }
  if (need_v) {
    vbar->resize(x() + view_w, y(), kBar, view_h);
    vbar->bounds(0, std::max(1, max_y));
    vbar->value(sy);
    vbar->slider_size(ch > 0 ? std::min(1.f, (float)view_h / (float)ch) : 1.f);
    vbar->show();
  } else if (vbar) {
    vbar->value(0);
    vbar->hide();
  }
  place();
  init_sizes();
  redraw();
  viewport->redraw();
  g_laying = false;
  expose_shell();
}

void ClipPane::resize(int x, int y, int w, int h) {
  Fl_Group::resize(x, y, w, h);
  layout();
}

class DeskPane;

// Viewport of the design desktop. The design window is its child, and the frame is drawn on it.
class DeskView : public Fl_Double_Window {
public:
  DeskPane *pane = nullptr;
  DeskView(int x, int y, int w, int h) : Fl_Double_Window(x, y, w, h) {}
  void draw() override;
  int handle(int event) override;
};

// Design desktop in the center.
//
// The design window is a borderless child of the viewport, so the system clips it to the design area and it moves with the main window.
// The frame image comes from the system (see frame_skin.h). The buttons are only shown to the designer; clicking them does nothing.
// Drag the caption to move the window inside the design area. Drag an edge or a corner to resize the design window.
class DeskPane : public ClipPane {
public:
  DeskPane(int x, int y, int w, int h);

  // Embed the design window and show it. A null pointer clears the desktop.
  void set_form(Fl_Window *window);
  // The design window currently embedded. Returns null once it has been deleted.
  Fl_Window *form();
  // When the title, size, border flag, or system appearance changes, fetch the frame again and lay it out.
  void sync();
  // Call this when the frame image is replaced later.
  void refresh();
  // Place a window's frame at a point on the canvas.
  void move_frame(Fl_Window *window, int cx, int cy);
  // Client-area position of the design window inside the viewport. Returns false when widget is not the embedded window.
  bool client_origin(const Fl_Widget *widget, int &x, int &y);
  // Give the keyboard to the design window: arrow keys move widgets, and undo and delete follow menu shortcuts.
  // The design window is a child of the main window. Without this, keystrokes fall into some other input field.
  void focus_form();
  bool form_has_focus();
  void draw_frame();
  int frame_event(int event);

protected:
  bool extent(int &cw, int &ch) override;
  void place() override;

private:
  enum {
    kNone = 0, kLeft = 1, kRight = 2, kTop = 4, kBottom = 8, kEdges = 15,
    kTitle = 16,   // Caption; drag it to move
    kInert = 32    // Minimize, maximize, and close; appearance only
  };

  // The design-window state that decides what the frame looks like.
  struct Look {
    bool visible = false;
    int w = 0;
    int h = 0;
    int border = 0;
    float scale = 0;
    std::string title;
  };

  void insets(int &l, int &t, int &r, int &b);
  void outer(int &x, int &y, int &w, int &h);
  int zone_at(int mx, int my);
  void point(int zone);
  void drag_to(int root_x, int root_y);
  void request_skin();

  Fl_Window *form_ = nullptr;
  Fl_Widget_Tracker *alive_ = nullptr;
  int fx_ = kMargin;        // Top-left of the frame on the canvas
  int fy_ = kMargin;
  Look seen_;
  float pinned_scale_ = 0; // Screen scale last used when the design window's pixel size was set
  bool syncing_ = false;
  Fl_Cursor cursor_ = FL_CURSOR_DEFAULT;
  int drag_ = kNone;
  int start_root_x_ = 0;
  int start_root_y_ = 0;
  int start_fx_ = 0;
  int start_fy_ = 0;
  int start_w_ = 0;
  int start_h_ = 0;
};

DeskPane::DeskPane(int x, int y, int w, int h) : ClipPane(x, y, w, h) {
  auto *view = new DeskView(x, y, std::max(1, w), std::max(1, h));
  view->pane = this;
  view->color(kDesktop);
  finish(view);
}

Fl_Window *DeskPane::form() {
  // Deleting the node deletes the window object too, so it must not be touched again.
  if (form_ && alive_ && alive_->deleted()) {
    form_ = nullptr;
    delete alive_;
    alive_ = nullptr;
    drag_ = kNone;
  }
  return form_;
}

void DeskPane::set_form(Fl_Window *window) {
  Fl_Window *old = form();
  if (old && old != window) {
    if (old->shown()) old->hide();
    if (old->parent() == viewport) viewport->remove(old);
  }
  drag_ = kNone;
  point(kNone);
  delete alive_;
  alive_ = nullptr;
  form_ = window;
  seen_ = Look();
  if (!window) {
    layout();
    return;
  }
  alive_ = new Fl_Widget_Tracker(window);
  viewport->show();
  if (window->parent() != viewport) {
    if (window->shown()) window->hide();
    if (window->parent()) window->parent()->remove(window);
    viewport->add(window);
  }
  auto found = g_place.find(window);
  if (found == g_place.end()) found = g_place.emplace(window, Point(kMargin, kMargin)).first;
  fx_ = found->second.first;
  fy_ = found->second.second;
  if (g_expect_frame) {
    fx_ = std::max(0, g_expect_x);
    fy_ = std::max(0, g_expect_y);
    g_place[window] = Point(fx_, fy_);
    g_expect_frame = false;
  }
  // The border flag is a project property. macOS clears it when showing a child window; restore it after show.
  int border = window->border();
  g_pinning = true;
  if (!window->shown()) window->show();
  if ((window->border() != 0) != (border != 0)) window->border(border);
  g_pinning = false;
  sync();
}

void DeskPane::insets(int &l, int &t, int &r, int &b) {
  l = t = r = b = 0;
  Fl_Window *window = form();
  if (window && window->border()) frame_skin_insets(l, t, r, b);
}

void DeskPane::outer(int &x, int &y, int &w, int &h) {
  int l, t, r, b;
  insets(l, t, r, b);
  Fl_Window *window = form();
  x = fx_ - scroll_x();
  y = fy_ - scroll_y();
  w = (window ? window->w() : 0) + l + r;
  h = (window ? window->h() : 0) + t + b;
}

bool DeskPane::client_origin(const Fl_Widget *widget, int &x, int &y) {
  if (!widget || widget != form()) return false;
  int l, t, r, b;
  insets(l, t, r, b);
  x = fx_ - scroll_x() + l;
  y = fy_ - scroll_y() + t;
  return true;
}

bool DeskPane::extent(int &cw, int &ch) {
  Fl_Window *window = form();
  if (!window || !window->visible()) return false;
  int l, t, r, b;
  insets(l, t, r, b);
  cw = fx_ + window->w() + l + r + kMargin;
  ch = fy_ + window->h() + t + b + kMargin;
  return true;
}

// A child window does not receive WM_DPICHANGED when it moves to another screen. When the logical size is unchanged, an ordinary resize does not change the pixel size,
// but drawing already uses the new scale, so widgets are drawn past the window and clipped. Force the pixel size to the current scale.
void pin_window(Fl_Window *window, int x, int y, int w, int h, bool rescale) {
  if (!window) return;
  bool saved = g_pinning;
  g_pinning = true;
  Fl_Window_Driver *driver = Fl_Window_Driver::driver(window);
  if (rescale && driver) driver->is_a_rescale(true);
  window->resize(x, y, w, h);
  if (rescale && driver) driver->is_a_rescale(false);
  g_pinning = saved;
}

void DeskPane::place() {
  Fl_Window *window = form();
  if (!window || !window->visible()) {
    viewport->redraw();
    return;
  }
  float scale = 1.f;
  if (Fluid.main_window) scale = Fl::screen_scale(Fluid.main_window->screen_num());
  if (scale < 0.1f) scale = 1.f;
  bool rescale = scale < pinned_scale_ - 0.01f || scale > pinned_scale_ + 0.01f;
  if (rescale && viewport && viewport->shown())
    pin_window(viewport, viewport->x(), viewport->y(), viewport->w(), viewport->h(), true);
  int px = 0;
  int py = 0;
  client_origin(window, px, py);
  if (rescale || window->x() != px || window->y() != py)
    pin_window(window, px, py, window->w(), window->h(), rescale);
  if (rescale) pinned_scale_ = scale;
  viewport->redraw();
}

void DeskPane::request_skin() {
  Fl_Window *window = form();
  if (!window || !Fluid.main_window) return;
  Frame_Request request;
  request.title = window->label() ? window->label() : "";
  request.w = window->w();
  request.h = window->h();
  int cx = 0;
  int cy = 0;
  client_origin(window, cx, cy);
  request.screen_x = viewport->x_root() + cx;
  request.screen_y = viewport->y_root() + cy;
  request.owner = Fluid.main_window;
  frame_skin_request(request);
}

void DeskPane::sync() {
  if (syncing_) return;
  if (!Fluid.main_window || !Fluid.main_window->visible()) return;
  Fl_Window *window = form();
  Look now;
  const char *title = "";
  now.visible = window && window->visible();
  if (now.visible) {
    now.w = window->w();
    now.h = window->h();
    now.border = window->border() ? 1 : 0;
    now.scale = Fl::screen_scale(Fluid.main_window->screen_num());
    if (window->label()) title = window->label();
  }
  // Reached after every round of event handling. Do not allocate when nothing has changed.
  bool same = now.visible == seen_.visible && now.w == seen_.w && now.h == seen_.h &&
              now.border == seen_.border && now.scale == seen_.scale && seen_.title == title;
  bool stale = frame_skin_take_stale();
  if (!stale && same) return;
  now.title = title;
  seen_ = now;
  syncing_ = true;
  // On systems that must show a temporary window to capture the frame, stretch the old frame while resizing and capture a new one when the mouse is released.
  bool resizing = (drag_ & kEdges) != 0;
  if (now.visible && now.border && (!resizing || frame_skin_live())) request_skin();
  refresh();
  syncing_ = false;
}

void DeskPane::refresh() {
  // Do not recompute the scrollbars during a drag, so the canvas does not jump under the pointer.
  if (drag_) place();
  else layout();
}

void DeskPane::move_frame(Fl_Window *window, int cx, int cy) {
  if (!window) return;
  Point at(std::max(0, cx), std::max(0, cy));
  g_place[window] = at;
  if (window != form()) return;
  fx_ = at.first;
  fy_ = at.second;
  layout();
}

void DeskPane::focus_form() {
  Fl_Window *window = form();
  if (window && window->visible() && Fl::focus() != window) Fl::focus(window);
}

bool DeskPane::form_has_focus() {
  Fl_Window *window = form();
  return window && Fl::focus() == window;
}

void DeskPane::draw_frame() {
  Fl_Window *window = form();
  if (!window || !window->visible() || !window->border()) return;
  int x, y, w, h;
  outer(x, y, w, h);
  frame_skin_draw(x, y, w, h, window->label());
}

int DeskPane::zone_at(int mx, int my) {
  Fl_Window *window = form();
  if (!window || !window->visible()) return kNone;
  int l, t, r, b;
  insets(l, t, r, b);
  int x, y, w, h;
  outer(x, y, w, h);
  if (mx < x - kGrip || my < y - kGrip || mx >= x + w + kGrip || my >= y + h + kGrip) return kNone;
  // The client area belongs to the design window.
  if (mx >= x + l && mx < x + w - r && my >= y + t && my < y + h - b) return kNone;
  int zone = kNone;
  if (mx < x + l) zone |= kLeft;
  else if (mx >= x + w - r) zone |= kRight;
  if (my < y + std::min(t, kTopGrip)) zone |= kTop;
  else if (my >= y + h - b) zone |= kBottom;
  if (zone) {
    // An edge hit close to a corner counts as a corner drag.
    if ((zone & (kTop | kBottom)) && !(zone & (kLeft | kRight))) {
      if (mx < x + kCorner) zone |= kLeft;
      else if (mx >= x + w - kCorner) zone |= kRight;
    } else if ((zone & (kLeft | kRight)) && !(zone & (kTop | kBottom))) {
      if (my < y + kCorner) zone |= kTop;
      else if (my >= y + h - kCorner) zone |= kBottom;
    }
    return zone;
  }
  if (t > 0 && my < y + t) return frame_skin_inert(mx - x, w) ? kInert : kTitle;
  return kNone;
}

void DeskPane::point(int zone) {
  Fl_Cursor cursor = FL_CURSOR_DEFAULT;
  int edges = zone & kEdges;
  if (edges == kLeft || edges == kRight) cursor = FL_CURSOR_WE;
  else if (edges == kTop || edges == kBottom) cursor = FL_CURSOR_NS;
  else if (edges == (kLeft | kTop) || edges == (kRight | kBottom)) cursor = FL_CURSOR_NWSE;
  else if (edges == (kRight | kTop) || edges == (kLeft | kBottom)) cursor = FL_CURSOR_NESW;
  if (cursor == cursor_) return;
  cursor_ = cursor;
  if (viewport && viewport->shown()) viewport->cursor(cursor);
}

void DeskPane::drag_to(int root_x, int root_y) {
  Fl_Window *window = form();
  if (!window) {
    drag_ = kNone;
    return;
  }
  int dx = root_x - start_root_x_;
  int dy = root_y - start_root_y_;
  if (drag_ == kTitle) {
    fx_ = std::max(0, start_fx_ + dx);
    fy_ = std::max(0, start_fy_ + dy);
    g_place[window] = Point(fx_, fy_);
    place();
    return;
  }
  if (!(drag_ & kEdges)) return;
  int nx = start_fx_;
  int ny = start_fy_;
  int nw = start_w_;
  int nh = start_h_;
  if (drag_ & kRight) nw = start_w_ + dx;
  if (drag_ & kBottom) nh = start_h_ + dy;
  if (drag_ & kLeft) {
    // Dragging the left edge keeps the right edge fixed.
    int move = std::max(-start_fx_, std::min(dx, start_w_ - kMinSize));
    nx = start_fx_ + move;
    nw = start_w_ - move;
  }
  if (drag_ & kTop) {
    int move = std::max(-start_fy_, std::min(dy, start_h_ - kMinSize));
    ny = start_fy_ + move;
    nh = start_h_ - move;
  }
  nw = std::max(kMinSize, nw);
  nh = std::max(kMinSize, nh);
  if (nx == fx_ && ny == fy_ && nw == window->w() && nh == window->h()) return;
  fx_ = nx;
  fy_ = ny;
  g_place[window] = Point(fx_, fy_);
  int px = 0;
  int py = 0;
  client_origin(window, px, py);
  // Use the design window's own resize so it updates the undo point, the modified flag, and the property page.
  window->resize(px, py, nw, nh);
  viewport->redraw();
}

int DeskPane::frame_event(int event) {
  switch (event) {
    case FL_ENTER:
    case FL_MOVE: {
      if (drag_) return 1;
      int zone = zone_at(Fl::event_x(), Fl::event_y());
      point(zone);
      if (zone == kNone) return 0;
      Fl::belowmouse(viewport);
      return 1;
    }
    case FL_LEAVE:
      if (!drag_) point(kNone);
      return 0;
    case FL_PUSH: {
      // A click anywhere on the design desktop gives the keyboard to the design window.
      focus_form();
      if (Fl::event_button() != FL_LEFT_MOUSE) return 0;
      Fl_Window *window = form();
      int zone = zone_at(Fl::event_x(), Fl::event_y());
      if (!window || zone == kNone) return 0;
      drag_ = zone;
      start_root_x_ = Fl::event_x_root();
      start_root_y_ = Fl::event_y_root();
      start_fx_ = fx_;
      start_fy_ = fy_;
      start_w_ = window->w();
      start_h_ = window->h();
      return 1;
    }
    case FL_DRAG:
      if (!drag_) return 0;
      drag_to(Fl::event_x_root(), Fl::event_y_root());
      return 1;
    case FL_RELEASE: {
      if (!drag_) return 0;
      bool resized = (drag_ & kEdges) != 0;
      drag_ = kNone;
      if (resized && !frame_skin_live() && form() && form()->border()) request_skin();
      layout();
      point(zone_at(Fl::event_x(), Fl::event_y()));
      return 1;
    }
  }
  return 0;
}

void DeskView::draw() {
  Fl_Double_Window::draw();
  if (pane) pane->draw_frame();
  toolbox_draw_desk_band();
}

int DeskView::handle(int event) {
  if (pane && pane->frame_event(event)) return 1;
  if (toolbox_desk_event(event)) return 1;
  return Fl_Double_Window::handle(event);
}

// Project-tree page: the window chooser on top, the project tree below.
class Tree_Page : public Fl_Group {
public:
  Owned_Choice *chooser = nullptr;
  Fl_Widget *tree = nullptr;

  Tree_Page(int x, int y, int w, int h) : Fl_Group(x, y, w, h) {
    box(FL_FLAT_BOX);
    color(FL_BACKGROUND_COLOR);
    chooser = new Owned_Choice(x + 4, y + 4, std::max(20, w - 8), 24);
    chooser->textsize(12);
    end();
  }

  void place() {
    if (chooser) chooser->resize(x() + 4, y() + 4, std::max(20, w() - 8), 24);
    if (tree) tree->resize(x(), y() + kChooser, std::max(1, w()), std::max(1, h() - kChooser));
  }

  void resize(int x, int y, int w, int h) override {
    Fl_Widget::resize(x, y, w, h);
    place();
    redraw();
  }
};

// Tab strip along the bottom. While a tab is dragged, it is not drawn in place; Tab_Pack draws it under the pointer.
class Tab_Pack : public Fl_Pack {
public:
  Tab_Pack(int x, int y, int w, int h) : Fl_Pack(x, y, w, h) {}
  void lift(Fl_Widget *tab, int x);
  void drop();
  void draw() override;

private:
  Fl_Widget *lifted_ = nullptr;
  int lift_x_ = 0;
};

bool center_shows_code();

// Design/code switch at the top of the design area. Both modes share this bar, and it is not hidden with the content below.
class Mode_Bar : public Fl_Group {
public:
  Fl_Button *design = nullptr;
  Fl_Button *code = nullptr;
  Mode_Bar(int x, int y, int w, int h) : Fl_Group(x, y, w, h) {
    box(FL_FLAT_BOX);
    color(FL_BACKGROUND_COLOR);
    design = new Fl_Button(x + 8, y + 4, 72, 22);
    code = new Fl_Button(x + 84, y + 4, 72, 22);
    design->copy_label(ui_text("Design"));
    code->copy_label(ui_text("Code"));
    for (Fl_Button *button : {design, code}) {
      button->box(FL_FLAT_BOX);
      button->down_box(FL_FLAT_BOX);
      button->labelsize(12);
      button->callback(mode_cb, this);
    }
    end();
    show_mode(false);
  }
  void show_mode(bool code_on) {
    design->color(code_on ? FL_BACKGROUND_COLOR : FL_SELECTION_COLOR);
    design->labelcolor(code_on ? FL_FOREGROUND_COLOR : FL_WHITE);
    code->color(code_on ? FL_SELECTION_COLOR : FL_BACKGROUND_COLOR);
    code->labelcolor(code_on ? FL_WHITE : FL_FOREGROUND_COLOR);
    redraw();
  }
  static void mode_cb(Fl_Widget *widget, void *bar) {
    auto *self = static_cast<Mode_Bar *>(bar);
    designer_set_code_mode(widget == self->code);
  }
};

// The center piece that cannot be dragged away: the design/code switch on top, and the desktop with window tabs while designing.
class Tab_Strip : public Fl_Scroll {
public:
  Tab_Strip(int x, int y, int w, int h) : Fl_Scroll(x, y, w, h) {}
  int handle(int event) override {
    // Hand the event to the tab buttons first. Only a click on empty space pops up the menu that contains New Window alone.
    if (event == FL_PUSH && Fl::event_button() == FL_RIGHT_MOUSE) {
      if (Fl_Scroll::handle(event)) return 1;
      show_tab_menu(nullptr);
      return 1;
    }
    return Fl_Scroll::handle(event);
  }
};

class CenterPane : public Fl_Group {
public:
  DeskPane *desktop = nullptr;
  Fl_Scroll *tabs = nullptr;
  Mode_Bar *modes = nullptr;
  Fl_Group *code = nullptr;
  CenterPane(int x, int y, int w, int h) : Fl_Group(x, y, w, h) {}
  void resize(int x, int y, int w, int h) override {
    Fl_Widget::resize(x, y, w, h);
    const int bar_h = 30;
    if (modes) modes->resize(x, y, w, bar_h);
    int y2 = y + bar_h;
    int h2 = std::max(1, h - bar_h);
    if (center_shows_code()) {
      if (desktop) desktop->hide();
      if (tabs) tabs->hide();
      if (code) {
        code->show();
        code->resize(x, y2, w, h2);
      }
    } else {
      if (code) code->hide();
      if (desktop) desktop->show();
      if (tabs) tabs->show();
      int strip = tab_strip_height(w);
      int desk_h = std::max(1, h2 - strip);
      if (desktop) desktop->resize(x, y2, w, desk_h);
      if (tabs) tabs->resize(x, y2 + desk_h, w, strip);
      layout_tabs();
    }
    redraw();
  }
};

class ShellGroup : public Fl_Group {
public:
  Fl_Widget *menu = nullptr;
  Dock_Host *host = nullptr;
  ShellGroup(int x, int y, int w, int h) : Fl_Group(x, y, w, h) {
    box(FL_FLAT_BOX);
    color(FL_BACKGROUND_COLOR);
  }
  void resize(int x, int y, int w, int h) override {
    Fl_Widget::resize(x, y, w, h);
    if (menu) menu->resize(x, y, w, kMenu);
    if (host) host->resize(x, y + kMenu, w, std::max(1, h - kMenu));
    redraw();
  }
};

struct Shell {
  ShellGroup *shell = nullptr;
  Dock_Host *host = nullptr;
  CenterPane *center = nullptr;
  DeskPane *desktop = nullptr;
  Tree_Page *tree_page = nullptr;
  Fl_Scroll *tabs = nullptr;
  Fl_Pack *tab_pack = nullptr;
  Mode_Bar *modes = nullptr;
  bool code_mode = false;
  Window_Node *shown = nullptr;   // Window shown in the design area
  bool scope_all = false;         // The project tree shows the whole project, not only the tree that contains the current window
  int picked = -1;                // Item just chosen in the chooser
  bool busy = false;
};

Shell *g_shell = nullptr;

bool center_shows_code() {
  return g_shell && g_shell->code_mode;
}

// Panels hidden temporarily for code mode. On exit or when saving the layout, write the design-mode state back so this hide is not stored as the user's layout.
bool g_suspend_save = false;
bool g_keep_props = true;
bool g_keep_bin = true;
bool g_keep_snippets = true;
bool g_fold_props = false;
bool g_fold_bin = false;
bool g_fold_snippets = false;

void expose_shell() {
  if (Fluid.main_window) Fluid.main_window->damage(FL_DAMAGE_ALL);
}

// Look at the design window after every round of event handling. The title and the border flag are changed elsewhere; replace the frame when they change.
void desk_check(void *) {
  if (!g_shell || !g_shell->desktop || g_shell->busy || g_pinning || g_laying) return;
  g_shell->desktop->sync();
}

void skin_changed() {
  if (!g_shell || !g_shell->desktop || g_pinning || g_laying) return;
  // The frame may have been captured only after the window settled onto a new screen. Lay it out once more here.
  g_shell->desktop->sync();
}

int label_width(const std::string &text) {
  int chars = 0;
  for (const unsigned char *p = (const unsigned char *)text.c_str(); *p; ++p) {
    if ((*p & 0xC0) != 0x80) chars++;
  }
  return std::max(64, chars * 13 + 16);
}

std::string tab_caption(Window_Node *window) {
  if (!window->name().empty()) return window->name();
  if (!window->label().empty()) return window->label();
  return ui_text("Window");
}

std::string tab_order_key(const std::string &path) {
  unsigned hash = 2166136261u;
  for (unsigned char c : path) hash = (hash ^ c) * 16777619u;
  char key[16];
  std::snprintf(key, sizeof(key), "w%08x", hash);
  return key;
}

void save_tab_order();

void load_tab_order() {
  std::string file = Fluid.proj.proj_filename;
  if (file == g_tab_file) return;
  // An unnamed project was just saved as a file: keep the order that was just arranged.
  if (g_tab_file.empty() && !file.empty() && !g_tab_uids.empty()) {
    g_tab_file = file;
    save_tab_order();
    return;
  }
  g_tab_file = file;
  g_tab_uids.clear();
  if (file.empty()) return;
  Fl_Preferences prefs(Fluid.preferences, "window_order");
  char *text = nullptr;
  prefs.get(tab_order_key(file).c_str(), text, "");
  std::string value = text ? text : "";
  if (text) free(text);
  std::string marker = file + "\t";
  if (value.compare(0, marker.size(), marker) != 0) return;
  const char *p = value.c_str() + marker.size();
  while (*p) {
    char *end = nullptr;
    unsigned long id = std::strtoul(p, &end, 10);
    if (end == p) break;
    g_tab_uids.push_back((unsigned)id);
    p = end;
  }
}

void save_tab_order() {
  if (g_tab_file.empty()) {
    g_tab_file = Fluid.proj.proj_filename;
  }
  if (g_tab_file.empty()) return;
  std::string value = g_tab_file;
  value.push_back('\t');
  for (size_t i = 0; i < g_tab_uids.size(); i++) {
    if (i) value.push_back(' ');
    value += std::to_string(g_tab_uids[i]);
  }
  Fl_Preferences prefs(Fluid.preferences, "window_order");
  prefs.set(tab_order_key(g_tab_file).c_str(), value.c_str());
  Fluid.preferences.flush();
}

void sort_windows(std::vector<Window_Node *> &windows) {
  load_tab_order();
  std::vector<size_t> rank(windows.size());
  for (size_t i = 0; i < windows.size(); i++) {
    unsigned uid = windows[i]->get_uid();
    auto found = std::find(g_tab_uids.begin(), g_tab_uids.end(), uid);
    rank[i] = found == g_tab_uids.end() ? g_tab_uids.size() + i : (size_t)(found - g_tab_uids.begin());
  }
  std::vector<size_t> order(windows.size());
  for (size_t i = 0; i < order.size(); i++) order[i] = i;
  std::stable_sort(order.begin(), order.end(), [&rank](size_t a, size_t b) { return rank[a] < rank[b]; });
  std::vector<Window_Node *> sorted;
  sorted.reserve(windows.size());
  std::vector<unsigned> uids;
  for (size_t index : order) {
    sorted.push_back(windows[index]);
    uids.push_back(windows[index]->get_uid());
  }
  windows.swap(sorted);
  if (uids != g_tab_uids) {
    g_tab_uids.swap(uids);
    save_tab_order();
  }
}

void collect_windows(std::vector<Window_Node *> &windows) {
  windows.clear();
  if (!Fluid.proj.tree.first) return;
  for (Node *node = Fluid.proj.tree.first; node; node = node->next) {
    if (auto *window = dynamic_cast<Window_Node *>(node))
      windows.push_back(window);
  }
  sort_windows(windows);
}

// Root of the tree that contains the node: the outermost class, function, or other top-level node.
Node *top_of(Node *node) {
  while (node && node->parent) node = node->parent;
  return node;
}

// The project tree shows only the tree that contains the current window.
void apply_scope() {
  if (!widget_browser) return;
  Node *root = (!g_shell->scope_all && g_shell->shown) ? top_of(g_shell->shown) : nullptr;
  widget_browser->scope(root);
}

// Chooser above the project tree: the first item is all nodes, then one entry per window.
void sync_chooser(const std::vector<Window_Node *> &windows) {
  Owned_Choice *chooser = g_shell->tree_page ? g_shell->tree_page->chooser : nullptr;
  if (!chooser) return;
  std::vector<std::pair<std::string, int>> items;
  items.push_back(std::make_pair(std::string(ui_text("All Nodes")), -1));
  int current = -1;
  for (size_t i = 0; i < windows.size(); i++) {
    items.push_back(std::make_pair(tab_caption(windows[i]), (int)i));
    if (!g_shell->scope_all && windows[i] == g_shell->shown) current = (int)i;
  }
  chooser->set_items(items, current);
  chooser->redraw();
}

// The user clicked a tab or picked a window in the chooser: switch the design area and the project tree to it, and select that window.
void pick_window(Window_Node *window) {
  g_shell->scope_all = false;
  designer_show(window, true);
  if (g_shell->desktop) g_shell->desktop->focus_form();
}

void tab_cb(Fl_Widget *widget, void *) {
  auto *window = static_cast<Window_Node *>(widget->user_data());
  if (!window) return;
  for (Node *node = Fluid.proj.tree.first; node; node = node->next) {
    if (node == window) {
      pick_window(window);
      return;
    }
  }
}

// Do not rebuild the chooser from inside its own callback. Wait until this event has been handled.
void chooser_later(void *) {
  if (!g_shell || g_shell->busy || Fluid.proj.undo.paused_) return;
  std::vector<Window_Node *> windows;
  collect_windows(windows);
  int pick = g_shell->picked;
  if (pick >= 0 && pick < (int)windows.size()) {
    pick_window(windows[pick]);
    return;
  }
  g_shell->scope_all = true;
  apply_scope();
  sync_chooser(windows);
  if (widget_browser && Fluid.proj.tree.current) widget_browser->display(Fluid.proj.tree.current);
}

void chooser_cb(Fl_Widget *widget, void *) {
  auto *chooser = static_cast<Owned_Choice *>(widget);
  const Fl_Menu_Item *item = chooser->mvalue();
  if (!item || !g_shell) return;
  g_shell->picked = (int)(fl_intptr_t)item->user_data();
  Fl::remove_timeout(chooser_later);
  Fl::add_timeout(0, chooser_later);
}

void remember_xy(Fl_Window *window) {
  if (!window || g_stored_xy.count(window)) return;
  g_stored_xy[window] = Point(window->x(), window->y());
}

void forget_closed(const std::vector<Window_Node *> &windows) {
  auto live = [&windows](const Fl_Widget *widget) {
    for (Window_Node *node : windows) {
      if (node->o == widget) return true;
    }
    return false;
  };
  for (auto it = g_stored_xy.begin(); it != g_stored_xy.end(); ) {
    if (live(it->first)) ++it;
    else it = g_stored_xy.erase(it);
  }
  for (auto it = g_place.begin(); it != g_place.end(); ) {
    if (live(it->first)) ++it;
    else it = g_place.erase(it);
  }
}

void show_windows(const std::vector<Window_Node *> &windows) {
  forget_closed(windows);
  Window_Node *shown = g_shell->shown;
  Fl_Window *embedded = g_shell->desktop ? g_shell->desktop->form() : nullptr;
  for (Window_Node *node : windows) {
    Fl_Window *window = static_cast<Fl_Window *>(node->o);
    if (!window) continue;
    remember_xy(window);
    // Leave the one currently embedded for the desktop to swap out.
    if (node != shown && window != embedded && window->shown()) window->hide();
  }
  if (!g_shell->desktop) return;
  g_shell->desktop->set_form(shown && shown->o ? static_cast<Fl_Window *>(shown->o) : nullptr);
}

void layout_tabs() {
  Fl_Pack *pack = g_shell ? g_shell->tab_pack : nullptr;
  Fl_Scroll *tabs = g_shell ? g_shell->tabs : nullptr;
  if (!pack || !tabs) return;
  int width = 0;
  for (int i = 0; i < pack->children(); i++) width += pack->child(i)->w() + pack->spacing();
  int strip = tab_strip_height(tabs->w());
  Fl_Widget *center = tabs->parent();
  if (center && g_shell->desktop && tabs->h() != strip) {
    int desk_h = std::max(1, center->h() - strip);
    g_shell->desktop->resize(center->x(), center->y(), center->w(), desk_h);
    tabs->resize(center->x(), center->y() + desk_h, center->w(), strip);
  }
  int sx = tabs->xposition();
  int max_scroll = std::max(0, width - tabs->w());
  if (sx > max_scroll) sx = max_scroll;
  pack->resize(tabs->x() - sx, tabs->y(), std::max(width, tabs->w()), kTabButton);
  tabs->init_sizes();
  tabs->redraw();
  pack->redraw();
}

int tab_strip_height(int view_w) {
  int bar = Fl::scrollbar_size();
  if (bar < 8) bar = 16;
  int content = 0;
  if (g_shell && g_shell->tab_pack) {
    Fl_Pack *pack = g_shell->tab_pack;
    for (int i = 0; i < pack->children(); i++) content += pack->child(i)->w() + pack->spacing();
  }
  if (view_w > 0 && content > view_w) return kTabButton + kTabGap + bar;
  return kTabButton + kTabGap;
}

void apply_caption(Fl_Widget *button, const std::string &caption) {
  if (!button->label() || caption != button->label()) button->copy_label(caption.c_str());
  int width = label_width(caption);
  if (button->w() != width) button->size(width, button->h());
}

bool delete_tab_window(Window_Node *window) {
  if (!window || !g_shell) return false;
  bool live = false;
  for (Node *node = Fluid.proj.tree.first; node; node = node->next) live = live || node == window;
  if (!live) return false;
  std::string caption = tab_caption(window);
  if (fluid_choice(ui_text("Delete window \"%s\" and everything inside it?"),
                   ui_text("Delete"), ui_text("Cancel"), nullptr, caption.c_str()) != 0)
    return false;
  if (g_shell->desktop && window->o && g_shell->desktop->form() == window->o)
    g_shell->desktop->set_form(nullptr);
  if (g_shell->shown == window) g_shell->shown = nullptr;
  select_only(window);
  Fluid.delete_selected();
  designer_refresh();
  return true;
}

Window_Node *g_menu_window = nullptr;
bool g_preview_from_tab = false;

void delete_menu_cb(Fl_Widget *, void *) {
  if (delete_tab_window(g_menu_window)) g_menu_window = nullptr;
}

void preview_menu_cb(Fl_Widget *, void *) {
  // The menu still has the pointer grabbed, so a preview opened now would not appear. Remember it and follow the F5 path after the menu closes.
  g_preview_from_tab = true;
}

void new_window_cb(Fl_Widget *, void *) {
  Node *before = Fluid.proj.tree.current;
  if (g_menu_window) {
    select_only(g_menu_window);
    Fluid.proj.tree.current = g_menu_window;
    before = g_menu_window;
  }
  type_make_cb(nullptr, (void *)"Fl_Window");
  // After a successful create, the current node becomes the new window. Do not jump back to the tab that was right-clicked.
  if (Fluid.proj.tree.current && Fluid.proj.tree.current != before) g_menu_window = nullptr;
}

void show_tab_menu(Window_Node *window) {
  g_menu_window = window;
  g_preview_from_tab = false;
  if (window) {
    Fl_Menu_Item menu[] = {
      {ui_text("Preview Window"), 0, preview_menu_cb, nullptr, FL_MENU_DIVIDER, 0, 0, 12, 0},
      {ui_text("New Window"), 0, new_window_cb, nullptr, 0, 0, 0, 12, 0},
      {ui_text("Delete Window"), 0, delete_menu_cb, nullptr, 0, 0, 0, 12, 0},
      {nullptr}
    };
    // popup returns the chosen item and does not call the callback. Call it while the array is still on the stack.
    const Fl_Menu_Item *choice = menu->popup(Fl::event_x(), Fl::event_y());
    if (choice && choice->callback()) choice->do_callback(nullptr);
  } else {
    Fl_Menu_Item menu[] = {
      {ui_text("New Window"), 0, new_window_cb, nullptr, 0, 0, 0, 12, 0},
      {nullptr}
    };
    const Fl_Menu_Item *choice = menu->popup(Fl::event_x(), Fl::event_y());
    if (choice && choice->callback()) choice->do_callback(nullptr);
  }
  bool preview = g_preview_from_tab;
  g_preview_from_tab = false;
  Window_Node *picked = g_menu_window;
  g_menu_window = nullptr;
  if (picked) {
    for (Node *node = Fluid.proj.tree.first; node; node = node->next) {
      if (node == picked) {
        pick_window(picked);
        break;
      }
    }
  }
  // After the menu has closed and the mouse has been released, open the same preview as F5.
  if (preview) fluid::preview_project_windows();
}

void Tab_Pack::lift(Fl_Widget *tab, int x) {
  lifted_ = tab;
  if (tab && parent()) {
    int left = parent()->x();
    int right = parent()->x() + parent()->w() - tab->w();
    if (x < left) x = left;
    if (x > right) x = right;
  }
  lift_x_ = x;
  redraw();
}

void Tab_Pack::drop() {
  lifted_ = nullptr;
  redraw();
}

void Tab_Pack::draw() {
  Fl_Pack::draw();
  Fl_Widget *tab = lifted_;
  if (!tab || !tab->visible()) return;
  int sx = tab->x();
  int sy = tab->y();
  int sw = tab->w();
  int sh = tab->h();
  // Leave a groove in the original spot to show where the tab will land when released.
  fl_draw_box(FL_THIN_DOWN_BOX, sx, sy, sw, sh,
              fl_color_average(FL_BACKGROUND_COLOR, FL_FOREGROUND_COLOR, 0.92f));
  int nx = lift_x_;
  fl_color(fl_color_average(FL_BLACK, FL_BACKGROUND_COLOR, 0.25f));
  fl_rectf(nx + 2, sy, sw, sh);
  Fl_Boxtype saved = tab->box();
  tab->box(FL_UP_BOX);
  tab->position(nx, sy);
  draw_child(*tab);
  tab->position(sx, sy);
  tab->box(saved);
}

void move_tab_button(Fl_Button *button) {
  auto *pack = static_cast<Fl_Pack *>(button->parent());
  if (!pack) return;
  int from = pack->find(button);
  if (from < 0) return;
  int x = Fl::event_x();
  int to = pack->children();
  for (int i = 0; i < pack->children(); i++) {
    Fl_Widget *child = pack->child(i);
    if (child == button) continue;
    if (x < child->x() + child->w() / 2) {
      to = i;
      break;
    }
  }
  if (to > from) to--;
  if (to == from) return;
  pack->remove(button);
  pack->insert(*button, to);
  layout_tabs();
}

void reorder_window_tree(const std::vector<Window_Node *> &ordered) {
  std::vector<Node *> parents;
  for (Window_Node *window : ordered) {
    if (!window->parent || window->parent->is_widget()) continue;
    if (std::find(parents.begin(), parents.end(), window->parent) == parents.end())
      parents.push_back(window->parent);
  }
  bool needed = false;
  for (Node *parent : parents) {
    std::vector<Window_Node *> current;
    std::vector<Window_Node *> desired;
    for (Node *node = Fluid.proj.tree.first; node; node = node->next) {
      auto *window = dynamic_cast<Window_Node *>(node);
      if (window && window->parent == parent) current.push_back(window);
    }
    for (Window_Node *window : ordered)
      if (window->parent == parent) desired.push_back(window);
    if (current != desired) needed = true;
  }
  if (!needed) return;
  Fluid.proj.undo.checkpoint();
  for (Node *parent : parents) {
    for (size_t i = 0; ; i++) {
      std::vector<Window_Node *> current;
      for (Node *node = Fluid.proj.tree.first; node; node = node->next) {
        auto *window = dynamic_cast<Window_Node *>(node);
        if (window && window->parent == parent) current.push_back(window);
      }
      std::vector<Window_Node *> desired;
      for (Window_Node *window : ordered)
        if (window->parent == parent) desired.push_back(window);
      if (i >= desired.size() || i >= current.size() || current[i] == desired[i]) {
        if (i >= desired.size()) break;
        continue;
      }
      if (desired[i]->level != current[i]->level) break;
      desired[i]->move_before(current[i]);
    }
  }
  Fluid.proj.set_modflag(1);
  if (widget_browser) widget_browser->rebuild();
}

void commit_tab_order() {
  Fl_Pack *pack = g_shell->tab_pack;
  if (!pack) return;
  std::vector<Window_Node *> ordered;
  g_tab_uids.clear();
  for (int i = 0; i < pack->children(); i++) {
    auto *window = static_cast<Window_Node *>(pack->child(i)->user_data());
    if (!window) continue;
    ordered.push_back(window);
    g_tab_uids.push_back(window->get_uid());
  }
  g_tab_file = Fluid.proj.proj_filename;
  save_tab_order();
  reorder_window_tree(ordered);
  sync_chooser(ordered);
}

class Tab_Button : public Fl_Button {
public:
  Tab_Button(int w, int h) : Fl_Button(0, 0, w, h) {}
  int handle(int event) override {
    switch (event) {
      case FL_PUSH:
        if (Fl::event_button() == FL_RIGHT_MOUSE) {
          show_tab_menu(static_cast<Window_Node *>(user_data()));
          return 1;
        }
        if (Fl::event_button() != FL_LEFT_MOUSE) return 1;
        press_x_ = Fl::event_x_root();
        grab_ = Fl::event_x() - x();
        dragging_ = false;
        return 1;
      case FL_DRAG:
        if (!dragging_ && std::abs(Fl::event_x_root() - press_x_) >= 6) {
          dragging_ = true;
          if (window()) window()->cursor(FL_CURSOR_MOVE);
        }
        if (dragging_) {
          move_tab_button(this);
          if (auto *pack = dynamic_cast<Tab_Pack *>(parent()))
            pack->lift(this, Fl::event_x() - grab_);
        }
        return 1;
      case FL_RELEASE:
        if (Fl::event_button() == FL_LEFT_MOUSE && dragging_) {
          if (auto *pack = dynamic_cast<Tab_Pack *>(parent())) pack->drop();
          if (window()) window()->cursor(FL_CURSOR_DEFAULT);
          commit_tab_order();
        } else if (Fl::event_button() == FL_LEFT_MOUSE) {
          do_callback();
        }
        dragging_ = false;
        return 1;
      default:
        return Fl_Button::handle(event);
    }
  }

private:
  int press_x_ = 0;
  int grab_ = 0;
  bool dragging_ = false;
};

void rebuild_tabs(const std::vector<Window_Node *> &windows) {
  Fl_Pack *pack = g_shell->tab_pack;
  if (!pack) return;
  for (int i = pack->children() - 1; i >= 0; i--) {
    Fl_Widget *child = pack->child(i);
    pack->remove(child);
    Fl::delete_widget(child);
  }
  Fl_Group *saved = Fl_Group::current();
  pack->begin();
  for (Window_Node *node : windows) {
    std::string caption = tab_caption(node);
    int button_w = label_width(caption);
    auto *button = new Tab_Button(button_w, 26);
    button->copy_label(caption.c_str());
    button->user_data(node);
    button->callback(tab_cb);
    button->labelsize(12);
    button->box(FL_THIN_UP_BOX);
    if (node == g_shell->shown) button->color(FL_SELECTION_COLOR);
  }
  pack->end();
  Fl_Group::current(saved);
  layout_tabs();
}

void reset_scroll() {
  if (!g_shell->desktop) return;
  if (g_shell->desktop->hbar) g_shell->desktop->hbar->value(0);
  if (g_shell->desktop->vbar) g_shell->desktop->vbar->value(0);
}

std::string saved_layout() {
  Fl_Preferences prefs(Fluid.preferences, "designer");
  char *text = nullptr;
  prefs.get("dock", text, "");
  std::string out = text ? text : "";
  if (text) free(text);
  return out;
}

// The Show/Hide Widget Bin menu label follows whether the widget bin is present.
void sync_bin_menu() {
  if (!g_shell || !g_shell->host || !Fluid.gui.menu_item_widgetbin) return;
  set_menu_label(Fluid.gui.menu_item_widgetbin,
                 g_shell->host->panel_shown(kToolbox) ? "Hide Widget &Bin" : "Show Widget &Bin...");
}

void layout_changed() {
  if (!g_suspend_save) designer_save_layout();
  sync_bin_menu();
}

}  // namespace

void designer_refresh_tabs() {
  if (!g_shell || !g_shell->tab_pack) return;
  for (int i = 0; i < g_shell->tab_pack->children(); i++) {
    Fl_Widget *button = g_shell->tab_pack->child(i);
    auto *window = static_cast<Window_Node *>(button->user_data());
    if (!window) continue;
    apply_caption(button, tab_caption(window));
  }
  layout_tabs();
  std::vector<Window_Node *> windows;
  collect_windows(windows);
  sync_chooser(windows);
}

void designer_preview_tab(Window_Node *window, const char *text) {
  if (!g_shell || !g_shell->tab_pack || !window) return;
  std::string caption = text ? text : "";
  if (caption.empty()) {
    if (!window->label().empty()) caption = window->label();
    else caption = ui_text("Window");
  }
  for (int i = 0; i < g_shell->tab_pack->children(); i++) {
    Fl_Widget *button = g_shell->tab_pack->child(i);
    if (button->user_data() == window) apply_caption(button, caption);
  }
  layout_tabs();
}

void designer_release_windows() {
  if (!g_shell) return;
  // The nodes still exist. Record each window's placement and the index of the one on display.
  std::vector<Window_Node *> windows;
  collect_windows(windows);
  g_place_backup.clear();
  g_shown_backup = -1;
  for (size_t i = 0; i < windows.size(); i++) {
    auto found = g_place.find(windows[i]->o);
    g_place_backup.push_back(found != g_place.end() ? found->second : Point(kMargin, kMargin));
    if (windows[i] == g_shell->shown) g_shown_backup = (int)i;
  }
  g_scroll_backup = Point(0, 0);
  g_focus_backup = false;
  if (g_shell->desktop) {
    g_scroll_backup = Point(g_shell->desktop->scroll_x(), g_shell->desktop->scroll_y());
    g_focus_backup = g_shell->desktop->form_has_focus();
  }
  g_scope_backup = g_shell->scope_all;
  g_have_backup = true;

  g_shell->shown = nullptr;
  g_stored_xy.clear();
  g_place.clear();
  if (widget_browser) widget_browser->scope(nullptr);
  if (g_shell->desktop) g_shell->desktop->set_form(nullptr);
  if (g_shell->tab_pack) {
    for (int i = g_shell->tab_pack->children() - 1; i >= 0; i--) {
      Fl_Widget *child = g_shell->tab_pack->child(i);
      child->user_data(nullptr);
      g_shell->tab_pack->remove(child);
      Fl::delete_widget(child);
    }
  }
  property_page_reset();
  code_console_reset();
}

bool designer_ready() {
  return g_shell != nullptr;
}

void designer_show(Window_Node *window, bool select_in_tree) {
  if (!g_shell) return;
  if (Fluid.proj.undo.paused_) {
    g_pending = true;
    return;
  }
  if (g_shell->busy) return;
  g_shell->busy = true;
  Window_Node *previous = g_shell->shown;
  g_shell->shown = window;
  std::vector<Window_Node *> windows;
  collect_windows(windows);
  if (window && std::find(windows.begin(), windows.end(), window) == windows.end())
    windows.push_back(window);
  if (window != previous) reset_scroll();
  show_windows(windows);
  rebuild_tabs(windows);
  apply_scope();
  sync_chooser(windows);
  if (select_in_tree && window) {
    if (Fluid.proj.tree.current != window || !window->selected) select_only(window);
    if (widget_browser) widget_browser->display(window);
  }
  g_shell->busy = false;
}

void designer_request_refresh() {
  if (g_shell && Fluid.proj.undo.paused_) g_pending = true;
}

void designer_resumed() {
  if (!g_shell || !g_pending || Fluid.proj.undo.paused_) return;
  designer_refresh();
}

void designer_refresh() {
  if (!g_shell) return;
  if (Fluid.proj.undo.paused_) {
    g_pending = true;
    return;
  }
  if (g_shell->busy) return;
  g_pending = false;
  g_shell->busy = true;
  std::vector<Window_Node *> windows;
  collect_windows(windows);
  auto known = [&windows](Window_Node *window) {
    return window && std::find(windows.begin(), windows.end(), window) != windows.end();
  };
  Window_Node *before = g_shell->shown;
  bool lost = g_shell->shown && !known(g_shell->shown);
  if (lost) g_shell->shown = nullptr;
  bool restored = false;
  if (g_have_backup) {
    // Undo or redo has just rebuilt the nodes. Restore placement, the current tab, and the project-tree scope in order.
    g_have_backup = false;
    for (size_t i = 0; i < windows.size() && i < g_place_backup.size(); i++) {
      if (windows[i]->o && !g_place.count(windows[i]->o)) g_place[windows[i]->o] = g_place_backup[i];
    }
    if (!g_shell->shown && g_shown_backup >= 0 && g_shown_backup < (int)windows.size()) {
      g_shell->shown = windows[g_shown_backup];
      g_shell->scope_all = g_scope_backup;
      restored = true;
    }
    g_place_backup.clear();
  } else if (lost) {
    // The project changed, or the window on display was deleted: go back to showing only the current window.
    g_shell->scope_all = false;
  }
  Window_Node *kept = restored ? g_shell->shown : nullptr;
  bool adopted = false;
  Node *current = Fluid.proj.tree.current;
  Window_Node *owner = nullptr;
  if (current) {
    owner = dynamic_cast<Window_Node *>(current);
    if (!owner) owner = current->window();
    if (!known(owner)) owner = nullptr;
  }
  if (owner) {
    // The design area shows the window that owns the current node.
    if (owner != g_shell->shown) {
      g_shell->shown = owner;
      reset_scroll();
    }
  } else if (!g_shell->shown) {
    if (!windows.empty()) {
      g_shell->shown = windows.front();
      adopted = true;
    }
    reset_scroll();
  } else if (current && !g_shell->scope_all && top_of(current) != top_of(g_shell->shown)) {
    // The current node is a function, class, declaration, or similar, and it is not in the tree the project tree is showing: show the whole project.
    g_shell->scope_all = true;
  }
  show_windows(windows);
  if (kept && kept == g_shell->shown && g_shell->desktop)
    g_shell->desktop->scroll_to(g_scroll_backup.first, g_scroll_backup.second);
  rebuild_tabs(windows);
  apply_scope();
  sync_chooser(windows);
  g_shell->busy = false;
  // A window was chosen by default just after a project was opened, or after the window on display was deleted:
  // select that window, the same way as when the user switches windows.
  if (adopted && g_shell->shown) {
    select_only(g_shell->shown);
    if (widget_browser) widget_browser->display(g_shell->shown);
  }
  property_page_load(Fluid.proj.tree.current, true);
  if (!g_shell->desktop) return;
  // Repeated undo must keep the keyboard on the design window.
  // After a project is opened or the window changes, give the keyboard to the design window first so the arrow keys and Delete work at once.
  if (restored ? g_focus_backup : g_shell->shown != before) g_shell->desktop->focus_form();
}

void designer_edit_node(Node *node) {
  if (!g_shell || !node || !g_shell->host) return;
  bool showing = g_shell->host->panel_front(kProps);
  bool same = property_page_shows(node);
  if (showing && same) return;
  if (!same) property_page_load(node);
  if (!showing) g_shell->host->show_panel(kProps);
}

void designer_reveal(Node *node) {
  if (!g_shell || !node) return;
  if (Fluid.proj.undo.paused_) {
    g_pending = true;
    return;
  }
  if (g_shell->busy) return;
  Window_Node *window = dynamic_cast<Window_Node *>(node);
  if (!window) window = node->window();
  if (window) {
    if (window != g_shell->shown) designer_show(window, false);
    return;
  }
  // The selection is a function, class, declaration, or another node that belongs to no window. If it is outside the tree the project tree is showing, show the whole project.
  if (g_shell->scope_all || !g_shell->shown || top_of(node) == top_of(g_shell->shown)) return;
  g_shell->scope_all = true;
  apply_scope();
  std::vector<Window_Node *> windows;
  collect_windows(windows);
  sync_chooser(windows);
  if (widget_browser) widget_browser->display(node);
}

void designer_note_geometry() {
  if (!g_shell || g_shell->busy || g_pinning || g_laying) return;
  if (g_shell->desktop) g_shell->desktop->sync();
}

bool designer_stored_xy(const Fl_Widget *widget, int &x, int &y) {
  auto found = g_stored_xy.find(widget);
  if (found == g_stored_xy.end()) return false;
  x = found->second.first;
  y = found->second.second;
  return true;
}

bool designer_adjust_resize(Fl_Widget *widget, int &x, int &y) {
  if (g_pinning || g_laying || !widget || !g_shell || !g_shell->desktop) return false;
  return g_shell->desktop->client_origin(widget, x, y);
}

void designer_set_stored_xy(Fl_Widget *widget, int x, int y) {
  if (!widget) return;
  g_stored_xy[widget] = Point(x, y);
}

bool designer_drop_window(Fl_Window *window) {
  if (!window || !g_shell || !g_shell->desktop || !g_shell->desktop->viewport) return false;
  Fl_Window *view = g_shell->desktop->viewport;
  int left = view->x_root();
  int top = view->y_root();
  int mx = Fl::event_x_root();
  int my = Fl::event_y_root();
  if (mx < left || my < top || mx >= left + view->w() || my >= top + view->h())
    return false;
  g_shell->desktop->move_frame(window, mx - left + g_shell->desktop->scroll_x(),
                               my - top + g_shell->desktop->scroll_y());
  return true;
}

void designer_new_window_xy(int &x, int &y) {
  int count = 0;
  if (Fluid.proj.tree.first) {
    for (Node *node = Fluid.proj.tree.first; node; node = node->next) {
      if (dynamic_cast<Window_Node *>(node)) count++;
    }
  }
  int slot = std::max(0, count - 1);
  x = 24 + (slot % 6) * 24;
  y = 24 + (slot % 6) * 24;
}

void designer_expect_frame(int viewport_x, int viewport_y) {
  int sx = 0;
  int sy = 0;
  if (g_shell && g_shell->desktop) {
    sx = g_shell->desktop->scroll_x();
    sy = g_shell->desktop->scroll_y();
  }
  g_expect_frame = true;
  g_expect_x = std::max(0, viewport_x + sx);
  g_expect_y = std::max(0, viewport_y + sy);
}

void designer_clear_expect_frame() {
  g_expect_frame = false;
}

void designer_drawn_window_size(int outer_w, int outer_h, int &client_w, int &client_h) {
  int left = 0, top = 0, right = 0, bottom = 0;
  frame_skin_insets(left, top, right, bottom);
  client_w = outer_w - left - right;
  client_h = outer_h - top - bottom;
  if (client_w < kMinSize) client_w = kMinSize;
  if (client_h < kMinSize) client_h = kMinSize;
}

Fl_Window *designer_visible_form() {
  if (!g_shell || !g_shell->desktop) return nullptr;
  return g_shell->desktop->form();
}

Fl_Window *designer_desk_viewport() {
  if (!g_shell || !g_shell->desktop) return nullptr;
  return g_shell->desktop->viewport;
}

void show_kept_panels() {
  if (!g_shell || !g_shell->host) return;
  if (g_keep_props) g_shell->host->show_panel(kProps);
  if (g_keep_bin) g_shell->host->show_panel(kToolbox);
  if (g_keep_snippets) g_shell->host->show_panel(kCode);
  if (g_keep_props && g_fold_props) g_shell->host->fold_panel(kProps, true);
  if (g_keep_bin && g_fold_bin) g_shell->host->fold_panel(kToolbox, true);
  if (g_keep_snippets && g_fold_snippets) g_shell->host->fold_panel(kCode, true);
}

void hide_design_panels() {
  if (!g_shell || !g_shell->host) return;
  if (g_shell->host->panel_shown(kProps)) g_shell->host->hide_panel(kProps);
  if (g_shell->host->panel_shown(kToolbox)) g_shell->host->hide_panel(kToolbox);
  if (g_shell->host->panel_shown(kCode)) g_shell->host->hide_panel(kCode);
}

void relayout_center() {
  if (!g_shell || !g_shell->center) return;
  Fl_Widget *center = g_shell->center;
  center->resize(center->x(), center->y(), center->w(), center->h());
}

void designer_set_code_mode(bool on) {
  if (!g_shell || !g_shell->host || !g_shell->center) return;
  if (g_shell->code_mode == on) {
    if (g_shell->modes) g_shell->modes->show_mode(on);
    return;
  }
  g_suspend_save = true;
  if (on) {
    g_keep_props = g_shell->host->panel_shown(kProps);
    g_keep_bin = g_shell->host->panel_shown(kToolbox);
    g_keep_snippets = g_shell->host->panel_shown(kCode);
    g_fold_props = g_shell->host->panel_collapsed(kProps);
    g_fold_bin = g_shell->host->panel_collapsed(kToolbox);
    g_fold_snippets = g_shell->host->panel_collapsed(kCode);
    hide_design_panels();
    if (Fl_Window *form = g_shell->desktop ? g_shell->desktop->form() : nullptr) form->hide();
  }
  g_shell->code_mode = on;
  code_stage_set_active(on);
  if (!on) {
    show_kept_panels();
    if (Fl_Window *form = g_shell->desktop ? g_shell->desktop->form() : nullptr) form->show();
  }
  g_suspend_save = false;
  relayout_center();
  if (g_shell->modes) g_shell->modes->show_mode(on);
  if (Fluid.gui.menu_item_codeview)
    set_menu_label(Fluid.gui.menu_item_codeview, on ? "Hide Code View" : "Show Code View");
  sync_bin_menu();
}

void designer_toggle_code_mode() {
  designer_set_code_mode(!(g_shell && g_shell->code_mode));
}

bool designer_code_mode() {
  return g_shell && g_shell->code_mode;
}

void designer_show_panel(const char *id) {
  if (g_shell && g_shell->host) g_shell->host->show_panel(id);
}

bool designer_toggle_toolbox() {
  if (!g_shell || !g_shell->host) return false;
  if (g_shell->host->panel_shown(kToolbox)) g_shell->host->hide_panel(kToolbox);
  else g_shell->host->show_panel(kToolbox);
  return g_shell->host->panel_shown(kToolbox);
}

void designer_reset_layout() {
  if (g_shell && g_shell->code_mode) designer_set_code_mode(false);
  if (g_shell && g_shell->host) g_shell->host->reset();
}

void designer_save_layout() {
  if (!g_shell || !g_shell->host) return;
  bool code = g_shell->code_mode;
  if (code) {
    g_suspend_save = true;
    show_kept_panels();
  }
  Fl_Preferences prefs(Fluid.preferences, "designer");
  prefs.set("dock", g_shell->host->save().c_str());
  Fluid.preferences.flush();
  if (code) {
    hide_design_panels();
    g_suspend_save = false;
  }
}

void install_designer_shell() {
  if (g_shell || !Fluid.main_window || !Fluid.gui.node_browser) return;
  Fl_Window *app = Fluid.main_window;

  int sx = 0, sy = 0, sw = 1280, sh = 820;
  Fl::screen_work_area(sx, sy, sw, sh, Fl::screen_num(app->x(), app->y()));
  int ww = std::min(1280, sw);
  int hh = std::min(820, sh);
  app->resize(sx + std::max(0, (sw - ww) / 2), sy + std::max(0, (sh - hh) / 2), ww, hh);
  app->size_range(std::min(960, sw), std::min(620, sh));
  app->label("fluid-plus");

  g_shell = new Shell();
  app->begin();
  g_shell->shell = new ShellGroup(0, 0, app->w(), app->h());
  g_shell->shell->menu = Fluid.gui.app_menu_bar;
  g_shell->shell->add(Fluid.gui.app_menu_bar);
  Fluid.gui.node_browser->hide();
  g_shell->host = new Dock_Host(0, kMenu, app->w(), std::max(1, app->h() - kMenu));
  g_shell->host->end();
  g_shell->shell->host = g_shell->host;
  g_shell->shell->resizable(g_shell->host);
  g_shell->shell->end();
  app->resizable(g_shell->shell);
  app->end();

  // Do not parent these to a window yet. The dock host decides where they go.
  Fl_Group::current(nullptr);
  int strip = tab_strip_height(600);
  g_shell->center = new CenterPane(0, 0, 600, 400);
  g_shell->modes = new Mode_Bar(0, 0, 600, 30);
  g_shell->center->modes = g_shell->modes;
  g_shell->desktop = new DeskPane(0, 0, 600, 370 - strip);
  g_shell->center->desktop = g_shell->desktop;
  g_shell->tabs = new Tab_Strip(0, 400 - strip, 600, strip);
  g_shell->tabs->type(Fl_Scroll::HORIZONTAL);
  g_shell->tabs->box(FL_FLAT_BOX);
  g_shell->tab_pack = new Tab_Pack(0, 400 - strip, 600, kTabButton);
  g_shell->tab_pack->type(Fl_Pack::HORIZONTAL);
  g_shell->tab_pack->spacing(2);
  g_shell->tab_pack->end();
  g_shell->tabs->end();
  g_shell->center->tabs = g_shell->tabs;
  g_shell->center->code = make_code_stage(0, 30, 600, 370);
  g_shell->center->end();

  Fl_Group::current(nullptr);
  g_shell->tree_page = new Tree_Page(0, 0, 300, 500);
  g_shell->tree_page->add(Fluid.gui.node_browser);
  g_shell->tree_page->tree = Fluid.gui.node_browser;
  g_shell->tree_page->chooser->callback(chooser_cb);
  g_shell->tree_page->chooser->copy_tooltip(ui_text("Show only the nodes of this window."));
  g_shell->tree_page->place();
  Fluid.gui.node_browser->has_scrollbar(Fl_Browser_::BOTH);
  Fluid.gui.node_browser->show();

  Fl_Group::current(nullptr);
  Fl_Group *props = make_property_page(0, 0, 300, 500);
  Fl_Group::current(nullptr);
  Fl_Group *toolbox = make_toolbox(0, 0, 230, 500);
  Fl_Group::current(nullptr);
  Fl_Group *console = make_code_console(0, 0, 600, 210);
  Fl_Group::current(nullptr);

  g_shell->host->center(g_shell->center);
  Dock_Home home;
  home.where = Dock_Home::ROOT;
  home.side = Dock_Side::LEFT;
  home.size = 340;   // A property row is 314 wide, plus the scrollbar and the border
  g_shell->host->add_panel(kTree, "Project Tree", g_shell->tree_page, home);
  home.where = Dock_Home::TAB;
  home.with = kTree;
  g_shell->host->add_panel(kProps, "Widget Properties", props, home);
  home.where = Dock_Home::ROOT;
  home.with = nullptr;
  home.side = Dock_Side::RIGHT;
  home.size = 230;
  g_shell->host->add_panel(kToolbox, "Widget Bin", toolbox, home);
  home.where = Dock_Home::CENTER;
  home.side = Dock_Side::BOTTOM;
  home.size = 210;
  g_shell->host->add_panel(kCode, "C++ Code", console, home);

  g_shell->shell->resize(0, 0, app->w(), app->h());
  g_shell->host->restore(saved_layout());
  g_shell->host->on_change(layout_changed);
  sync_bin_menu();
  if (g_shell->desktop->viewport) g_shell->desktop->viewport->show();
  app->init_sizes();
  frame_skin_on_change(skin_changed);
  designer_refresh();
  property_page_load(Fluid.proj.tree.current);
  load_panel();
  g_shell->desktop->focus_form();
  Fl::add_check(desk_check, nullptr);
  Fl_Group::current(nullptr);
  apply_ui_font();
  app->redraw();
}

}  // namespace fluid
