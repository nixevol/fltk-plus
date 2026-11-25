//
// Dockable panels. See dock.h.

#include "app/dock.h"

#include "app/ui_locale.h"

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Tooltip.H>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>
#include <FL/fl_utf8.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace fluid {

namespace dock {

const int kGap = 5;           // width of a splitter
const int kCaption = 22;      // caption height
const int kSideStrip = 24;    // width of a collapsed left or right edge strip
const int kTabs = 24;         // tab bar height
const int kButton = 18;       // caption button size
const int kMinPanel = 90;     // minimum panel width and height
const int kMinCenterW = 240;  // minimum width left for the center
const int kMinCenterH = 160;
const int kEdge = 24;         // this close to a host edge counts as the edge
const int kLine = 3;          // preview frame line width
const int kDragStart = 6;     // pointer travel before a drag starts
const int kFloatMin = 260;    // size range when a panel first floats out
const int kFloatMax = 560;

class Group_Box;
class Float_Window;
struct Model;

// One node of the split tree.
struct Tree_Node {
  enum Kind { SPLIT, GROUP, CENTER };
  Kind kind = GROUP;
  Tree_Node *parent = nullptr;
  bool across = true;               // SPLIT: children run left to right, otherwise top to bottom
  std::vector<Tree_Node *> kids;    // SPLIT
  Group_Box *group = nullptr;       // GROUP
  int size = 200;                   // extent along the parent. A node that contains the center ignores this and takes the rest
  int x = 0, y = 0, w = 0, h = 0;   // position from the last layout
};

struct Panel {
  std::string id;
  std::string title;            // English source
  Fl_Widget *content = nullptr;
  Dock_Home home;
  std::string mate;             // home.with
  Group_Box *group = nullptr;   // null means hidden
  int float_x = -1, float_y = -1, float_w = 0, float_h = 0;  // window position the last time it floated
  int dock_w = 240, dock_h = 200;  // last width when docked left or right, last height when docked top or bottom
};

// Where a panel being dragged would go if released now.
struct Drop {
  enum Kind {
    FLOAT,   // become a floating window
    KEEP,    // drop back in place and do nothing
    TAB,     // join group as a tab
    BESIDE   // dock on node's side
  };
  Kind kind = FLOAT;
  Group_Box *group = nullptr;
  Tree_Node *node = nullptr;   // null means the whole host
  Dock_Side side = Dock_Side::LEFT;
  int size = 0;                // extent along the dock direction
  Fl_Window *window = nullptr; // window the preview is drawn in
  int x = 0, y = 0, w = 0, h = 0;
};

// Drop preview while dragging. Four thin child windows form a frame, so it can cover child windows such as the design area.
class Outline {
public:
  void show(Fl_Window *owner, int x, int y, int w, int h) {
    if (!owner || !owner->shown()) return;
    if (!bars_[0]) {
      Fl_Group *current = Fl_Group::current();
      Fl_Group::current(nullptr);
      for (Fl_Window *&bar : bars_) {
        bar = new Fl_Window(0, 0, 1, 1);
        bar->end();
        bar->border(0);
        bar->box(FL_FLAT_BOX);
        bar->color(FL_SELECTION_COLOR);
      }
      Fl_Group::current(current);
    }
    if (owner_ != owner) {
      hide();
      for (Fl_Window *bar : bars_) {
        if (bar->parent()) bar->parent()->remove(bar);
        owner->add(bar);
      }
      owner_ = owner;
    }
    w = std::max(w, kLine * 2 + 1);
    h = std::max(h, kLine * 2 + 1);
    const int rect[4][4] = {
      {x, y, w, kLine},
      {x, y + h - kLine, w, kLine},
      {x, y + kLine, kLine, h - kLine * 2},
      {x + w - kLine, y + kLine, kLine, h - kLine * 2}
    };
    for (int i = 0; i < 4; i++) {
      Fl_Window *bar = bars_[i];
      if (bar->x() != rect[i][0] || bar->y() != rect[i][1] || bar->w() != rect[i][2] ||
          bar->h() != rect[i][3])
        bar->resize(rect[i][0], rect[i][1], rect[i][2], rect[i][3]);
      // Calling show again on a child window that is already shown raises it above its siblings,
      // so the preview is not covered by child windows such as the design area.
      bar->show();
    }
  }

  void hide() {
    for (Fl_Window *bar : bars_)
      if (bar && bar->shown()) bar->hide();
  }

  // This window is about to be deleted. If the preview is parented to it, take it back first.
  void forget(Fl_Window *owner) {
    if (owner_ != owner) return;
    hide();
    for (Fl_Window *bar : bars_)
      if (bar && bar->parent()) bar->parent()->remove(bar);
    owner_ = nullptr;
  }

private:
  Fl_Window *bars_[4] = {};
  Fl_Window *owner_ = nullptr;
};

// A panel group: the caption, the current panel's content, and tabs below when it holds more than one panel.
class Group_Box : public Fl_Group {
public:
  Model *model;
  std::vector<Panel *> panels;
  int active = 0;
  bool collapsed = false;          // collapsed to a strip. Top and bottom keep the caption; left and right become a vertical edge strip
  Tree_Node *node = nullptr;       // while docked
  Float_Window *floater = nullptr; // while floating

  explicit Group_Box(Model *owner);
  void add_panel(Panel *panel, bool front);
  void remove_panel(Panel *panel);
  void activate(int index);
  bool can_collapse() const;
  bool side_strip() const;
  bool on_left() const;
  int strip_at(int my) const;
  void layout();
  void resize(int x, int y, int w, int h) override;
  void draw() override;
  int handle(int event) override;

private:
  enum Part { NOWHERE, CAPTION, COLLAPSE, FLOAT, CLOSE, TAB };
  Part part_at(int mx, int my, int &tab) const;
  int tabs_height() const;
  void tab_span(int index, int &tx, int &tw) const;
  int button_x(Part part) const;
  void draw_chrome();
  void draw_side_strip();
  void draw_button(Part part, Fl_Color bar);
  void damage_chrome();
  void context_menu();

  Part pressed_ = NOWHERE;
  int pressed_tab_ = -1;
  Part hover_ = NOWHERE;
  int hover_tab_ = -1;
  int press_x_ = 0;
  int press_y_ = 0;
};

// A floating window. It holds one panel group.
class Float_Window : public Fl_Double_Window {
public:
  Model *model;
  Group_Box *group = nullptr;
  Float_Window(Model *owner, int x, int y, int w, int h);
  void resize(int x, int y, int w, int h) override;

private:
  static void close_cb(Fl_Widget *widget, void *);
};

// A saved layout is read into this first. The interface changes only after every part has been checked.
struct Spec {
  char kind = 'C';               // C center, G a panel group, H a left-right split, V a top-bottom split, F a floating window
  std::vector<Spec> kids;
  std::vector<std::string> ids;
  int active = 0;
  int collapsed = 0;
  int size = 200;
  int x = 0, y = 0, w = 0, h = 0;
};

struct Model {
  Dock_Host *host = nullptr;
  Fl_Widget *center = nullptr;
  Tree_Node *root = nullptr;         // always contains the center node
  Tree_Node *center_node = nullptr;
  std::vector<Panel *> panels;       // in registration order
  std::vector<Float_Window *> floats;
  void (*changed)() = nullptr;
  bool building = false;             // rebuilding the whole layout, so do not notify yet

  // splitter being dragged
  Tree_Node *split = nullptr;
  int split_index = 0;
  int split_from = 0;
  int split_a = 0;
  int split_b = 0;
  Fl_Cursor cursor = FL_CURSOR_DEFAULT;

  // panel being dragged
  bool dragging = false;
  Group_Box *drag_group = nullptr;
  Panel *drag_panel = nullptr;       // null means the whole group is being dragged
  int grab_dx = 0;                   // how far the press is from this group's top-left
  int grab_dy = 0;
  Outline outline;

  Panel *find(const std::string &id) const;
  static bool flexible(const Tree_Node *node);
  static bool folded(const Tree_Node *node);
  // This branch has no design area, and every panel is collapsed. Laid out left to right, it should only be as wide as one side strip.
  static bool bar_column(const Tree_Node *node);
  static int extent(const Tree_Node *node);

  void layout();
  void place(Tree_Node *node, int x, int y, int w, int h);
  Tree_Node *leaf_at(Tree_Node *node, int mx, int my) const;
  bool splitter_at(Tree_Node *node, int mx, int my, Tree_Node *&found, int &index) const;
  void point(Fl_Cursor shape);
  void begin_split(Tree_Node *node, int index, int from);
  void drag_split(int to);

  Group_Box *new_group();
  Float_Window *new_float(int x, int y, int w, int h);
  Tree_Node *node_for(Group_Box *group, int size);
  void remove_node(Tree_Node *node);
  void insert_beside(Tree_Node *target, Tree_Node *fresh, Dock_Side side);
  void remember(Panel *panel);
  void home_sizes(Panel *panel);
  void detach(Panel *panel);
  void discard(Group_Box *group);
  void dock_home(Panel *panel, bool front);
  void send_home(Group_Box *group);
  void float_out(Group_Box *group);
  void toggle_float(Group_Box *group);
  void hide(Panel *panel);
  void show(Panel *panel);
  void normalize(Tree_Node *node);
  void settle();
  void notify();
  void clear();
  void defaults();

  Drop target(int rx, int ry) const;
  void begin_drag(Group_Box *group, Panel *panel);
  void drag_to(int rx, int ry);
  void end_drag(int rx, int ry);

  std::string save() const;
  void write(const Tree_Node *node, std::string &out) const;
  bool load(const std::string &saved);
  Tree_Node *build(const Spec &spec);
};

// ---- split tree ----

Panel *Model::find(const std::string &id) const {
  for (Panel *panel : panels)
    if (panel->id == id) return panel;
  return nullptr;
}

// A node that contains the center takes all the remaining space.
bool Model::flexible(const Tree_Node *node) {
  if (node->kind == Tree_Node::CENTER) return true;
  for (const Tree_Node *kid : node->kids)
    if (flexible(kid)) return true;
  return false;
}

bool Model::folded(const Tree_Node *node) {
  return node->kind == Tree_Node::GROUP && node->group->collapsed && node->parent;
}

bool Model::bar_column(const Tree_Node *node) {
  if (!node || flexible(node)) return false;
  if (node->kind == Tree_Node::GROUP) return folded(node);
  if (node->kind != Tree_Node::SPLIT || node->kids.empty()) return false;
  for (const Tree_Node *kid : node->kids)
    if (!bar_column(kid)) return false;
  return true;
}

// How much space the node actually takes along its parent.
int Model::extent(const Tree_Node *node) {
  if (!node->parent) return 0;
  return node->parent->across ? node->w : node->h;
}

void Model::layout() {
  if (!root || !host) return;
  place(root, host->x(), host->y(), host->w(), host->h());
  host->redraw();
}

void Model::place(Tree_Node *node, int x, int y, int w, int h) {
  node->x = x;
  node->y = y;
  node->w = w;
  node->h = h;
  if (node->kind == Tree_Node::CENTER) {
    if (center) center->resize(x, y, std::max(1, w), std::max(1, h));
    return;
  }
  if (node->kind == Tree_Node::GROUP) {
    node->group->resize(x, y, std::max(1, w), std::max(1, h));
    return;
  }
  int count = (int)node->kids.size();
  if (!count) return;
  int total = std::max(0, (node->across ? w : h) - kGap * (count - 1));
  std::vector<int> size(count, 0);
  std::vector<bool> pinned(count, false);  // a collapsed group takes only the caption and does not flex
  int flex = -1;
  int pinned_total = 0;
  int wanted = 0;
  for (int i = 0; i < count; i++) {
    Tree_Node *kid = node->kids[i];
    if (flexible(kid)) {
      flex = i;
      continue;
    }
    if (node->across && (folded(kid) || bar_column(kid))) {
      // Against the left or right of the design area, and the whole branch is collapsed: keep one side strip, and do not take the original width.
      size[i] = kSideStrip;
      pinned[i] = true;
      pinned_total += kSideStrip;
      continue;
    }
    if (folded(kid)) {
      size[i] = kCaption;
      pinned[i] = true;
      pinned_total += kCaption;
      continue;
    }
    size[i] = std::max(kMinPanel, kid->size);
    wanted += size[i];
  }
  int room = total - pinned_total;
  if (flex >= 0) room -= node->across ? kMinCenterW : kMinCenterH;
  room = std::max(0, room);
  if (wanted > 0 && (flex < 0 || wanted > room)) {
    // With no center, fill in proportion; when there is not enough room, shrink in proportion.
    int used = 0;
    int last = -1;
    for (int i = 0; i < count; i++) {
      if (i == flex || pinned[i]) continue;
      size[i] = (int)((long long)size[i] * room / wanted);
      if (flex >= 0) size[i] = std::max(40, size[i]);
      used += size[i];
      last = i;
    }
    if (flex < 0 && last >= 0) size[last] += room - used;
  }
  if (flex >= 0) {
    int others = 0;
    for (int i = 0; i < count; i++)
      if (i != flex) others += size[i];
    size[flex] = std::max(1, total - others);
  } else if (wanted == 0) {
    // Stacked top to bottom and all collapsed to side strips: split the height evenly, and do not pile the spare space on the last piece.
    bool side = !node->across && w <= kSideStrip + kGap;
    if (side) {
      int each = count > 0 ? total / count : 0;
      int used = 0;
      for (int i = 0; i < count; i++) {
        size[i] = each;
        used += each;
      }
      if (count > 0) size[count - 1] += total - used;
    } else {
      size[count - 1] += total - pinned_total;
    }
  }
  int at = node->across ? x : y;
  for (int i = 0; i < count; i++) {
    if (node->across) place(node->kids[i], at, y, size[i], h);
    else place(node->kids[i], x, at, w, size[i]);
    at += size[i] + kGap;
  }
}

Tree_Node *Model::leaf_at(Tree_Node *node, int mx, int my) const {
  if (mx < node->x || my < node->y || mx >= node->x + node->w || my >= node->y + node->h)
    return nullptr;
  if (node->kind != Tree_Node::SPLIT) return node;
  for (Tree_Node *kid : node->kids)
    if (Tree_Node *leaf = leaf_at(kid, mx, my)) return leaf;
  return nullptr;
}

bool Model::splitter_at(Tree_Node *node, int mx, int my, Tree_Node *&found, int &index) const {
  if (node->kind != Tree_Node::SPLIT) return false;
  for (int i = 0; i + 1 < (int)node->kids.size(); i++) {
    const Tree_Node *before = node->kids[i];
    int gx = node->across ? before->x + before->w : node->x;
    int gy = node->across ? node->y : before->y + before->h;
    int gw = node->across ? kGap : node->w;
    int gh = node->across ? node->h : kGap;
    if (mx >= gx && mx < gx + gw && my >= gy && my < gy + gh) {
      found = node;
      index = i;
      return true;
    }
  }
  for (Tree_Node *kid : node->kids)
    if (splitter_at(kid, mx, my, found, index)) return true;
  return false;
}

void Model::point(Fl_Cursor shape) {
  if (shape == cursor) return;
  cursor = shape;
  if (host && host->window()) host->window()->cursor(shape);
}

void Model::begin_split(Tree_Node *node, int index, int from) {
  split = node;
  split_index = index;
  split_from = from;
  // Record each panel's size as its current extent first. Afterwards only the two sides of the splitter change.
  for (Tree_Node *kid : node->kids)
    if (!flexible(kid) && !folded(kid) && !bar_column(kid) && extent(kid) > 0)
      kid->size = extent(kid);
  // Collapsed to a side strip, the width on screen is not the expanded width. Do not overwrite the remembered size with it.
  Tree_Node *a = node->kids[index];
  Tree_Node *b = node->kids[index + 1];
  split_a = bar_column(a) ? a->size : extent(a);
  split_b = bar_column(b) ? b->size : extent(b);
}

void Model::drag_split(int to) {
  if (!split) return;
  Tree_Node *a = split->kids[split_index];
  Tree_Node *b = split->kids[split_index + 1];
  int floor_center = split->across ? kMinCenterW : kMinCenterH;
  int min_a = flexible(a) ? floor_center : kMinPanel;
  int min_b = flexible(b) ? floor_center : kMinPanel;
  int lowest = std::min(0, min_a - split_a);
  int highest = std::max(0, split_b - min_b);
  int move = std::max(lowest, std::min(to - split_from, highest));
  if (!flexible(a) && !bar_column(a)) a->size = split_a + move;
  if (!flexible(b) && !bar_column(b)) b->size = split_b - move;
  layout();
}

Group_Box *Model::new_group() {
  Fl_Group *current = Fl_Group::current();
  Fl_Group::current(nullptr);
  auto *group = new Group_Box(this);
  Fl_Group::current(current);
  host->add(group);
  return group;
}

Float_Window *Model::new_float(int x, int y, int w, int h) {
  int sx = 0, sy = 0, sw = 1280, sh = 800;
  Fl::screen_work_area(sx, sy, sw, sh, x, y);
  w = std::max(220, std::min(w, sw));
  h = std::max(180, std::min(h, sh));
  x = std::max(sx, std::min(x, sx + sw - 120));
  y = std::max(sy + 30, std::min(y, sy + sh - 80));
  Fl_Group *current = Fl_Group::current();
  Fl_Group::current(nullptr);
  auto *window = new Float_Window(this, x, y, w, h);
  Fl_Group::current(current);
  floats.push_back(window);
  return window;
}

Tree_Node *Model::node_for(Group_Box *group, int size) {
  auto *node = new Tree_Node();
  node->kind = Tree_Node::GROUP;
  node->group = group;
  node->size = size;
  group->node = node;
  return node;
}

void Model::remove_node(Tree_Node *node) {
  Tree_Node *parent = node->parent;
  if (!parent) return;
  parent->kids.erase(std::find(parent->kids.begin(), parent->kids.end(), node));
  delete node;
  if (parent->kids.size() != 1) return;
  // The split has only one child left. Replace the split with that child.
  Tree_Node *only = parent->kids[0];
  Tree_Node *up = parent->parent;
  only->parent = up;
  only->size = parent->size;
  if (up) *std::find(up->kids.begin(), up->kids.end(), parent) = only;
  else root = only;
  delete parent;
  if (!up || only->kind != Tree_Node::SPLIT || only->across != up->across) return;
  // The child that moved up is a split in the same direction. Merge it into the parent.
  auto at = std::find(up->kids.begin(), up->kids.end(), only);
  at = up->kids.erase(at);
  for (Tree_Node *kid : only->kids) {
    kid->parent = up;
    if (!flexible(kid) && !folded(kid)) {
      int now = only->across ? kid->w : kid->h;
      if (now > 0) kid->size = now;
    }
  }
  up->kids.insert(at, only->kids.begin(), only->kids.end());
  delete only;
}

void Model::insert_beside(Tree_Node *target, Tree_Node *fresh, Dock_Side side) {
  bool across = side == Dock_Side::LEFT || side == Dock_Side::RIGHT;
  bool before = side == Dock_Side::LEFT || side == Dock_Side::TOP;
  if (target->kind == Tree_Node::SPLIT && target->across == across) {
    // Place it on the outer edge of the whole split.
    fresh->parent = target;
    target->kids.insert(before ? target->kids.begin() : target->kids.end(), fresh);
    return;
  }
  int span = across ? target->w : target->h;
  Tree_Node *parent = target->parent;
  if (parent && parent->across == across) {
    auto at = std::find(parent->kids.begin(), parent->kids.end(), target);
    parent->kids.insert(before ? at : at + 1, fresh);
    fresh->parent = parent;
    if (!flexible(target) && span > 0) target->size = std::max(kMinPanel, span - fresh->size - kGap);
    return;
  }
  // Different direction: replace the target with a new split that holds the target and the new panel.
  auto *wrap = new Tree_Node();
  wrap->kind = Tree_Node::SPLIT;
  wrap->across = across;
  wrap->size = target->size;
  wrap->parent = parent;
  if (parent) *std::find(parent->kids.begin(), parent->kids.end(), target) = wrap;
  else root = wrap;
  target->parent = wrap;
  fresh->parent = wrap;
  if (before) wrap->kids = {fresh, target};
  else wrap->kids = {target, fresh};
  if (!flexible(target)) target->size = std::max(kMinPanel, span - fresh->size - kGap);
}

// ---- panels coming and going ----

void Model::remember(Panel *panel) {
  Group_Box *group = panel->group;
  if (!group) return;
  if (group->floater) {
    if (!group->floater->shown()) return;
    panel->float_x = group->floater->x();
    panel->float_y = group->floater->y();
    panel->float_w = group->floater->w();
    panel->float_h = group->floater->h();
    return;
  }
  if (!group->node || !group->node->parent || group->w() <= 1) return;
  if (group->collapsed) return;
  if (group->node->parent->across) panel->dock_w = group->w();
  else panel->dock_h = group->h();
}

void Model::home_sizes(Panel *panel) {
  bool across = panel->home.side == Dock_Side::LEFT || panel->home.side == Dock_Side::RIGHT;
  panel->dock_w = across ? panel->home.size : 260;
  panel->dock_h = across ? 220 : panel->home.size;
}

void Model::detach(Panel *panel) {
  Group_Box *group = panel->group;
  if (!group) return;
  remember(panel);
  group->remove_panel(panel);
  if (group->panels.empty()) discard(group);
}

// This group is empty. The window and widgets are deleted after this event has been handled.
void Model::discard(Group_Box *group) {
  if (Float_Window *window = group->floater) {
    floats.erase(std::remove(floats.begin(), floats.end(), window), floats.end());
    outline.forget(window);
    window->hide();
    Fl::delete_widget(window);
    return;
  }
  if (group->node) {
    remove_node(group->node);
    group->node = nullptr;
  }
  if (group->parent()) group->parent()->remove(group);
  Fl::delete_widget(group);
}

void Model::dock_home(Panel *panel, bool front) {
  if (panel->home.where == Dock_Home::TAB) {
    Panel *mate = find(panel->mate);
    if (mate && mate != panel && mate->group && mate->group->node) {
      mate->group->add_panel(panel, front);
      return;
    }
  }
  bool across = panel->home.side == Dock_Side::LEFT || panel->home.side == Dock_Side::RIGHT;
  Group_Box *group = new_group();
  group->add_panel(panel, true);
  Tree_Node *fresh = node_for(group, across ? panel->dock_w : panel->dock_h);
  Tree_Node *beside = panel->home.where == Dock_Home::CENTER ? center_node : root;
  insert_beside(beside, fresh, panel->home.side);
}

void Model::send_home(Group_Box *group) {
  std::vector<Panel *> moving = group->panels;
  for (Panel *panel : moving) detach(panel);
  for (Panel *panel : moving) dock_home(panel, true);
  settle();
}

void Model::float_out(Group_Box *group) {
  if (group->panels.empty()) return;
  std::vector<Panel *> moving = group->panels;
  Panel *front = moving[std::min(group->active, (int)moving.size() - 1)];
  int fx = front->float_x, fy = front->float_y, fw = front->float_w, fh = front->float_h;
  Fl_Window *top = group->window();
  if (fw <= 0 || fh <= 0) {
    // Docked on one side, it may be narrow and tall, so a floating window does not take that size as-is.
    fw = std::max(kFloatMin, std::min(group->w(), kFloatMax));
    fh = std::max(kFloatMin, std::min(group->h(), kFloatMax));
  }
  if (fx < 0 && top) {
    fx = top->x_root() + group->x() + 40;
    fy = top->y_root() + group->y() + 40;
  }
  for (Panel *panel : moving) detach(panel);
  Float_Window *window = new_float(fx, fy, fw, fh);
  for (Panel *panel : moving) window->group->add_panel(panel, panel == front);
  window->show();
  settle();
}

void Model::toggle_float(Group_Box *group) {
  if (group->floater) send_home(group);
  else float_out(group);
}

void Model::hide(Panel *panel) {
  if (!panel->group) return;
  detach(panel);
  settle();
}

void Model::show(Panel *panel) {
  if (!panel->group) dock_home(panel, true);
  Group_Box *group = panel->group;
  if (!group) return;
  group->collapsed = false;
  group->activate((int)(std::find(group->panels.begin(), group->panels.end(), panel) -
                        group->panels.begin()));
  if (group->floater) group->floater->show();
  settle();
}

// A group with no dock position cannot collapse. Once it has floated, the collapsed state is cleared.
void Model::normalize(Tree_Node *node) {
  if (!node) return;
  if (node->kind == Tree_Node::GROUP && !node->parent) node->group->collapsed = false;
  for (Tree_Node *kid : node->kids) normalize(kid);
}

void Model::settle() {
  normalize(root);
  layout();
  notify();
}

void Model::notify() {
  if (!building && changed) changed();
}

void Model::clear() {
  for (Panel *panel : panels)
    if (panel->group) detach(panel);
}

void Model::defaults() {
  building = true;
  clear();
  for (Panel *panel : panels) {
    home_sizes(panel);
    panel->float_x = panel->float_y = -1;
    panel->float_w = panel->float_h = 0;
  }
  // In a group of panels, the one registered first is shown in front.
  for (Panel *panel : panels) dock_home(panel, false);
  building = false;
  normalize(root);
  layout();
}

// ---- dragging panels ----

namespace {

// A strip along one side of a rectangle, with thickness size.
void strip(Drop &drop, int x, int y, int w, int h, Dock_Side side, int size) {
  drop.side = side;
  drop.size = size;
  drop.x = x;
  drop.y = y;
  drop.w = w;
  drop.h = h;
  switch (side) {
    case Dock_Side::LEFT: drop.w = size; break;
    case Dock_Side::RIGHT: drop.x = x + w - size; drop.w = size; break;
    case Dock_Side::TOP: drop.h = size; break;
    case Dock_Side::BOTTOM: drop.y = y + h - size; drop.h = size; break;
  }
}

bool is_across(Dock_Side side) {
  return side == Dock_Side::LEFT || side == Dock_Side::RIGHT;
}

}  // namespace

Drop Model::target(int rx, int ry) const {
  Drop drop;
  if (!drag_group) return drop;
  // Floating windows sit above the main window, so check them first.
  for (Float_Window *window : floats) {
    if (!window->shown()) continue;
    if (rx < window->x() || ry < window->y() || rx >= window->x() + window->w() ||
        ry >= window->y() + window->h())
      continue;
    if (window->group == drag_group) {
      // The whole floating window is being dragged: it follows the pointer, so look at what is underneath.
      if (!drag_panel) continue;
      drop.kind = Drop::KEEP;
      return drop;
    }
    drop.kind = Drop::TAB;
    drop.group = window->group;
    drop.window = window;
    drop.w = window->w();
    drop.h = window->h();
    return drop;
  }
  Fl_Window *top = host->window();
  if (!top || !root) return drop;
  int mx = rx - top->x_root();
  int my = ry - top->y_root();
  if (mx < host->x() || my < host->y() || mx >= host->x() + host->w() ||
      my >= host->y() + host->h())
    return drop;
  drop.window = top;
  Panel *front = drag_panel;
  if (!front && !drag_group->panels.empty())
    front = drag_group->panels[std::min(drag_group->active, (int)drag_group->panels.size() - 1)];
  int want_w = front ? front->dock_w : 240;
  int want_h = front ? front->dock_h : 200;

  // On a host edge: span that whole edge.
  bool edge = true;
  Dock_Side side = Dock_Side::LEFT;
  if (mx < host->x() + kEdge) side = Dock_Side::LEFT;
  else if (mx >= host->x() + host->w() - kEdge) side = Dock_Side::RIGHT;
  else if (my < host->y() + kEdge) side = Dock_Side::TOP;
  else if (my >= host->y() + host->h() - kEdge) side = Dock_Side::BOTTOM;
  else edge = false;
  if (edge) {
    int span = is_across(side) ? host->w() : host->h();
    int size = std::max(kMinPanel, std::min(is_across(side) ? want_w : want_h, span * 2 / 5));
    drop.kind = Drop::BESIDE;
    drop.node = nullptr;
    strip(drop, host->x(), host->y(), host->w(), host->h(), side, size);
    return drop;
  }

  Tree_Node *leaf = leaf_at(root, mx, my);
  if (!leaf) return drop;  // on a splitter
  bool own = leaf->kind == Tree_Node::GROUP && leaf->group == drag_group;
  if (own && !drag_panel) {
    drop.kind = Drop::KEEP;
    return drop;
  }
  // Which edge of this block is nearest. Far from all four edges means the middle.
  double fx = (mx - leaf->x) / (double)std::max(1, leaf->w);
  double fy = (my - leaf->y) / (double)std::max(1, leaf->h);
  double nearest = fx;
  side = Dock_Side::LEFT;
  if (1 - fx < nearest) { nearest = 1 - fx; side = Dock_Side::RIGHT; }
  if (fy < nearest) { nearest = fy; side = Dock_Side::TOP; }
  if (1 - fy < nearest) { nearest = 1 - fy; side = Dock_Side::BOTTOM; }
  if (nearest >= 0.25) {
    if (leaf->kind == Tree_Node::CENTER) return drop;  // cannot dock in the middle of the design area, so it floats
    if (own) {
      drop.kind = Drop::KEEP;
      return drop;
    }
    drop.kind = Drop::TAB;
    drop.group = leaf->group;
    drop.x = leaf->x;
    drop.y = leaf->y;
    drop.w = leaf->w;
    drop.h = leaf->h;
    return drop;
  }
  int span = is_across(side) ? leaf->w : leaf->h;
  int size;
  if (leaf->kind == Tree_Node::GROUP) size = std::max(40, (span - kGap) / 2);  // split half and half with this group
  else size = std::max(kMinPanel, std::min(is_across(side) ? want_w : want_h, span * 9 / 20));
  drop.kind = Drop::BESIDE;
  drop.node = leaf;
  strip(drop, leaf->x, leaf->y, leaf->w, leaf->h, side, size);
  return drop;
}

void Model::begin_drag(Group_Box *group, Panel *panel) {
  dragging = true;
  drag_group = group;
  drag_panel = panel;
  for (Panel *each : group->panels) remember(each);
  if (Fl_Window *window = group->window()) window->cursor(FL_CURSOR_MOVE);
}

void Model::drag_to(int rx, int ry) {
  if (!dragging) return;
  // The whole floating window follows the pointer, so it does not cover the place it would dock.
  if (!drag_panel && drag_group && drag_group->floater)
    drag_group->floater->position(rx - grab_dx, ry - grab_dy);
  Drop drop = target(rx, ry);
  if (drop.kind == Drop::TAB || drop.kind == Drop::BESIDE)
    outline.show(drop.window, drop.x, drop.y, drop.w, drop.h);
  else
    outline.hide();
}

void Model::end_drag(int rx, int ry) {
  if (!dragging) return;
  Drop drop = target(rx, ry);
  outline.hide();
  Group_Box *group = drag_group;
  Panel *single = drag_panel;
  dragging = false;
  drag_group = nullptr;
  drag_panel = nullptr;
  if (!group || group->panels.empty()) return;
  if (Fl_Window *window = group->window()) window->cursor(FL_CURSOR_DEFAULT);
  cursor = FL_CURSOR_DEFAULT;
  if (drop.kind == Drop::KEEP) return;

  std::vector<Panel *> moving;
  Panel *front = single;
  if (single) {
    moving.push_back(single);
  } else {
    moving = group->panels;
    front = moving[std::min(group->active, (int)moving.size() - 1)];
  }

  if (drop.kind == Drop::FLOAT) {
    if (!single && group->floater) {
      // The whole floating window: the drag already moved it with the pointer.
      notify();
      return;
    }
    int fw = std::max(kFloatMin, std::min(group->w(), kFloatMax));
    int fh = std::max(kFloatMin, std::min(group->h(), kFloatMax));
    if (front->float_w > 0 && front->float_h > 0) {
      fw = front->float_w;
      fh = front->float_h;
    }
    if (group->floater) {
      fw = group->floater->w();
      fh = group->floater->h();
    }
    for (Panel *panel : moving) detach(panel);
    Float_Window *window = new_float(rx - std::min(grab_dx, fw - 40), ry - std::min(grab_dy, kCaption - 4), fw, fh);
    for (Panel *panel : moving) window->group->add_panel(panel, panel == front);
    window->show();
  } else if (drop.kind == Drop::TAB) {
    Group_Box *into = drop.group;
    for (Panel *panel : moving) detach(panel);
    for (Panel *panel : moving) into->add_panel(panel, panel == front);
    if (into->floater) into->floater->show();
  } else {
    for (Panel *panel : moving) detach(panel);
    Group_Box *fresh = new_group();
    for (Panel *panel : moving) fresh->add_panel(panel, panel == front);
    insert_beside(drop.node ? drop.node : root, node_for(fresh, drop.size), drop.side);
  }
  settle();
}

// ---- save and restore ----
//
// Of the form  1|H[G(tree,props:0:0)@300 V[C G(code:0:0)@210]@0 G(toolbox:0:0)@230]@0|F(props:0)@80,90,320,420;|code
// The first part is the split tree, the second is floating windows, and the third is hidden panels.

void Model::write(const Tree_Node *node, std::string &out) const {
  if (node->kind == Tree_Node::CENTER) {
    out += 'C';
    return;
  }
  int size = node->size;
  if (node->parent && !flexible(node) && !folded(node) && !bar_column(node) && extent(node) > 0)
    size = extent(node);
  if (node->kind == Tree_Node::GROUP) {
    out += "G(";
    for (size_t i = 0; i < node->group->panels.size(); i++) {
      if (i) out += ',';
      out += node->group->panels[i]->id;
    }
    out += ':' + std::to_string(node->group->active) + ':' + (node->group->collapsed ? "1" : "0") + ")@" +
           std::to_string(size);
    return;
  }
  out += node->across ? "H[" : "V[";
  for (size_t i = 0; i < node->kids.size(); i++) {
    if (i) out += ' ';
    write(node->kids[i], out);
  }
  out += "]@" + std::to_string(size);
}

std::string Model::save() const {
  std::string out = "1|";
  if (root) write(root, out);
  out += '|';
  for (const Float_Window *window : floats) {
    if (!window->group || window->group->panels.empty()) continue;
    out += "F(";
    for (size_t i = 0; i < window->group->panels.size(); i++) {
      if (i) out += ',';
      out += window->group->panels[i]->id;
    }
    out += ':' + std::to_string(window->group->active) + ")@" + std::to_string(window->x()) + ',' +
           std::to_string(window->y()) + ',' + std::to_string(window->w()) + ',' +
           std::to_string(window->h()) + ';';
  }
  out += '|';
  bool first = true;
  for (const Panel *panel : panels) {
    if (panel->group) continue;
    if (!first) out += ',';
    out += panel->id;
    first = false;
  }
  return out;
}

namespace {

struct Reader {
  const std::string &text;
  size_t at = 0;

  explicit Reader(const std::string &source) : text(source) {}

  bool eat(char c) {
    if (at < text.size() && text[at] == c) {
      at++;
      return true;
    }
    return false;
  }

  bool number(int &value) {
    size_t from = at;
    bool minus = eat('-');
    long long total = 0;
    size_t digits = 0;
    while (at < text.size() && text[at] >= '0' && text[at] <= '9' && digits < 7) {
      total = total * 10 + (text[at] - '0');
      at++;
      digits++;
    }
    if (!digits) {
      at = from;
      return false;
    }
    value = (int)(minus ? -total : total);
    return true;
  }

  bool word(std::string &out) {
    size_t from = at;
    while (at < text.size() && (std::isalnum((unsigned char)text[at]) || text[at] == '_')) at++;
    if (at == from) return false;
    out = text.substr(from, at - from);
    return true;
  }

  bool ids(std::vector<std::string> &out) {
    do {
      std::string id;
      if (!word(id)) return false;
      out.push_back(id);
    } while (eat(','));
    return true;
  }

  bool node(Spec &out, int depth) {
    if (depth > 12) return false;
    if (eat('C')) {
      out.kind = 'C';
      return true;
    }
    if (eat('G')) {
      out.kind = 'G';
      return eat('(') && ids(out.ids) && eat(':') && number(out.active) && eat(':') &&
             number(out.collapsed) && eat(')') && eat('@') && number(out.size);
    }
    if (at < text.size() && (text[at] == 'H' || text[at] == 'V')) {
      out.kind = text[at++];
      if (!eat('[')) return false;
      do {
        Spec kid;
        if (!node(kid, depth + 1)) return false;
        out.kids.push_back(kid);
      } while (eat(' '));
      return eat(']') && eat('@') && number(out.size);
    }
    return false;
  }
};

void collect(const Spec &spec, int &centers, std::vector<std::string> &ids) {
  if (spec.kind == 'C') centers++;
  ids.insert(ids.end(), spec.ids.begin(), spec.ids.end());
  for (const Spec &kid : spec.kids) collect(kid, centers, ids);
}

}  // namespace

Tree_Node *Model::build(const Spec &spec) {
  if (spec.kind == 'C') {
    center_node->parent = nullptr;
    return center_node;
  }
  if (spec.kind == 'G') {
    std::vector<Panel *> members;
    for (const std::string &id : spec.ids)
      if (Panel *panel = find(id)) members.push_back(panel);  // skip an unrecognized panel
    if (members.empty()) return nullptr;
    Group_Box *group = new_group();
    for (Panel *panel : members) group->add_panel(panel, false);
    group->collapsed = spec.collapsed != 0;
    group->activate(std::max(0, std::min(spec.active, (int)members.size() - 1)));
    return node_for(group, std::max(40, std::min(spec.size, 6000)));
  }
  auto *node = new Tree_Node();
  node->kind = Tree_Node::SPLIT;
  node->across = spec.kind == 'H';
  node->size = std::max(40, std::min(spec.size, 6000));
  for (const Spec &kid : spec.kids) {
    Tree_Node *made = build(kid);
    if (!made) continue;
    made->parent = node;
    node->kids.push_back(made);
  }
  if (node->kids.empty()) {
    delete node;
    return nullptr;
  }
  if (node->kids.size() == 1) {
    Tree_Node *only = node->kids[0];
    only->parent = nullptr;
    only->size = node->size;
    delete node;
    return only;
  }
  return node;
}

bool Model::load(const std::string &saved) {
  Reader reader(saved);
  Spec tree;
  std::vector<Spec> windows;
  std::vector<std::string> hidden;
  if (!reader.eat('1') || !reader.eat('|') || !reader.node(tree, 0) || !reader.eat('|')) return false;
  while (reader.eat('F')) {
    Spec window;
    window.kind = 'F';
    if (!(reader.eat('(') && reader.ids(window.ids) && reader.eat(':') && reader.number(window.active) &&
          reader.eat(')') && reader.eat('@') && reader.number(window.x) && reader.eat(',') &&
          reader.number(window.y) && reader.eat(',') && reader.number(window.w) && reader.eat(',') &&
          reader.number(window.h) && reader.eat(';')))
      return false;
    windows.push_back(window);
  }
  if (!reader.eat('|')) return false;
  if (reader.at < saved.size() && !reader.ids(hidden)) return false;
  if (reader.at != saved.size()) return false;

  // Exactly one center, and each panel appears at most once.
  int centers = 0;
  std::vector<std::string> seen;
  collect(tree, centers, seen);
  for (const Spec &window : windows) seen.insert(seen.end(), window.ids.begin(), window.ids.end());
  seen.insert(seen.end(), hidden.begin(), hidden.end());
  if (centers != 1) return false;
  std::sort(seen.begin(), seen.end());
  if (std::adjacent_find(seen.begin(), seen.end()) != seen.end()) return false;

  building = true;
  clear();
  root = build(tree);
  if (!root) root = center_node;  // will not happen: the tree always has a center
  root->parent = nullptr;
  for (const Spec &spec : windows) {
    std::vector<Panel *> members;
    for (const std::string &id : spec.ids)
      if (Panel *panel = find(id)) members.push_back(panel);
    if (members.empty()) continue;
    Float_Window *window = new_float(spec.x, spec.y, spec.w, spec.h);
    for (Panel *panel : members) window->group->add_panel(panel, false);
    window->group->activate(std::max(0, std::min(spec.active, (int)members.size() - 1)));
    window->show();
  }
  // A panel added after the layout was saved goes to its default place.
  for (Panel *panel : panels) {
    if (panel->group) continue;
    if (std::find(hidden.begin(), hidden.end(), panel->id) != hidden.end()) continue;
    dock_home(panel, false);
  }
  building = false;
  normalize(root);
  layout();
  return true;
}

// ---- panel group ----

Group_Box::Group_Box(Model *owner) : Fl_Group(0, 0, 10, 10), model(owner) {
  box(FL_NO_BOX);
  end();
}

void Group_Box::add_panel(Panel *panel, bool front) {
  panels.push_back(panel);
  panel->group = this;
  if (panel->content->parent() != this) add(panel->content);
  if (front || panels.size() == 1) active = (int)panels.size() - 1;
  layout();
}

void Group_Box::remove_panel(Panel *panel) {
  auto found = std::find(panels.begin(), panels.end(), panel);
  if (found == panels.end()) return;
  int index = (int)(found - panels.begin());
  panels.erase(found);
  panel->group = nullptr;
  // Hide it before removing it, so keyboard focus leaves it.
  if (panel->content->visible()) panel->content->hide();
  if (panel->content->parent() == this) remove(panel->content);
  if (active > index || active >= (int)panels.size()) active = std::max(0, active - 1);
  layout();
}

void Group_Box::activate(int index) {
  if (index < 0 || index >= (int)panels.size()) return;
  active = index;
  layout();
}

bool Group_Box::can_collapse() const {
  return node && node->parent;
}

// Collapsed against the left or right, as one vertical strip. When stacked top to bottom and the whole column is collapsed, the column width shrinks to that strip too.
bool Group_Box::side_strip() const {
  if (!collapsed || !node || !node->parent) return false;
  if (node->parent->across) return true;
  return w() <= kSideStrip + kGap;
}

int Group_Box::strip_at(int my) const {
  int count = std::max(1, (int)panels.size());
  int each = std::max(1, h() / count);
  int index = (my - y()) / each;
  if (index < 0) index = 0;
  if (index >= count) index = count - 1;
  return index;
}

bool Group_Box::on_left() const {
  if (!model->center) return true;
  return x() + w() / 2 < model->center->x() + model->center->w() / 2;
}

int Group_Box::tabs_height() const {
  return (!collapsed && panels.size() > 1) ? kTabs : 0;
}

void Group_Box::layout() {
  int tabs = tabs_height();
  bool room = !collapsed && h() > kCaption + tabs + 8;
  for (size_t i = 0; i < panels.size(); i++) {
    Fl_Widget *content = panels[i]->content;
    if ((int)i == active && room) {
      content->resize(x() + 1, y() + kCaption, std::max(1, w() - 2), std::max(1, h() - kCaption - tabs - 1));
      if (!content->visible()) content->show();
    } else if (content->visible()) {
      content->hide();
    }
  }
  if (floater && !panels.empty())
    floater->copy_label(ui_text(panels[std::min(active, (int)panels.size() - 1)]->title.c_str()));
  redraw();
}

void Group_Box::resize(int x, int y, int w, int h) {
  Fl_Widget::resize(x, y, w, h);
  layout();
}

int Group_Box::button_x(Part part) const {
  int right = x() + w() - 2;
  if (part == CLOSE) return right - kButton;
  if (part == FLOAT) return right - kButton * 2;
  return right - kButton * 3;
}

void Group_Box::tab_span(int index, int &tx, int &tw) const {
  fl_font(FL_HELVETICA, 12);
  int count = (int)panels.size();
  int room = std::max(1, w() - 2);
  std::vector<int> width(count, 0);
  int total = 0;
  for (int i = 0; i < count; i++) {
    width[i] = (int)fl_width(ui_text(panels[i]->title.c_str())) + 24;
    total += width[i];
  }
  if (total > room)
    for (int i = 0; i < count; i++) width[i] = room / count;
  tx = x() + 1;
  for (int i = 0; i < index; i++) tx += width[i];
  tw = width[index];
}

Group_Box::Part Group_Box::part_at(int mx, int my, int &tab) const {
  tab = -1;
  if (mx < x() || mx >= x() + w() || my < y() || my >= y() + h()) return NOWHERE;
  // On the edge strip, the piece that is clicked is the panel about to open.
  if (side_strip()) {
    tab = strip_at(my);
    return COLLAPSE;
  }
  if (my < y() + kCaption) {
    if (mx >= button_x(CLOSE)) return CLOSE;
    if (mx >= button_x(FLOAT)) return FLOAT;
    if (can_collapse() && mx >= button_x(COLLAPSE)) return COLLAPSE;
    return CAPTION;
  }
  int tabs = tabs_height();
  if (tabs && my >= y() + h() - tabs) {
    for (int i = 0; i < (int)panels.size(); i++) {
      int tx = 0, tw = 0;
      tab_span(i, tx, tw);
      if (mx >= tx && mx < tx + tw) {
        tab = i;
        return TAB;
      }
    }
    return CAPTION;  // blank space on the tab bar works like the caption, and the drag moves the whole group
  }
  return NOWHERE;
}

void Group_Box::draw_button(Part part, Fl_Color bar) {
  int bx = button_x(part);
  int by = y() + 2;
  if (hover_ == part || pressed_ == part)
    fl_rectf(bx, by, kButton, kButton, fl_color_average(bar, FL_FOREGROUND_COLOR, pressed_ == part ? 0.72f : 0.84f));
  int cx = bx + kButton / 2;
  int cy = by + kButton / 2;
  fl_color(FL_FOREGROUND_COLOR);
  switch (part) {
    case CLOSE:
      fl_line(cx - 4, cy - 4, cx + 4, cy + 4);
      fl_line(cx - 4, cy + 4, cx + 4, cy - 4);
      break;
    case FLOAT:
      fl_rect(cx - 5, cy - 4, 11, 9);
      if (floater) fl_rectf(cx - 5, cy - 4, 4, 9);   // dock it back: a thick bar along the left of the window
      else fl_rectf(cx - 5, cy - 4, 11, 3);          // float: a small window with a caption
      break;
    case COLLAPSE:
      if (node && node->parent && node->parent->across) {
        // On the left the arrow points left to collapse and right to expand; on the right it is the other way around.
        bool point_left = on_left() ? !collapsed : collapsed;
        if (point_left) fl_polygon(cx + 3, cy - 5, cx - 2, cy, cx + 3, cy + 5);
        else fl_polygon(cx - 2, cy - 5, cx + 3, cy, cx - 2, cy + 5);
      } else if (collapsed) {
        fl_polygon(cx - 5, cy + 2, cx + 5, cy + 2, cx, cy - 3);
      } else {
        fl_polygon(cx - 5, cy - 2, cx + 5, cy - 2, cx, cy + 3);
      }
      break;
    default:
      break;
  }
}

void Group_Box::draw_side_strip() {
  Fl_Color bar = fl_color_average(FL_BACKGROUND_COLOR, FL_FOREGROUND_COLOR, 0.90f);
  Fl_Color edge = fl_color_average(FL_BACKGROUND_COLOR, FL_FOREGROUND_COLOR, 0.66f);
  int count = std::max(1, (int)panels.size());
  int each = std::max(1, h() / count);
  bool left = on_left();
  fl_font(FL_HELVETICA, 12);
  int line = fl_height();
  for (int i = 0; i < count; i++) {
    int sy = y() + i * each;
    int sh = (i == count - 1) ? y() + h() - sy : each;
    bool hot = hover_ == COLLAPSE && hover_tab_ == i;
    Fl_Color fill = hot ? fl_color_average(bar, FL_SELECTION_COLOR, 0.82f) : bar;
    fl_rectf(x(), sy, w(), sh, fill);
    int cx = x() + w() / 2;
    int cy = sy + 12;
    fl_color(FL_FOREGROUND_COLOR);
    if (left) fl_polygon(cx - 2, cy - 5, cx + 3, cy, cx - 2, cy + 5);
    else fl_polygon(cx + 3, cy - 5, cx - 2, cy, cx + 3, cy + 5);
    if (i < (int)panels.size()) {
      const char *text = ui_text(panels[i]->title.c_str());
      int top = sy + 28;
      int room = sh - 32;
      const char *p = text;
      while (*p && room >= line) {
        int n = fl_utf8len(*p);
        if (n < 1) break;
        char glyph[8];
        if (n >= (int)sizeof(glyph)) n = (int)sizeof(glyph) - 1;
        std::memcpy(glyph, p, n);
        glyph[n] = 0;
        fl_color(FL_FOREGROUND_COLOR);
        fl_draw(glyph, x(), top, w(), line, FL_ALIGN_CENTER | FL_ALIGN_CLIP, nullptr, 0);
        top += line;
        room -= line;
        p += n;
      }
    }
    fl_color(edge);
    fl_rect(x(), sy, w(), sh);
  }
}

void Group_Box::draw_chrome() {
  if (side_strip()) {
    draw_side_strip();
    return;
  }
  Fl_Color bar = fl_color_average(FL_BACKGROUND_COLOR, FL_FOREGROUND_COLOR, 0.90f);
  Fl_Color edge = fl_color_average(FL_BACKGROUND_COLOR, FL_FOREGROUND_COLOR, 0.66f);
  fl_rectf(x(), y(), w(), h(), FL_BACKGROUND_COLOR);
  fl_rectf(x(), y(), w(), kCaption, bar);
  int left = button_x(can_collapse() ? COLLAPSE : FLOAT);
  if (!panels.empty()) {
    fl_font(FL_HELVETICA, 12);
    fl_color(FL_FOREGROUND_COLOR);
    fl_draw(ui_text(panels[std::min(active, (int)panels.size() - 1)]->title.c_str()), x() + 8, y(),
            std::max(1, left - x() - 12), kCaption, FL_ALIGN_LEFT | FL_ALIGN_CLIP, nullptr, 0);
  }
  if (can_collapse()) draw_button(COLLAPSE, bar);
  draw_button(FLOAT, bar);
  draw_button(CLOSE, bar);

  int tabs = tabs_height();
  if (tabs) {
    int ty = y() + h() - tabs;
    fl_rectf(x(), ty, w(), tabs, bar);
    for (int i = 0; i < (int)panels.size(); i++) {
      int tx = 0, tw = 0;
      tab_span(i, tx, tw);
      bool on = i == active;
      if (on) {
        fl_rectf(tx, ty, tw, tabs - 1, FL_BACKGROUND2_COLOR);
        fl_rectf(tx, ty + tabs - 3, tw, 2, FL_SELECTION_COLOR);
      } else if (hover_ == TAB && hover_tab_ == i) {
        fl_rectf(tx, ty + 1, tw, tabs - 2, fl_color_average(bar, FL_BACKGROUND2_COLOR, 0.5f));
      }
      fl_font(FL_HELVETICA, 12);
      fl_color(on ? FL_FOREGROUND_COLOR : fl_color_average(FL_FOREGROUND_COLOR, bar, 0.72f));
      fl_draw(ui_text(panels[i]->title.c_str()), tx + 4, ty, std::max(1, tw - 8), tabs - 2,
              FL_ALIGN_CENTER | FL_ALIGN_CLIP, nullptr, 0);
    }
    fl_color(edge);
    fl_xyline(x(), ty, x() + w() - 1);
  }
  fl_color(edge);
  fl_rect(x(), y(), w(), h());
}

void Group_Box::draw() {
  fl_push_clip(x(), y(), w(), h());
  if (damage() & ~FL_DAMAGE_CHILD) {
    draw_chrome();
    for (int i = 0; i < children(); i++) draw_child(*child(i));
  } else {
    for (int i = 0; i < children(); i++) update_child(*child(i));
  }
  fl_pop_clip();
}

void Group_Box::damage_chrome() {
  if (side_strip()) {
    damage(FL_DAMAGE_ALL);
    return;
  }
  damage(FL_DAMAGE_ALL, x(), y(), w(), kCaption);
  int tabs = tabs_height();
  if (tabs) damage(FL_DAMAGE_ALL, x(), y() + h() - tabs, w(), tabs);
}

void Group_Box::context_menu() {
  Fl_Menu_Item items[4];
  std::memset(items, 0, sizeof(items));
  items[0].text = ui_text(floater ? "Dock Panel" : "Float Panel");
  items[1].text = ui_text("Hide Panel");
  items[1].flags = FL_MENU_DIVIDER;
  items[2].text = ui_text("Reset Window Layout");
  for (int i = 0; i < 3; i++) {
    items[i].labelsize_ = 13;
    items[i].labelcolor_ = FL_FOREGROUND_COLOR;
  }
  const Fl_Menu_Item *picked = items->popup(Fl::event_x(), Fl::event_y());
  if (!picked) return;
  Model *owner = model;
  switch ((int)(picked - items)) {
    case 0: owner->toggle_float(this); break;
    case 1:
      if (!panels.empty()) owner->hide(panels[std::min(active, (int)panels.size() - 1)]);
      break;
    case 2:
      owner->defaults();
      owner->notify();
      break;
  }
}

int Group_Box::handle(int event) {
  int tab = -1;
  switch (event) {
    case FL_ENTER:
    case FL_MOVE: {
      Part part = part_at(Fl::event_x(), Fl::event_y(), tab);
      if (part != hover_ || tab != hover_tab_) {
        hover_ = part;
        hover_tab_ = tab;
        damage_chrome();
      }
      if (part == NOWHERE) break;
      Fl::belowmouse(this);
      const char *tip = nullptr;
      if (part == CLOSE) tip = ui_text("Hide Panel");
      else if (part == FLOAT) tip = ui_text(floater ? "Dock Panel" : "Float Panel");
      else if (part == COLLAPSE) tip = ui_text(collapsed ? "Expand Panel" : "Collapse Panel");
      if (tip && side_strip()) Fl_Tooltip::enter_area(this, x(), y(), w(), h(), tip);
      else if (tip) Fl_Tooltip::enter_area(this, button_x(part), y(), kButton, kCaption, tip);
      else Fl_Tooltip::enter_area(this, 0, 0, 0, 0, nullptr);
      return 1;
    }
    case FL_LEAVE:
      if (hover_ != NOWHERE) {
        hover_ = NOWHERE;
        hover_tab_ = -1;
        damage_chrome();
      }
      break;
    case FL_PUSH: {
      Part part = part_at(Fl::event_x(), Fl::event_y(), tab);
      if (part == NOWHERE) break;
      if (Fl::event_button() == FL_RIGHT_MOUSE) {
        context_menu();
        return 1;
      }
      if (Fl::event_button() != FL_LEFT_MOUSE) return 1;
      if (part == TAB) activate(tab);
      if (part == CAPTION && Fl::event_clicks() > 0) {
        // Double-click the caption to toggle between docked and floating. This group may be gone afterwards.
        pressed_ = NOWHERE;
        model->toggle_float(this);
        return 1;
      }
      pressed_ = part;
      pressed_tab_ = tab;
      press_x_ = Fl::event_x_root();
      press_y_ = Fl::event_y_root();
      model->grab_dx = Fl::event_x() - x();
      model->grab_dy = Fl::event_y() - y();
      damage_chrome();
      return 1;
    }
    case FL_DRAG:
      if (pressed_ == NOWHERE) break;
      if (pressed_ == CAPTION || pressed_ == TAB) {
        if (!model->dragging) {
          if (std::abs(Fl::event_x_root() - press_x_) < kDragStart &&
              std::abs(Fl::event_y_root() - press_y_) < kDragStart)
            return 1;
          bool one = pressed_ == TAB && panels.size() > 1 && pressed_tab_ >= 0 &&
                     pressed_tab_ < (int)panels.size();
          model->begin_drag(this, one ? panels[pressed_tab_] : nullptr);
        }
        model->drag_to(Fl::event_x_root(), Fl::event_y_root());
      }
      return 1;
    case FL_RELEASE: {
      if (pressed_ == NOWHERE) break;
      Part was = pressed_;
      pressed_ = NOWHERE;
      damage_chrome();
      if (model->dragging) {
        // This group may be gone afterwards.
        model->end_drag(Fl::event_x_root(), Fl::event_y_root());
        return 1;
      }
      if (part_at(Fl::event_x(), Fl::event_y(), tab) != was) return 1;
      if (was == COLLAPSE) {
        // When a group holds several pieces, the one clicked expands and becomes the current one.
        if (side_strip() && panels.size() > 1 && tab >= 0 && tab < (int)panels.size()) {
          activate(tab);
          collapsed = false;
        } else {
          collapsed = !collapsed;
        }
        model->settle();
      } else if (was == FLOAT) {
        model->toggle_float(this);
      } else if (was == CLOSE && !panels.empty()) {
        model->hide(panels[std::min(active, (int)panels.size() - 1)]);
      }
      return 1;
    }
  }
  return Fl_Group::handle(event);
}

// ---- floating window ----

Float_Window::Float_Window(Model *owner, int x, int y, int w, int h)
  : Fl_Double_Window(x, y, w, h), model(owner) {
  group = new Group_Box(owner);
  group->floater = this;
  end();
  group->resize(0, 0, w, h);
  resizable(group);
  size_range(220, 180);
  set_non_modal();
  callback(close_cb);
}

void Float_Window::resize(int x, int y, int w, int h) {
  Fl_Double_Window::resize(x, y, w, h);
  if (group) group->resize(0, 0, w, h);
}

// Closing a floating window docks its panels back to their default places. They are not lost.
void Float_Window::close_cb(Fl_Widget *widget, void *) {
  if (Fl::event() == FL_SHORTCUT && Fl::event_key() == FL_Escape) return;
  auto *window = static_cast<Float_Window *>(widget);
  window->model->send_home(window->group);
}

}  // namespace dock

// ---- host ----

Dock_Host::Dock_Host(int x, int y, int w, int h) : Fl_Group(x, y, w, h) {
  box(FL_FLAT_BOX);
  color(FL_BACKGROUND_COLOR);
  model_ = new dock::Model();
  model_->host = this;
  model_->center_node = new dock::Tree_Node();
  model_->center_node->kind = dock::Tree_Node::CENTER;
  model_->root = model_->center_node;
}

void Dock_Host::center(Fl_Widget *widget) {
  model_->center = widget;
  if (widget && widget->parent() != this) add(widget);
}

void Dock_Host::add_panel(const char *id, const char *title, Fl_Widget *content, const Dock_Home &home) {
  if (!id || !content || model_->find(id)) return;
  auto *panel = new dock::Panel();
  panel->id = id;
  panel->title = title ? title : id;
  panel->content = content;
  panel->home = home;
  panel->mate = home.with ? home.with : "";
  panel->home.with = nullptr;
  model_->home_sizes(panel);
  if (content->parent()) content->parent()->remove(content);
  content->hide();
  model_->panels.push_back(panel);
}

void Dock_Host::restore(const std::string &saved) {
  if (saved.empty() || !model_->load(saved)) model_->defaults();
}

std::string Dock_Host::save() const {
  return model_->save();
}

void Dock_Host::reset() {
  model_->defaults();
  model_->notify();
}

void Dock_Host::show_panel(const char *id) {
  if (dock::Panel *panel = model_->find(id ? id : "")) model_->show(panel);
}

void Dock_Host::hide_panel(const char *id) {
  if (dock::Panel *panel = model_->find(id ? id : "")) model_->hide(panel);
}

bool Dock_Host::panel_shown(const char *id) const {
  dock::Panel *panel = model_->find(id ? id : "");
  return panel && panel->group;
}

bool Dock_Host::panel_collapsed(const char *id) const {
  dock::Panel *panel = model_->find(id ? id : "");
  return panel && panel->group && panel->group->collapsed;
}

void Dock_Host::fold_panel(const char *id, bool on) {
  dock::Panel *panel = model_->find(id ? id : "");
  if (!panel || !panel->group || !panel->group->can_collapse()) return;
  if (panel->group->collapsed == on) return;
  panel->group->collapsed = on;
  model_->settle();
}

bool Dock_Host::panel_front(const char *id) const {
  dock::Panel *panel = model_->find(id ? id : "");
  if (!panel || !panel->group) return false;
  dock::Group_Box *group = panel->group;
  if (group->panels.empty() || group->panels[std::min(group->active, (int)group->panels.size() - 1)] != panel)
    return false;
  if (group->floater) return group->floater->shown() != 0;
  return !(group->node && dock::Model::folded(group->node));
}

void Dock_Host::on_change(void (*changed)()) {
  model_->changed = changed;
}

void Dock_Host::resize(int x, int y, int w, int h) {
  Fl_Widget::resize(x, y, w, h);
  model_->layout();
}

int Dock_Host::handle(int event) {
  dock::Model *model = model_;
  dock::Tree_Node *found = nullptr;
  int index = 0;
  switch (event) {
    case FL_ENTER:
    case FL_MOVE:
      if (model->root && !model->dragging &&
          model->splitter_at(model->root, Fl::event_x(), Fl::event_y(), found, index) &&
          !dock::Model::folded(found->kids[index]) && !dock::Model::folded(found->kids[index + 1])) {
        model->point(found->across ? FL_CURSOR_WE : FL_CURSOR_NS);
        Fl::belowmouse(this);
        return 1;
      }
      model->point(FL_CURSOR_DEFAULT);
      break;
    case FL_LEAVE:
      if (!model->split) model->point(FL_CURSOR_DEFAULT);
      break;
    case FL_PUSH:
      if (Fl::event_button() == FL_LEFT_MOUSE && model->root &&
          model->splitter_at(model->root, Fl::event_x(), Fl::event_y(), found, index) &&
          !dock::Model::folded(found->kids[index]) && !dock::Model::folded(found->kids[index + 1])) {
        model->begin_split(found, index, found->across ? Fl::event_x() : Fl::event_y());
        return 1;
      }
      break;
    case FL_DRAG:
      if (model->split) {
        model->drag_split(model->split->across ? Fl::event_x() : Fl::event_y());
        return 1;
      }
      break;
    case FL_RELEASE:
      if (model->split) {
        model->split = nullptr;
        model->notify();
        return 1;
      }
      break;
  }
  return Fl_Group::handle(event);
}

}  // namespace fluid
