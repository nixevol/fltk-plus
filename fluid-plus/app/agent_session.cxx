// Project commands for scripts and for MCP clients.
// Each call reloads the .fl file. The designer window is not opened.

#include "app/agent_session.h"

#include "Fluid.h"
#include "app/export_actions.h"
#include "app/ui_locale.h"
#include "io/Code_Writer.h"
#include "io/Project_Reader.h"
#include "io/Project_Writer.h"
#include "io/String_Writer.h"
#include "nodes/Function_Node.h"
#include "nodes/Widget_Node.h"
#include "nodes/Window_Node.h"
#include "nodes/factory.h"
#include "tools/filename.h"

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/filename.H>
#include <FL/fl_utf8.h>

#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <map>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

namespace fluid {

namespace {

struct J {
  enum Kind { Null, Bool, Number, String, Array, Object } kind = Null;
  bool b = false;
  double n = 0;
  std::string s;
  std::vector<J> a;
  std::vector<std::pair<std::string, J>> o;

  const J *find(const std::string &key) const {
    if (kind != Object) return nullptr;
    for (const auto &item : o) if (item.first == key) return &item.second;
    return nullptr;
  }
};

struct Outcome {
  bool ok = false;
  std::string json;
  std::vector<std::string> png;
  std::string error;
};

struct Request {
  std::string project;
  std::string window;
  std::string widget;
  std::string type;
  std::string parent;
  std::string label;
  std::string name;
  std::string field;
  std::string code;
  std::string image_out;
  std::map<std::string, std::string> props;
};

std::string json_escape(const std::string &text) {
  std::string out;
  out.reserve(text.size() + 8);
  for (unsigned char c : text) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          char buf[8];
          snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out.push_back((char)c);
        }
    }
  }
  return out;
}

std::string q(const std::string &text) {
  return "\"" + json_escape(text) + "\"";
}

std::string uid_text(unsigned short uid) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%04x", uid);
  return buf;
}

std::string base64_encode(const std::string &raw) {
  static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve((raw.size() + 2) / 3 * 4);
  unsigned value = 0;
  int bits = -6;
  for (unsigned char c : raw) {
    value = (value << 8) + c;
    bits += 8;
    while (bits >= 0) {
      out.push_back(table[(value >> bits) & 63]);
      bits -= 6;
    }
  }
  if (bits > -6) out.push_back(table[((value << 8) >> (bits + 8)) & 63]);
  while (out.size() % 4) out.push_back('=');
  return out;
}

std::string read_binary(const std::string &path) {
  FILE *file = fl_fopen(path.c_str(), "rb");
  if (!file) return std::string();
  std::string data;
  char buf[4096];
  size_t n = 0;
  while ((n = fread(buf, 1, sizeof(buf), file)) > 0) data.append(buf, n);
  fclose(file);
  return data;
}

std::string temp_png_path() {
  static int seq = 0;
  char name[64];
  snprintf(name, sizeof(name), "fluid-plus-snap-%d.png", ++seq);
#ifdef _WIN32
  const char *raw = fl_getenv("TEMP");
  if (!raw || !raw[0]) raw = fl_getenv("TMP");
  std::string dir = raw ? raw : std::string(".");
  return end_with_slash(fix_separators(dir)) + name;
#else
  return std::string("/tmp/") + name;
#endif
}

class Parser {
public:
  const char *p;
  const char *end;
  bool ok = true;

  void skip() {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
  }

  J parse() {
    skip();
    if (p >= end) { ok = false; return {}; }
    if (*p == '{') return object();
    if (*p == '[') return array();
    if (*p == '"') return J{J::String, false, 0, string(), {}, {}};
    if (*p == 't' || *p == 'f') return boolean();
    if (*p == 'n') { literal("null"); return {}; }
    if (*p == '-' || (*p >= '0' && *p <= '9')) return number();
    ok = false;
    return {};
  }

private:
  void literal(const char *word) {
    for (const char *c = word; *c; c++, p++) {
      if (p >= end || *p != *c) { ok = false; return; }
    }
  }

  std::string string() {
    std::string out;
    if (p >= end || *p != '"') { ok = false; return out; }
    p++;
    while (p < end && *p != '"') {
      if (*p == '\\') {
        p++;
        if (p >= end) { ok = false; break; }
        char c = *p++;
        if (c == 'n') out.push_back('\n');
        else if (c == 'r') out.push_back('\r');
        else if (c == 't') out.push_back('\t');
        else if (c == 'u') {
          unsigned code = 0;
          for (int i = 0; i < 4 && p < end; i++, p++) {
            char h = *p;
            code <<= 4;
            if (h >= '0' && h <= '9') code += h - '0';
            else if (h >= 'a' && h <= 'f') code += h - 'a' + 10;
            else if (h >= 'A' && h <= 'F') code += h - 'A' + 10;
          }
          if (code < 0x80) out.push_back((char)code);
          else if (code < 0x800) {
            out.push_back((char)(0xc0 | (code >> 6)));
            out.push_back((char)(0x80 | (code & 0x3f)));
          } else {
            out.push_back((char)(0xe0 | (code >> 12)));
            out.push_back((char)(0x80 | ((code >> 6) & 0x3f)));
            out.push_back((char)(0x80 | (code & 0x3f)));
          }
        } else out.push_back(c);
      } else {
        out.push_back(*p++);
      }
    }
    if (p < end && *p == '"') p++;
    else ok = false;
    return out;
  }

  J number() {
    const char *start = p;
    if (*p == '-') p++;
    while (p < end && ((*p >= '0' && *p <= '9') || *p == '.' || *p == 'e' || *p == 'E' || *p == '+' || *p == '-')) p++;
    J value;
    value.kind = J::Number;
    value.n = atof(std::string(start, p).c_str());
    return value;
  }

  J boolean() {
    J value;
    value.kind = J::Bool;
    if (p + 4 <= end && std::string(p, p + 4) == "true") { value.b = true; p += 4; }
    else if (p + 5 <= end && std::string(p, p + 5) == "false") { value.b = false; p += 5; }
    else ok = false;
    return value;
  }

  J object() {
    J value;
    value.kind = J::Object;
    p++;
    skip();
    if (p < end && *p == '}') { p++; return value; }
    while (ok && p < end) {
      skip();
      std::string key = string();
      skip();
      if (p >= end || *p != ':') { ok = false; break; }
      p++;
      value.o.push_back({key, parse()});
      skip();
      if (p < end && *p == ',') { p++; continue; }
      if (p < end && *p == '}') { p++; break; }
      ok = false;
    }
    return value;
  }

  J array() {
    J value;
    value.kind = J::Array;
    p++;
    skip();
    if (p < end && *p == ']') { p++; return value; }
    while (ok && p < end) {
      value.a.push_back(parse());
      skip();
      if (p < end && *p == ',') { p++; continue; }
      if (p < end && *p == ']') { p++; break; }
      ok = false;
    }
    return value;
  }
};

J parse_json(const std::string &text, bool &ok) {
  Parser parser { text.data(), text.data() + text.size(), true };
  J value = parser.parse();
  ok = parser.ok;
  return value;
}

std::string as_text(const J &value) {
  if (value.kind == J::String) return value.s;
  if (value.kind == J::Number) {
    if (value.n == (int)value.n) return std::to_string((int)value.n);
    return std::to_string(value.n);
  }
  if (value.kind == J::Bool) return value.b ? "true" : "false";
  return std::string();
}

Outcome fail(const std::string &message) {
  Outcome out;
  out.ok = false;
  out.error = message;
  out.json = "{\"ok\":false,\"error\":" + q(message) + "}";
  return out;
}

Outcome succeed(const std::string &body) {
  Outcome out;
  out.ok = true;
  out.json = body;
  return out;
}

bool load_project(const std::string &path, std::string &error) {
  if (path.empty()) {
    error = "Missing project path.";
    return false;
  }
  std::string absolute = fl_filename_absolute_str(path);
  Fluid.proj.tree.delete_all_nodes();
  Fluid.proj.set_modflag(0, 0);
  Fluid.proj.undo.suspend();
  int opened = fluid::io::read_file(Fluid.proj, absolute, 0);
  Fluid.proj.undo.resume();
  if (!opened) {
    error = std::string("Can't read project file: ") + strerror(errno);
    return false;
  }
  Fluid.proj.set_filename(absolute);
  Fluid.proj.set_modflag(0, 0);
  return true;
}

bool save_project(std::string &error) {
  if (Fluid.proj.proj_filename.empty()) {
    error = "The project has no filename.";
    return false;
  }
  if (!fluid::io::write_file(Fluid.proj, Fluid.proj.proj_filename.c_str(), 0)) {
    error = std::string("Can't write project file: ") + strerror(errno);
    return false;
  }
  Fluid.proj.set_modflag(0, 0);
  return true;
}

int hex_value(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool parse_uid(const std::string &text, unsigned short &uid) {
  if (text.size() != 4) return false;
  int value = 0;
  for (char c : text) {
    int digit = hex_value(c);
    if (digit < 0) return false;
    value = (value << 4) + digit;
  }
  uid = (unsigned short)value;
  return true;
}

std::string id_of(Node *node) {
  if (!node->name().empty()) return node->name();
  return uid_text(node->get_uid());
}

Node *find_node(const std::string &id, std::string &error) {
  if (id.empty()) {
    error = "Missing widget or window id.";
    return nullptr;
  }
  std::vector<Node *> named;
  Node *by_uid = nullptr;
  unsigned short uid = 0;
  bool has_uid = parse_uid(id, uid);
  for (Node *node = Fluid.proj.tree.first; node; node = node->next) {
    if (!node->name().empty() && node->name() == id) named.push_back(node);
    if (has_uid && node->get_uid() == uid) by_uid = node;
  }
  if (named.size() == 1) return named[0];
  if (named.size() > 1) {
    error = "More than one node uses this name. Pass the uid.";
    return nullptr;
  }
  if (by_uid) return by_uid;
  error = "No node matches \"" + id + "\".";
  return nullptr;
}

Window_Node *find_window(const std::string &id, std::string &error) {
  if (id.empty()) {
    error = "Missing window id.";
    return nullptr;
  }
  std::vector<Window_Node *> named;
  std::vector<Window_Node *> labeled;
  Window_Node *by_uid = nullptr;
  unsigned short uid = 0;
  bool has_uid = parse_uid(id, uid);
  for (Node *node = Fluid.proj.tree.first; node; node = node->next) {
    auto *window = dynamic_cast<Window_Node *>(node);
    if (!window || dynamic_cast<Widget_Class_Node *>(window)) continue;
    if (!window->name().empty() && window->name() == id) named.push_back(window);
    if (!window->label().empty() && window->label() == id) labeled.push_back(window);
    if (has_uid && window->get_uid() == uid) by_uid = window;
  }
  if (named.size() == 1) return named[0];
  if (named.size() > 1) { error = "More than one window uses this name."; return nullptr; }
  if (labeled.size() == 1) return labeled[0];
  if (labeled.size() > 1) { error = "More than one window uses this label. Pass the name or uid."; return nullptr; }
  if (by_uid) return by_uid;
  error = "No window matches \"" + id + "\".";
  return nullptr;
}

std::string widget_fields(Widget_Node *widget) {
  std::string out;
  if (!widget || !widget->o) return out;
  out += ",\"x\":" + std::to_string(widget->o->x());
  out += ",\"y\":" + std::to_string(widget->o->y());
  out += ",\"w\":" + std::to_string(widget->o->w());
  out += ",\"h\":" + std::to_string(widget->o->h());
  out += std::string(",\"visible\":") + (widget->shown_in_project() ? "true" : "false");
  out += std::string(",\"active\":") + (widget->o->active() ? "true" : "false");
  out += std::string(",\"resizable\":") + (widget->resizable() ? "true" : "false");
  out += ",\"tooltip\":" + q(widget->tooltip());
  return out;
}

std::string node_object(Node *node, bool with_children) {
  std::string out = "{";
  out += "\"type\":" + q(node->type_name());
  out += ",\"uid\":" + q(uid_text(node->get_uid()));
  out += ",\"name\":" + q(node->name());
  out += ",\"label\":" + q(node->label());
  out += ",\"id\":" + q(id_of(node));
  if (node->parent) out += ",\"parent\":" + q(id_of(node->parent));
  auto *widget = dynamic_cast<Widget_Node *>(node);
  if (widget && widget->is_true_widget()) out += widget_fields(widget);
  if (with_children) {
    out += ",\"children\":[";
    bool first = true;
    for (Node *child = node->first_child(); child; child = child->next_sibling()) {
      if (!first) out += ",";
      first = false;
      out += node_object(child, true);
    }
    out += "]";
  }
  out += "}";
  return out;
}

bool truth(const std::string &text) {
  return text == "1" || text == "true" || text == "yes";
}

bool apply_props(Node *node, const std::map<std::string, std::string> &props, std::string &error) {
  auto *widget = dynamic_cast<Widget_Node *>(node);
  int x = widget && widget->o ? widget->o->x() : 0;
  int y = widget && widget->o ? widget->o->y() : 0;
  int w = widget && widget->o ? widget->o->w() : 0;
  int h = widget && widget->o ? widget->o->h() : 0;
  bool moved = false;
  for (const auto &item : props) {
    const std::string &key = item.first;
    const std::string &value = item.second;
    if (key == "name") node->name(value);
    else if (key == "label") node->label(value);
    else if (key == "callback") node->callback(value);
    else if (key == "user_data") node->user_data(value);
    else if (key == "user_data_type") node->user_data_type(value);
    else if (key == "comment") node->comment(value);
    else if (key == "tooltip") {
      if (!widget) { error = "tooltip applies only to a widget."; return false; }
      widget->tooltip(value);
    } else if (key == "extra0" || key == "extra1" || key == "extra2" || key == "extra3") {
      if (!widget) { error = "extra code applies only to a widget."; return false; }
      widget->extra_code(key[5] - '0', value);
    } else if (key == "x" || key == "y" || key == "w" || key == "h") {
      if (!widget || !widget->o) { error = "Position applies only to a widget."; return false; }
      int number = atoi(value.c_str());
      if (key == "x") x = number;
      else if (key == "y") y = number;
      else if (key == "w") w = number;
      else h = number;
      moved = true;
    } else if (key == "visible") {
      if (!widget || !widget->o) { error = "visible applies only to a widget."; return false; }
      if (truth(value)) widget->o->show(); else widget->o->hide();
    } else if (key == "active") {
      if (!widget || !widget->o) { error = "active applies only to a widget."; return false; }
      if (truth(value)) widget->o->activate(); else widget->o->deactivate();
    } else if (key == "resizable") {
      if (!widget) { error = "resizable applies only to a widget."; return false; }
      widget->resizable(truth(value) ? 1 : 0);
    } else {
      error = "Unknown property \"" + key + "\".";
      return false;
    }
  }
  if (moved && widget && widget->o) widget->o->resize(x, y, w, h);
  return true;
}

std::map<std::string, std::string> props_from_json(const J *object) {
  std::map<std::string, std::string> props;
  if (!object || object->kind != J::Object) return props;
  for (const auto &item : object->o) props[item.first] = as_text(item.second);
  return props;
}

Outcome list_windows() {
  std::string body = "{\"ok\":true,\"windows\":[";
  bool first = true;
  for (Node *node = Fluid.proj.tree.first; node; node = node->next) {
    auto *window = dynamic_cast<Window_Node *>(node);
    if (!window || dynamic_cast<Widget_Class_Node *>(window)) continue;
    if (!first) body += ",";
    first = false;
    body += "{\"name\":" + q(window->name());
    body += ",\"label\":" + q(window->label());
    body += ",\"uid\":" + q(uid_text(window->get_uid()));
    body += ",\"id\":" + q(id_of(window)) + "}";
  }
  body += "]}";
  return succeed(body);
}

Outcome describe_window(const Request &request) {
  std::string error;
  Window_Node *window = find_window(request.window, error);
  if (!window) return fail(error);
  return succeed("{\"ok\":true,\"window\":" + node_object(window, true) + "}");
}

Outcome read_widget(const Request &request) {
  std::string error;
  Node *node = find_node(request.widget, error);
  if (!node) return fail(error);
  std::string body = "{\"ok\":true,\"node\":{";
  body += "\"type\":" + q(node->type_name());
  body += ",\"uid\":" + q(uid_text(node->get_uid()));
  body += ",\"name\":" + q(node->name());
  body += ",\"label\":" + q(node->label());
  body += ",\"callback\":" + q(node->callback());
  body += ",\"user_data\":" + q(node->user_data());
  body += ",\"user_data_type\":" + q(node->user_data_type());
  body += ",\"comment\":" + q(node->comment());
  if (node->parent) body += ",\"parent\":" + q(id_of(node->parent));
  auto *widget = dynamic_cast<Widget_Node *>(node);
  if (widget) {
    body += widget_fields(widget);
    for (int i = 0; i < NUM_EXTRA_CODE; i++)
      body += ",\"extra" + std::to_string(i) + "\":" + q(widget->extra_code(i));
  }
  body += "}}";
  return succeed(body);
}

Outcome project_info() {
  std::string body = "{\"ok\":true";
  body += ",\"project\":" + q(Fluid.proj.proj_filename);
  body += ",\"code\":" + q(Fluid.proj.codefile_path() + Fluid.proj.codefile_name());
  body += ",\"header\":" + q(Fluid.proj.headerfile_path() + Fluid.proj.headerfile_name());
  body += ",\"strings\":" + q(Fluid.proj.stringsfile_path() + Fluid.proj.stringsfile_name());
  body += ",\"nodes\":[";
  bool first = true;
  for (Node *node = Fluid.proj.tree.first; node; node = node->next) {
    if (!dynamic_cast<Function_Node *>(node) && !dynamic_cast<Class_Node *>(node)) continue;
    if (!first) body += ",";
    first = false;
    body += "{\"type\":" + q(node->type_name());
    body += ",\"name\":" + q(node->name());
    body += ",\"uid\":" + q(uid_text(node->get_uid())) + "}";
  }
  body += "]}";
  return succeed(body);
}

Outcome validate_project() {
  std::map<std::string, int> names;
  std::string problems = "[";
  bool first = true;
  auto add = [&](const std::string &text) {
    if (!first) problems += ",";
    first = false;
    problems += q(text);
  };
  for (Node *node = Fluid.proj.tree.first; node; node = node->next) {
    if (node->level > 0 && !node->parent)
      add("Node " + id_of(node) + " has no parent.");
    if (!node->name().empty()) names[node->name()]++;
  }
  for (const auto &item : names) {
    if (item.second > 1) add("Name \"" + item.first + "\" is used " + std::to_string(item.second) + " times.");
  }
  problems += "]";
  return succeed("{\"ok\":true,\"problems\":" + problems + "}");
}

Outcome set_properties(const Request &request) {
  std::string error;
  Node *node = find_node(request.widget, error);
  if (!node) return fail(error);
  if (!apply_props(node, request.props, error)) return fail(error);
  if (!save_project(error)) return fail(error);
  return succeed("{\"ok\":true,\"id\":" + q(id_of(node)) + "}");
}

Outcome create_widget(const Request &request) {
  std::string error;
  Node *parent = find_node(request.parent, error);
  if (!parent) return fail(error);
  if (!dynamic_cast<Group_Node *>(parent)) return fail("The parent is not a window or group.");
  if (!typename_to_prototype(request.type)) return fail("Unknown widget type \"" + request.type + "\".");
  Fluid.proj.tree.current = parent;
  Node *created = add_new_widget_from_user(request.type, Strategy::AS_LAST_CHILD, false);
  if (!created) return fail("Couldn't create the widget.");
  if (!request.name.empty()) created->name(request.name);
  if (!request.label.empty()) created->label(request.label);
  if (!apply_props(created, request.props, error)) return fail(error);
  if (!save_project(error)) return fail(error);
  return succeed("{\"ok\":true,\"id\":" + q(id_of(created)) + ",\"uid\":" + q(uid_text(created->get_uid())) + "}");
}

Outcome create_window(const Request &request) {
  Node *anchor = nullptr;
  for (Node *node = Fluid.proj.tree.first; node; node = node->next)
    if (!node->parent) anchor = node;
  Fluid.proj.tree.current = anchor;
  Strategy where = anchor ? Strategy::AFTER_CURRENT : Strategy::AS_LAST_CHILD;
  Node *function = add_new_widget_from_user("function", where, false);
  if (!function) return fail("Couldn't create a function for the window.");
  if (!request.name.empty()) function->name(request.name + "()");
  Fluid.proj.tree.current = function;
  std::string kind = request.type.empty() ? "Fl_Window" : request.type;
  Node *created = add_new_widget_from_user(kind, Strategy::AS_LAST_CHILD, false);
  if (!created) return fail("Couldn't create the window.");
  if (!request.name.empty()) created->name(request.name);
  if (!request.label.empty()) created->label(request.label);
  std::string error;
  if (!apply_props(created, request.props, error)) return fail(error);
  if (!save_project(error)) return fail(error);
  return succeed("{\"ok\":true,\"id\":" + q(id_of(created)) + ",\"uid\":" + q(uid_text(created->get_uid())) + "}");
}

Outcome delete_widget(const Request &request) {
  std::string error;
  Node *node = find_node(request.widget, error);
  if (!node) return fail(error);
  node->remove();
  std::vector<Node *> doomed;
  for (Node *item = node; item; item = item->next) doomed.push_back(item);
  while (!doomed.empty()) {
    delete doomed.back();
    doomed.pop_back();
  }
  if (!save_project(error)) return fail(error);
  return succeed("{\"ok\":true}");
}

Outcome set_code(const Request &request) {
  std::string error;
  Node *node = find_node(request.widget, error);
  if (!node) return fail(error);
  std::map<std::string, std::string> props;
  std::string field = request.field.empty() ? "callback" : request.field;
  props[field] = request.code;
  if (!apply_props(node, props, error)) return fail(error);
  if (!save_project(error)) return fail(error);
  return succeed("{\"ok\":true,\"id\":" + q(id_of(node)) + "}");
}

Outcome move_widget(const Request &request) {
  std::string error;
  Node *node = find_node(request.widget, error);
  if (!node) return fail(error);
  if (!request.parent.empty()) {
    Node *parent = find_node(request.parent, error);
    if (!parent) return fail(error);
    if (!dynamic_cast<Group_Node *>(parent)) return fail("The parent is not a window or group.");
    if (parent == node) return fail("A node cannot contain itself.");
    node->remove();
    Fluid.proj.tree.current = parent;
    node->add(parent, Strategy::AS_LAST_CHILD);
  }
  if (!apply_props(node, request.props, error)) return fail(error);
  if (!save_project(error)) return fail(error);
  return succeed("{\"ok\":true,\"id\":" + q(id_of(node)) + "}");
}

std::string capture_png(Window_Node *window, const std::string &keep_path, Outcome &out) {
  std::string path = keep_path.empty() ? temp_png_path() : keep_path;
  if (!write_preview_png(window, path)) return std::string();
  std::string bytes = read_binary(path);
  if (keep_path.empty()) fl_unlink(path.c_str());
  if (bytes.empty()) return std::string();
  out.png.push_back(bytes);
  return path;
}

Outcome snapshot_window(const Request &request) {
  std::string error;
  Window_Node *window = find_window(request.window, error);
  if (!window) return fail(error);
  Outcome out = succeed("{\"ok\":true,\"window\":" + q(id_of(window)) + "}");
  if (capture_png(window, request.image_out, out).empty() && out.png.empty())
    return fail("Couldn't capture the window.");
  return out;
}

Outcome snapshot_all(const Request &request) {
  Outcome out;
  out.ok = true;
  std::string body = "{\"ok\":true,\"windows\":[";
  bool first = true;
  for (Node *node = Fluid.proj.tree.first; node; node = node->next) {
    auto *window = dynamic_cast<Window_Node *>(node);
    if (!window || dynamic_cast<Widget_Class_Node *>(window)) continue;
    std::string kept;
    if (!request.image_out.empty()) {
      std::string safe = window->name().empty() ? uid_text(window->get_uid()) : window->name();
      kept = end_with_slash(fix_separators(request.image_out)) + safe + ".png";
    }
    Outcome one;
    if (capture_png(window, kept, one).empty() && one.png.empty())
      return fail("Couldn't capture window \"" + id_of(window) + "\".");
    out.png.insert(out.png.end(), one.png.begin(), one.png.end());
    if (!first) body += ",";
    first = false;
    body += q(id_of(window));
  }
  body += "]}";
  out.json = body;
  if (out.png.empty()) return fail("This project has no window to preview.");
  return out;
}

Outcome generate_code() {
  std::string code = Fluid.proj.codefile_path() + Fluid.proj.codefile_name();
  std::string header = Fluid.proj.headerfile_path() + Fluid.proj.headerfile_name();
  std::string strings = Fluid.proj.stringsfile_path() + Fluid.proj.stringsfile_name();
  fluid::io::Code_Writer writer(Fluid.proj);
  if (!writer.write_code(code, header))
    return fail(std::string("Can't write code: ") + strerror(errno));
  int strings_error = fluid::io::write_strings(Fluid.proj, strings);
  std::string body = "{\"ok\":true,\"code\":" + q(code);
  body += ",\"header\":" + q(header);
  body += ",\"strings\":" + q(strings);
  if (strings_error) body += ",\"strings_error\":" + q(strerror(errno));
  body += "}";
  return succeed(body);
}

Outcome run_command(const std::string &command, const Request &request) {
  if (command != "validate_project" && command != "list_windows" && command != "project_info"
      && command != "describe_window" && command != "read_widget" && command != "snapshot_window"
      && command != "snapshot_all" && command != "generate_code" && command != "set_properties"
      && command != "create_widget" && command != "delete_widget" && command != "create_window"
      && command != "set_code" && command != "move_widget")
    return fail("Unknown command \"" + command + "\".");
  std::string error;
  if (!load_project(request.project, error)) return fail(error);
  if (command == "list_windows") return list_windows();
  if (command == "describe_window") return describe_window(request);
  if (command == "read_widget") return read_widget(request);
  if (command == "project_info") return project_info();
  if (command == "validate_project") return validate_project();
  if (command == "set_properties") return set_properties(request);
  if (command == "create_widget") return create_widget(request);
  if (command == "delete_widget") return delete_widget(request);
  if (command == "create_window") return create_window(request);
  if (command == "set_code") return set_code(request);
  if (command == "move_widget") return move_widget(request);
  if (command == "snapshot_window") return snapshot_window(request);
  if (command == "snapshot_all") return snapshot_all(request);
  return generate_code();
}

std::string cli_json(const Outcome &out) {
  if (out.png.empty()) return out.json;
  std::string body = out.json;
  if (!body.empty() && body.back() == '}') body.pop_back();
  if (!body.empty() && body.back() != '{') body.push_back(',');
  body += "\"png_base64\":[";
  for (size_t i = 0; i < out.png.size(); i++) {
    if (i) body.push_back(',');
    body += q(base64_encode(out.png[i]));
  }
  body += "]}";
  return body;
}

bool take_option(int argc, char **argv, int &i, const char *name, std::string &value) {
  if (strcmp(argv[i], name) != 0) return false;
  if (i + 1 >= argc) return false;
  value = argv[++i];
  return true;
}

Request request_from_cli(int argc, char **argv, int begin) {
  Request request;
  for (int i = begin; i < argc; i++) {
    if (take_option(argc, argv, i, "--project", request.project)) continue;
    if (take_option(argc, argv, i, "--window", request.window)) continue;
    if (take_option(argc, argv, i, "--widget", request.widget)) continue;
    if (take_option(argc, argv, i, "--type", request.type)) continue;
    if (take_option(argc, argv, i, "--parent", request.parent)) continue;
    if (take_option(argc, argv, i, "--label", request.label)) continue;
    if (take_option(argc, argv, i, "--name", request.name)) continue;
    if (take_option(argc, argv, i, "--field", request.field)) continue;
    if (take_option(argc, argv, i, "--code", request.code)) continue;
    if (take_option(argc, argv, i, "--image-out", request.image_out)) continue;
    if (strcmp(argv[i], "--set") == 0 && i + 1 < argc) {
      std::string item = argv[++i];
      size_t eq = item.find('=');
      if (eq == std::string::npos) request.props[item] = "true";
      else request.props[item.substr(0, eq)] = item.substr(eq + 1);
      continue;
    }
    if (strcmp(argv[i], "--props") == 0 && i + 1 < argc) {
      bool ok = false;
      J parsed = parse_json(argv[++i], ok);
      if (ok) request.props = props_from_json(&parsed);
    }
  }
  return request;
}

const char *kInstructions =
  "Read and edit a FLUID .fl project, then preview its windows. "
  "Pass the .fl path on every call. Identify a widget by its unique name, or by its 4-digit uid when the name is empty or duplicated. "
  "snapshot_window and snapshot_all return the live preview image. Editing generated .cxx or .h files does not change that preview. "
  "Each call reloads the .fl from disk.";

struct Tool {
  const char *name;
  const char *description;
  const char *schema;
};

const Tool kTools[] = {
  {"list_windows", "List the windows in a .fl project. Each item has name, label, uid, and id.",
   "{\"type\":\"object\",\"properties\":{\"project\":{\"type\":\"string\",\"description\":\"Path to the .fl file.\"}},\"required\":[\"project\"]}"},
  {"describe_window", "Return one window and its child widget tree: type, name, label, parent, position, and size.",
   "{\"type\":\"object\",\"properties\":{\"project\":{\"type\":\"string\"},\"window\":{\"type\":\"string\",\"description\":\"Window name, label, or uid.\"}},\"required\":[\"project\",\"window\"]}"},
  {"read_widget", "Return the full properties of one widget or code node.",
   "{\"type\":\"object\",\"properties\":{\"project\":{\"type\":\"string\"},\"widget\":{\"type\":\"string\",\"description\":\"Unique name, or uid when the name is empty or duplicated.\"}},\"required\":[\"project\",\"widget\"]}"},
  {"project_info", "Return the generated source, header, and strings paths, plus class and function nodes.",
   "{\"type\":\"object\",\"properties\":{\"project\":{\"type\":\"string\"}},\"required\":[\"project\"]}"},
  {"validate_project", "Open the .fl file and report duplicate names and nodes that have no parent.",
   "{\"type\":\"object\",\"properties\":{\"project\":{\"type\":\"string\"}},\"required\":[\"project\"]}"},
  {"set_properties", "Change properties of one widget and save the .fl file. Keys: name, label, callback, user_data, user_data_type, comment, tooltip, x, y, w, h, visible, active, resizable, extra0, extra1, extra2, extra3.",
   "{\"type\":\"object\",\"properties\":{\"project\":{\"type\":\"string\"},\"widget\":{\"type\":\"string\"},\"properties\":{\"type\":\"object\"}},\"required\":[\"project\",\"widget\",\"properties\"]}"},
  {"create_widget", "Create a widget inside a window or group and save the .fl file. type is an FLTK type such as Fl_Button.",
   "{\"type\":\"object\",\"properties\":{\"project\":{\"type\":\"string\"},\"parent\":{\"type\":\"string\"},\"type\":{\"type\":\"string\"},\"name\":{\"type\":\"string\"},\"label\":{\"type\":\"string\"},\"properties\":{\"type\":\"object\"}},\"required\":[\"project\",\"parent\",\"type\"]}"},
  {"delete_widget", "Delete a widget and its children, then save the .fl file.",
   "{\"type\":\"object\",\"properties\":{\"project\":{\"type\":\"string\"},\"widget\":{\"type\":\"string\"}},\"required\":[\"project\",\"widget\"]}"},
  {"create_window", "Create a function and a window, then save the .fl file. type defaults to Fl_Window.",
   "{\"type\":\"object\",\"properties\":{\"project\":{\"type\":\"string\"},\"name\":{\"type\":\"string\"},\"label\":{\"type\":\"string\"},\"type\":{\"type\":\"string\"},\"properties\":{\"type\":\"object\"}},\"required\":[\"project\"]}"},
  {"set_code", "Set callback, comment, user_data, or extra0 through extra3 on one node and save the .fl file.",
   "{\"type\":\"object\",\"properties\":{\"project\":{\"type\":\"string\"},\"widget\":{\"type\":\"string\"},\"field\":{\"type\":\"string\"},\"code\":{\"type\":\"string\"}},\"required\":[\"project\",\"widget\",\"code\"]}"},
  {"move_widget", "Move a widget to x, y, w, h and optionally into another parent, then save the .fl file.",
   "{\"type\":\"object\",\"properties\":{\"project\":{\"type\":\"string\"},\"widget\":{\"type\":\"string\"},\"parent\":{\"type\":\"string\"},\"properties\":{\"type\":\"object\"}},\"required\":[\"project\",\"widget\"]}"},
  {"snapshot_window", "Preview one window with real widgets and return a PNG. The image is not kept as a file.",
   "{\"type\":\"object\",\"properties\":{\"project\":{\"type\":\"string\"},\"window\":{\"type\":\"string\"}},\"required\":[\"project\",\"window\"]}"},
  {"snapshot_all", "Preview every window and return one PNG per window. Images are not kept as files.",
   "{\"type\":\"object\",\"properties\":{\"project\":{\"type\":\"string\"}},\"required\":[\"project\"]}"},
  {"generate_code", "Write the project's .cxx, .h, and strings files. This does not change the preview; the preview reads the .fl file.",
   "{\"type\":\"object\",\"properties\":{\"project\":{\"type\":\"string\"}},\"required\":[\"project\"]}"}
};

Request request_from_args(const J &args) {
  Request request;
  auto text = [&](const char *key) {
    const J *value = args.find(key);
    return value ? as_text(*value) : std::string();
  };
  request.project = text("project");
  request.window = text("window");
  request.widget = text("widget");
  request.type = text("type");
  request.parent = text("parent");
  request.label = text("label");
  request.name = text("name");
  request.field = text("field");
  request.code = text("code");
  request.props = props_from_json(args.find("properties"));
  return request;
}

std::string tool_list_json() {
  std::string out = "[";
  for (size_t i = 0; i < sizeof(kTools) / sizeof(kTools[0]); i++) {
    if (i) out += ",";
    out += "{\"name\":" + q(kTools[i].name);
    out += ",\"description\":" + q(std::string(kTools[i].description) + " " + kInstructions);
    out += ",\"inputSchema\":" + std::string(kTools[i].schema) + "}";
  }
  out += "]";
  return out;
}

std::string mcp_result(const std::string &id, const Outcome &out) {
  std::string content = "[{\"type\":\"text\",\"text\":" + q(out.json) + "}";
  for (const std::string &png : out.png) {
    content += ",{\"type\":\"image\",\"mimeType\":\"image/png\",\"data\":" + q(base64_encode(png)) + "}";
  }
  content += "]";
  std::string body = "{\"content\":" + content;
  if (!out.ok) body += ",\"isError\":true";
  body += "}";
  return "{\"jsonrpc\":\"2.0\",\"id\":" + id + ",\"result\":" + body + "}";
}

std::string read_mcp_message() {
  std::string header;
  char chunk[1];
  while (!(header.size() >= 4 && (header.substr(header.size() - 4) == "\r\n\r\n" || header.substr(header.size() - 2) == "\n\n"))) {
    if (fread(chunk, 1, 1, stdin) != 1) return std::string();
    header.push_back(chunk[0]);
    if (header.size() > 65536) return std::string();
  }
  size_t length = 0;
  const char *mark = strstr(header.c_str(), "Content-Length:");
  if (!mark) return std::string();
  length = (size_t)atoi(mark + 15);
  std::string body(length, '\0');
  size_t got = 0;
  while (got < length) {
    size_t n = fread(&body[got], 1, length - got, stdin);
    if (n == 0) return std::string();
    got += n;
  }
  return body;
}

void write_mcp_message(const std::string &body) {
  fprintf(stdout, "Content-Length: %u\r\n\r\n", (unsigned)body.size());
  fwrite(body.data(), 1, body.size(), stdout);
  fflush(stdout);
}

void bind_stdio() {
#ifdef _WIN32
  if (_fileno(stdin) >= 0) _setmode(_fileno(stdin), _O_BINARY);
  if (_fileno(stdout) >= 0) _setmode(_fileno(stdout), _O_BINARY);
  if (_fileno(stderr) >= 0) _setmode(_fileno(stderr), _O_BINARY);
  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stdin, nullptr, _IONBF, 0);
  if (_fileno(stdin) >= 0 && _fileno(stdout) >= 0) return;
  HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
  HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
  auto bind = [](HANDLE handle, int target, int mode) {
    if (!handle || handle == INVALID_HANDLE_VALUE) return;
    DWORD kind = GetFileType(handle);
    if (kind != FILE_TYPE_PIPE && kind != FILE_TYPE_DISK) return;
    int fd = _open_osfhandle(reinterpret_cast<intptr_t>(handle), mode);
    if (fd < 0) return;
    _dup2(fd, target);
    _setmode(target, _O_BINARY);
  };
  bind(input, 0, _O_RDONLY | _O_BINARY);
  bind(output, 1, _O_WRONLY | _O_BINARY);
  bind(GetStdHandle(STD_ERROR_HANDLE), 2, _O_WRONLY | _O_BINARY);
  if (_fileno(stdout) < 0 && AttachConsole(ATTACH_PARENT_PROCESS)) {
    FILE *ignored = nullptr;
    freopen_s(&ignored, "CONOUT$", "w", stdout);
    freopen_s(&ignored, "CONOUT$", "w", stderr);
    freopen_s(&ignored, "CONIN$", "r", stdin);
  }
  setvbuf(stdin, nullptr, _IONBF, 0);
  setvbuf(stdout, nullptr, _IONBF, 0);
#endif
}

}  // namespace

int agent_cli_main(int argc, char **argv) {
  bind_stdio();
  if (argc < 3) {
    fprintf(stderr, "Usage: fluid-plus --agent <command> --project file.fl [options]\n");
    return 1;
  }
  Request request = request_from_cli(argc, argv, 3);
  Outcome out = run_command(argv[2], request);
  std::string json = cli_json(out);
  fwrite(json.data(), 1, json.size(), stdout);
  fputc('\n', stdout);
  fflush(stdout);
  return out.ok ? 0 : 1;
}

int agent_mcp_main() {
  bind_stdio();
  for (;;) {
    std::string message = read_mcp_message();
    if (message.empty()) return 0;
    bool ok = false;
    J root = parse_json(message, ok);
    if (!ok || root.kind != J::Object) continue;
    const J *method = root.find("method");
    if (!method || method->kind != J::String) continue;
    const J *id = root.find("id");
    std::string id_json;
    bool has_id = id && (id->kind == J::Number || id->kind == J::String);
    if (has_id) id_json = id->kind == J::String ? q(id->s) : std::to_string((int)id->n);
    const J *params = root.find("params");
    if (method->s == "initialize" && has_id) {
      std::string version = "2024-11-05";
      if (params) {
        const J *asked = params->find("protocolVersion");
        if (asked && asked->kind == J::String && asked->s.size() > 4) version = asked->s;
      }
      write_mcp_message(
        "{\"jsonrpc\":\"2.0\",\"id\":" + id_json +
        ",\"result\":{\"protocolVersion\":" + q(version) +
        ",\"capabilities\":{\"tools\":{}},\"serverInfo\":{\"name\":\"fluid-plus\",\"version\":\"1.5.0\"},\"instructions\":" +
        q(kInstructions) + "}}");
    } else if (method->s == "tools/list" && has_id) {
      write_mcp_message("{\"jsonrpc\":\"2.0\",\"id\":" + id_json + ",\"result\":{\"tools\":" + tool_list_json() + "}}");
    } else if (method->s == "tools/call" && has_id && params) {
      const J *name = params->find("name");
      const J *args = params->find("arguments");
      if (!name || name->kind != J::String || !args) {
        write_mcp_message(mcp_result(id_json, fail("Missing tool name or arguments.")));
      } else {
        write_mcp_message(mcp_result(id_json, run_command(name->s, request_from_args(*args))));
      }
    } else if (method->s == "ping" && has_id) {
      write_mcp_message("{\"jsonrpc\":\"2.0\",\"id\":" + id_json + ",\"result\":{}}");
    } else if (has_id) {
      write_mcp_message("{\"jsonrpc\":\"2.0\",\"id\":" + id_json + ",\"error\":{\"code\":-32601,\"message\":\"Method not found\"}}");
    }
  }
}

void show_mcp_setup_help() {
  std::string path = executable_path();
  for (char &c : path) if (c == '\\') c = '/';
  std::string config = "{\n  \"command\": " + q(path) + ",\n  \"args\": [\"--mcp\"]\n}\n";
  static std::string copied;
  copied = config;

  // Keep one window. A nested Fl::wait() here returns into Fl::run(), and closing
  // the dialog then looks like the last window closed, so the program exits.
  static Fl_Double_Window *window = nullptr;
  static Fl_Text_Buffer *buffer = nullptr;
  if (!window) {
    Fl_Group *saved = Fl_Group::current();
    Fl_Group::current(nullptr);
    window = new Fl_Double_Window(640, 420, "MCP Setup Help");
    window->callback([](Fl_Widget *widget, void *) { widget->hide(); });
    Fl_Box *text = new Fl_Box(16, 12, 608, 72,
      "Add the configuration below to your AI client's MCP settings. "
      "The AI can then open a project, edit the interface, and preview windows.");
    text->align(FL_ALIGN_TOP | FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
    text->box(FL_FLAT_BOX);
    text->labelsize(14);

    buffer = new Fl_Text_Buffer();
    Fl_Text_Display *display = new Fl_Text_Display(16, 92, 608, 250);
    display->buffer(buffer);
    display->textsize(14);

    Fl_Button *copy = new Fl_Button(16, 368, 120, 32, "Copy");
    copy->callback([](Fl_Widget *, void *data) {
      const std::string *text = static_cast<const std::string *>(data);
      Fl::copy(text->c_str(), (int)text->size(), 1);
    }, (void *)&copied);
    Fl_Button *close = new Fl_Button(504, 368, 120, 32, "Close");
    close->callback([](Fl_Widget *button, void *) { button->window()->hide(); });
    window->end();
    localize_window(window);
    Fl_Group::current(saved);
  }
  buffer->text(config.c_str());
  if (Fl::grab()) Fl::grab(nullptr);
  window->show();
}

}  // namespace fluid
