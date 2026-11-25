//
// FLUID interface language. English is written in the UI. Other languages live in JSON language packs in the locale directory.
//
// Added 2026-11-22: translate FLUID's own menus and settings window from external JSON language packs.
// This file belongs to fltk-plus and is released with FLTK under the LGPL.
//

#include "app/ui_locale.h"

#include "Fluid.h"
#include "app/code_console.h"
#include "app/code_stage.h"
#include "app/designer_shell.h"
#include "app/property_sheet.h"
#include "panels/settings_panel.h"

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Browser_.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Input_.H>
#include <FL/Fl_Menu_.H>
#include <FL/Fl_Menu_Item.H>
#include <FL/Fl_Multi_Label.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Group.H>
#include <FL/fl_utf8.h>
#include <FL/fl_ask.H>
#include <FL/filename.H>
#include <FL/Fl_File_Chooser.H>
#include <config.h>

#include <algorithm>
#include <deque>
#include <map>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#  include <windows.h>
#elif defined(__APPLE__)
#  include <mach-o/dyld.h>
#  include <limits.h>
#  include <stdlib.h>
#  include <CoreFoundation/CoreFoundation.h>
#  include <CoreText/CoreText.h>
#else
#  include <unistd.h>
#  include <limits.h>
#  include <dlfcn.h>
#endif

namespace fluid {

namespace {

struct Pack {
  std::string id;
  std::string name;
  std::map<std::string, std::string> text;
};

struct Json {
  const char *cursor;
  const char *end;
};

void skip_space(Json &json) {
  while (json.cursor < json.end) {
    char c = *json.cursor;
    if (c != ' ' && c != '\t' && c != '\n' && c != '\r') break;
    json.cursor++;
  }
}

bool parse_string(Json &json, std::string &out) {
  skip_space(json);
  if (json.cursor >= json.end || *json.cursor != '"') return false;
  json.cursor++;
  out.clear();
  while (json.cursor < json.end && *json.cursor != '"') {
    if (*json.cursor != '\\') {
      out.push_back(*json.cursor++);
      continue;
    }
    json.cursor++;
    if (json.cursor >= json.end) return false;
    char escaped = *json.cursor++;
    if (escaped == 'n') out.push_back('\n');
    else if (escaped == 'r') out.push_back('\r');
    else if (escaped == 't') out.push_back('\t');
    else if (escaped == '"' || escaped == '\\' || escaped == '/') out.push_back(escaped);
    else if (escaped == 'u' && json.cursor + 4 <= json.end) {
      unsigned code = 0;
      for (int i = 0; i < 4; i++) {
        char hex = *json.cursor++;
        code <<= 4;
        if (hex >= '0' && hex <= '9') code += hex - '0';
        else if (hex >= 'a' && hex <= 'f') code += hex - 'a' + 10;
        else if (hex >= 'A' && hex <= 'F') code += hex - 'A' + 10;
        else return false;
      }
      if (code < 0x80) out.push_back((char)code);
      else if (code < 0x800) {
        out.push_back((char)(0xC0 | (code >> 6)));
        out.push_back((char)(0x80 | (code & 0x3F)));
      } else {
        out.push_back((char)(0xE0 | (code >> 12)));
        out.push_back((char)(0x80 | ((code >> 6) & 0x3F)));
        out.push_back((char)(0x80 | (code & 0x3F)));
      }
    } else return false;
  }
  if (json.cursor >= json.end || *json.cursor != '"') return false;
  json.cursor++;
  return true;
}

bool parse_translations(Json &json, std::map<std::string, std::string> &text) {
  skip_space(json);
  if (json.cursor >= json.end || *json.cursor != '{') return false;
  json.cursor++;
  while (true) {
    skip_space(json);
    if (json.cursor < json.end && *json.cursor == '}') {
      json.cursor++;
      return true;
    }
    std::string key;
    std::string value;
    if (!parse_string(json, key)) return false;
    skip_space(json);
    if (json.cursor >= json.end || *json.cursor != ':') return false;
    json.cursor++;
    if (!parse_string(json, value)) return false;
    text[key] = value;
    skip_space(json);
    if (json.cursor < json.end && *json.cursor == ',') json.cursor++;
  }
}

bool parse_pack(const std::string &bytes, Pack &pack) {
  Json json { bytes.data(), bytes.data() + bytes.size() };
  skip_space(json);
  if (json.cursor >= json.end || *json.cursor != '{') return false;
  json.cursor++;
  while (true) {
    skip_space(json);
    if (json.cursor < json.end && *json.cursor == '}') return !pack.id.empty() && !pack.name.empty();
    std::string key;
    if (!parse_string(json, key)) return false;
    skip_space(json);
    if (json.cursor >= json.end || *json.cursor != ':') return false;
    json.cursor++;
    skip_space(json);
    if (json.cursor < json.end && *json.cursor == '"') {
      std::string value;
      if (!parse_string(json, value)) return false;
      if (key == "id") pack.id = value;
      else if (key == "name") pack.name = value;
    } else if (key == "translations") {
      if (!parse_translations(json, pack.text)) return false;
    } else return false;
    skip_space(json);
    if (json.cursor < json.end && *json.cursor == ',') json.cursor++;
  }
}

std::string join_path(const std::string &dir, const char *name) {
  if (dir.empty()) return name;
  char last = dir[dir.size() - 1];
  if (last == '/' || last == '\\') return dir + name;
  return dir + "/" + name;
}

std::string executable_dir() {
#if defined(_WIN32)
  wchar_t wide[32768];
  DWORD length = GetModuleFileNameW(nullptr, wide, 32768);
  if (!length) return std::string();
  char utf8[65536];
  int bytes = fl_utf8fromwc(utf8, sizeof(utf8), wide, (unsigned)length);
  if (bytes <= 0) return std::string();
  std::string path(utf8, (size_t)bytes);
#elif defined(__APPLE__)
  char buffer[PATH_MAX];
  uint32_t size = sizeof(buffer);
  if (_NSGetExecutablePath(buffer, &size) != 0) return std::string();
  char resolved[PATH_MAX];
  if (!realpath(buffer, resolved)) return std::string();
  std::string path(resolved);
#else
  char buffer[PATH_MAX];
  ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
  if (length <= 0) return std::string();
  buffer[length] = 0;
  std::string path(buffer);
#endif
  size_t slash = path.find_last_of("\\/");
  if (slash == std::string::npos) return std::string();
  return path.substr(0, slash);
}

std::string read_file(const std::string &path) {
  FILE *file = fl_fopen(path.c_str(), "rb");
  if (!file) return std::string();
  std::string bytes;
  char buffer[4096];
  size_t count = 0;
  while ((count = std::fread(buffer, 1, sizeof(buffer), file)) > 0) bytes.append(buffer, count);
  std::fclose(file);
  if (bytes.size() >= 3 &&
      (unsigned char)bytes[0] == 0xEF &&
      (unsigned char)bytes[1] == 0xBB &&
      (unsigned char)bytes[2] == 0xBF) {
    bytes.erase(0, 3);
  }
  return bytes;
}

void load_directory(const std::string &directory, std::vector<Pack> &packs) {
  if (directory.empty()) return;
  dirent **list = nullptr;
  int count = fl_filename_list(directory.c_str(), &list, fl_casenumericsort);
  if (count < 0) return;
  for (int i = 0; i < count; i++) {
    const char *name = list[i]->d_name;
    size_t length = std::strlen(name);
    if (length < 6 || fl_utf_strncasecmp(name + length - 5, ".json", 5) != 0) continue;
    std::string bytes = read_file(join_path(directory, name));
    if (bytes.empty()) continue;
    Pack pack;
    if (parse_pack(bytes, pack)) {
      bool duplicate = false;
      for (const Pack &existing : packs) {
        if (existing.id == pack.id) duplicate = true;
      }
      if (!duplicate) packs.push_back(std::move(pack));
    }
  }
  fl_filename_free_list(&list, count);
}

std::vector<Pack> g_packs;
std::vector<Fl_Menu_Item> g_language_items;

void load_packs() {
  g_packs.clear();
  Pack english;
  english.id = "en";
  english.name = "English";
  g_packs.push_back(english);

  std::vector<std::string> directories;
  if (const char *override_dir = std::getenv("FLUID_LOCALE_DIR")) {
    if (override_dir[0]) directories.push_back(override_dir);
  }
  std::string exe_dir = executable_dir();
  if (!exe_dir.empty()) {
    directories.push_back(join_path(exe_dir, "locale"));
    directories.push_back(join_path(exe_dir, "../share/fltk-plus/locale"));
  }
  for (const std::string &directory : directories) load_directory(directory, g_packs);
}

int g_language = 0;
bool g_captured = false;
Fl_Choice *g_language_choice = nullptr;
struct MenuHold {
  std::string english;
  bool icon;
  std::string shown;
};

std::map<Fl_Menu_Item *, MenuHold> g_menu_hold;
// Keep our own copy of the source text. Fl_Widget::label() frees the copy_label buffer.
// Translating through that pointer draws freed memory on the next refresh.
std::map<Fl_Widget *, std::string> g_widget_english;
std::map<Fl_Widget *, std::string> g_widget_tooltip;
std::vector<Fl_Widget *> g_windows;

const char *translate(const char *english) {
  if (!english || g_language <= 0 || g_language >= (int)g_packs.size()) return english;
  const auto &text = g_packs[g_language].text;
  auto found = text.find(english);
  if (found == text.end()) return english;
  return found->second.c_str();
}

bool contains_ci(const char *text, const char *part) {
  if (!text || !part || !part[0]) return false;
  const size_t part_len = std::strlen(part);
  for (const char *cursor = text; *cursor; cursor++) {
    if (fl_utf_strncasecmp(cursor, part, (int)part_len) == 0) return true;
  }
  return false;
}

// Interface font size. Widgets are usually created at 12, and later settings scale from this value.
int g_ui_size = 12;

std::string saved_ui_family() {
  std::string name;
  Fluid.preferences.get("ui_font", name, std::string(""));
  return name;
}

int saved_ui_size() {
  int size = 0;
  Fluid.preferences.get("ui_font_size", size, 0);
  return size;
}

std::string family_key(const char *name, int attr) {
  std::string text = name ? name : "";
  if ((attr & FL_ITALIC) && text.size() >= 7 && text.compare(text.size() - 7, 7, " italic") == 0)
    text.erase(text.size() - 7);
  if ((attr & FL_BOLD) && text.size() >= 5 && text.compare(text.size() - 5, 5, " bold") == 0)
    text.erase(text.size() - 5);
  return text;
}

unsigned read_u16(const std::string &bytes, size_t offset, unsigned &out) {
  if (offset + 2 > bytes.size()) return 0;
  const unsigned char *p = reinterpret_cast<const unsigned char *>(bytes.data()) + offset;
  out = ((unsigned)p[0] << 8) | p[1];
  return 1;
}

unsigned read_u32(const std::string &bytes, size_t offset, unsigned &out) {
  if (offset + 4 > bytes.size()) return 0;
  const unsigned char *p = reinterpret_cast<const unsigned char *>(bytes.data()) + offset;
  out = ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) | ((unsigned)p[2] << 8) | p[3];
  return 1;
}

void append_utf8(std::string &out, unsigned code) {
  if (code < 0x80) {
    out.push_back((char)code);
  } else if (code < 0x800) {
    out.push_back((char)(0xC0 | (code >> 6)));
    out.push_back((char)(0x80 | (code & 0x3F)));
  } else if (code < 0x10000) {
    out.push_back((char)(0xE0 | (code >> 12)));
    out.push_back((char)(0x80 | ((code >> 6) & 0x3F)));
    out.push_back((char)(0x80 | (code & 0x3F)));
  } else if (code < 0x110000) {
    out.push_back((char)(0xF0 | (code >> 18)));
    out.push_back((char)(0x80 | ((code >> 12) & 0x3F)));
    out.push_back((char)(0x80 | ((code >> 6) & 0x3F)));
    out.push_back((char)(0x80 | (code & 0x3F)));
  }
}

std::string utf16be_to_utf8(const unsigned char *data, unsigned nbytes) {
  std::string out;
  unsigned i = 0;
  while (i + 1 < nbytes) {
    unsigned unit = ((unsigned)data[i] << 8) | data[i + 1];
    i += 2;
    if (unit >= 0xD800 && unit <= 0xDBFF && i + 1 < nbytes) {
      unsigned low = ((unsigned)data[i] << 8) | data[i + 1];
      if (low >= 0xDC00 && low <= 0xDFFF) {
        i += 2;
        append_utf8(out, 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00));
        continue;
      }
    }
    if (unit == 0 || (unit >= 0xDC00 && unit <= 0xDFFF)) continue;
    append_utf8(out, unit);
  }
  return out;
}

std::string tidy_family(std::string text) {
  while (!text.empty() && (text.back() == '\0' || text.back() == ' ')) text.pop_back();
  size_t start = 0;
  while (start < text.size() && text[start] == ' ') start++;
  if (start) text.erase(0, start);
  if (text.size() > 200) return std::string();
  return text;
}

int name_rank(unsigned platform, unsigned encoding, unsigned language, unsigned name_id) {
  if (name_id != 16 && name_id != 1) return -1;
  int rank = name_id == 16 ? 100 : 0;
  if (platform == 3 && (encoding == 1 || encoding == 10)) rank += 40;
  else if (platform == 0 && (encoding == 1 || encoding == 3)) rank += 30;
  else if (platform == 1 && encoding == 0) rank += 10;
  else return -1;
  if (language == 0x409 || language == 0) rank += 5;
  return rank;
}

struct Name_Pick {
  int rank;
  std::string text;
};

void keep_name(Name_Pick &best, int rank, const std::string &text) {
  if (rank < 0 || text.empty() || rank < best.rank) return;
  best.rank = rank;
  best.text = text;
}

void parse_name_table(const std::string &bytes, size_t table, unsigned length, Name_Pick &best) {
  if (length < 6 || table + length > bytes.size()) return;
  unsigned count = 0;
  unsigned string_offset = 0;
  if (!read_u16(bytes, table + 2, count) || !read_u16(bytes, table + 4, string_offset)) return;
  size_t records = table + 6;
  size_t storage = table + string_offset;
  if (storage > table + length) return;
  for (unsigned i = 0; i < count; i++) {
    size_t rec = records + (size_t)i * 12;
    if (rec + 12 > table + length) break;
    unsigned platform = 0, encoding = 0, language = 0, name_id = 0, nbytes = 0, offset = 0;
    if (!read_u16(bytes, rec, platform) || !read_u16(bytes, rec + 2, encoding) ||
        !read_u16(bytes, rec + 4, language) || !read_u16(bytes, rec + 6, name_id) ||
        !read_u16(bytes, rec + 8, nbytes) || !read_u16(bytes, rec + 10, offset)) return;
    int rank = name_rank(platform, encoding, language, name_id);
    if (rank < 0 || storage + offset + nbytes > bytes.size()) continue;
    const unsigned char *data = reinterpret_cast<const unsigned char *>(bytes.data()) + storage + offset;
    std::string text;
    if (platform == 1) {
      bool ascii = true;
      for (unsigned n = 0; n < nbytes; n++) {
        if (data[n] < 32 || data[n] > 126) ascii = false;
      }
      if (!ascii) continue;
      text.assign(reinterpret_cast<const char *>(data), nbytes);
    } else {
      text = utf16be_to_utf8(data, nbytes);
    }
    keep_name(best, rank, tidy_family(text));
  }
}

void remember_family(std::vector<std::string> &families, const std::string &name) {
  if (name.empty()) return;
  for (const std::string &have : families) {
    if (have == name) return;
  }
  families.push_back(name);
}

void parse_sfnt(const std::string &bytes, size_t base, std::vector<std::string> &families) {
  unsigned version = 0;
  unsigned tables = 0;
  if (!read_u32(bytes, base, version) || !read_u16(bytes, base + 4, tables)) return;
  if (version != 0x00010000 && version != 0x4F54544F && version != 0x74727565) return;
  for (unsigned i = 0; i < tables; i++) {
    size_t rec = base + 12 + (size_t)i * 16;
    if (rec + 16 > bytes.size()) return;
    if (std::memcmp(bytes.data() + rec, "name", 4) != 0) continue;
    unsigned offset = 0;
    unsigned length = 0;
    if (!read_u32(bytes, rec + 8, offset) || !read_u32(bytes, rec + 12, length)) return;
    Name_Pick best;
    best.rank = -1;
    parse_name_table(bytes, offset, length, best);
    remember_family(families, best.text);
    return;
  }
}

void collect_families(const std::string &bytes, std::vector<std::string> &families) {
  if (bytes.size() >= 12 && std::memcmp(bytes.data(), "ttcf", 4) == 0) {
    unsigned count = 0;
    if (!read_u32(bytes, 8, count)) return;
    for (unsigned i = 0; i < count; i++) {
      unsigned offset = 0;
      if (!read_u32(bytes, 12 + (size_t)i * 4, offset)) return;
      parse_sfnt(bytes, offset, families);
    }
    return;
  }
  parse_sfnt(bytes, 0, families);
}

bool font_filename(const char *name) {
  if (!name) return false;
  size_t length = std::strlen(name);
  if (length < 4) return false;
  const char *ext = name + length - 4;
  return fl_utf_strncasecmp(ext, ".ttf", 4) == 0 ||
         fl_utf_strncasecmp(ext, ".otf", 4) == 0 ||
         fl_utf_strncasecmp(ext, ".ttc", 4) == 0 ||
         fl_utf_strncasecmp(ext, ".otc", 4) == 0;
}

bool register_private_font(const std::string &path) {
#if defined(_WIN32)
  wchar_t wide[32768];
  unsigned count = fl_utf8towc(path.c_str(), (unsigned)path.size(), wide, 32767);
  if (!count || count >= 32767) return false;
  wide[count] = 0;
  return AddFontResourceExW(wide, FR_PRIVATE, nullptr) > 0;
#elif defined(__APPLE__)
  CFURLRef url = CFURLCreateFromFileSystemRepresentation(
      kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(path.c_str()), (CFIndex)path.size(), false);
  if (!url) return false;
  Boolean ok = CTFontManagerRegisterFontsForURL(url, kCTFontManagerScopeProcess, nullptr);
  CFRelease(url);
  return ok;
#else
  static void *library = nullptr;
  static int (*add_file)(void *, const unsigned char *) = nullptr;
  static bool opened = false;
  if (!opened) {
    opened = true;
    library = dlopen("libfontconfig.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (!library) library = dlopen("libfontconfig.so", RTLD_LAZY | RTLD_LOCAL);
    if (library) add_file = (int (*)(void *, const unsigned char *))dlsym(library, "FcConfigAppFontAddFile");
  }
  if (!add_file) return false;
  return add_file(nullptr, reinterpret_cast<const unsigned char *>(path.c_str())) != 0;
#endif
}

std::vector<std::string> g_private_families;
std::deque<std::string> g_owned_names;
int g_catalog_count = 0;
bool g_catalog_ready = false;

void load_private_fonts() {
  static bool loaded = false;
  if (loaded) return;
  loaded = true;
  std::vector<std::string> directories;
  if (const char *override_dir = std::getenv("FLUID_FONT_DIR")) {
    if (override_dir[0]) directories.push_back(override_dir);
  }
  std::string exe_dir = executable_dir();
  if (!exe_dir.empty()) {
    directories.push_back(join_path(exe_dir, "fonts"));
    directories.push_back(join_path(exe_dir, "../share/fltk-plus/fonts"));
  }
  for (const std::string &directory : directories) {
    dirent **list = nullptr;
    int count = fl_filename_list(directory.c_str(), &list, fl_casenumericsort);
    if (count < 0) continue;
    for (int i = 0; i < count; i++) {
      const char *name = list[i]->d_name;
      if (!font_filename(name)) continue;
      std::string path = join_path(directory, name);
      std::string bytes = read_file(path);
      std::vector<std::string> families;
      collect_families(bytes, families);
      if (!families.empty() && register_private_font(path)) {
        for (const std::string &family : families) remember_family(g_private_families, family);
      }
    }
    fl_filename_free_list(&list, count);
  }
}

void own_font(const std::string &stored, int index) {
  g_owned_names.push_back(stored);
  Fl::set_font((Fl_Font)index, g_owned_names.back().c_str());
}

bool catalog_has_family(const std::string &family) {
  for (int index = 0; index < g_catalog_count; index++) {
    int attributes = 0;
    const char *name = Fl::get_font_name((Fl_Font)index, &attributes);
    if (family_key(name, attributes) == family) return true;
  }
  return false;
}

void append_private_fonts() {
  for (const std::string &family : g_private_families) {
    if (catalog_has_family(family)) continue;
#if defined(__APPLE__) || (defined(USE_PANGO) && USE_PANGO)
    own_font(family, g_catalog_count);
    g_catalog_count++;
#else
    own_font(std::string(" ") + family, g_catalog_count);
    g_catalog_count++;
    own_font(std::string("B") + family, g_catalog_count);
    g_catalog_count++;
    own_font(std::string("I") + family, g_catalog_count);
    g_catalog_count++;
    own_font(std::string("P") + family, g_catalog_count);
    g_catalog_count++;
#endif
  }
}

int ensure_font_catalog_impl() {
  load_private_fonts();
  if (!g_catalog_ready) {
    // "*" is what also enumerates non-Latin fonts such as Chinese. A null pointer makes Windows skip them.
    g_catalog_count = Fl::set_fonts("*");
    if (g_catalog_count < 1) g_catalog_count = 0;
    append_private_fonts();
    g_catalog_ready = true;
  }
  return g_catalog_count;
}

// Windows and older X11 prefix the family name with a letter that selects the weight; macOS, Wayland, and Pango use the font name directly.
bool bind_family(const std::string &family) {
  if (family.empty()) return false;
  const int count = ensure_font_catalog_impl();
  std::string regular;
  std::string bold;
  for (int index = 0; index < count; index++) {
    int attributes = 0;
    const char *name = Fl::get_font_name((Fl_Font)index, &attributes);
    if (family_key(name, attributes) != family) continue;
    if (attributes == 0 && regular.empty()) regular = family;
    if ((attributes & FL_BOLD) && !(attributes & FL_ITALIC) && bold.empty()) bold = family;
  }
  if (regular.empty()) return false;
  static std::string plain_name;
  static std::string bold_name;
#if defined(__APPLE__) || (defined(USE_PANGO) && USE_PANGO)
  plain_name = regular;
  bold_name = bold.empty() ? regular : bold;
#else
  plain_name = std::string(" ") + regular;
  bold_name = std::string("B") + regular;
#endif
  Fl::set_font(FL_HELVETICA, plain_name.c_str());
  Fl::set_font(FL_HELVETICA_BOLD, bold_name.c_str());
  return true;
}

int scaled_size(int size, int target) {
  if (size <= 0 || target <= 0) return size;
  if (size == g_ui_size || size == 12) return target;
  return size;
}

void scale_widget(Fl_Widget *widget, int target) {
  if (!widget || widget == designer_visible_form()) return;
  widget->labelsize(scaled_size(widget->labelsize(), target));
  if (auto *input = dynamic_cast<Fl_Input_ *>(widget))
    input->textsize(scaled_size(input->textsize(), target));
  if (auto *browser = dynamic_cast<Fl_Browser_ *>(widget))
    browser->textsize(scaled_size(browser->textsize(), target));
  if (auto *text = dynamic_cast<Fl_Text_Display *>(widget))
    text->textsize(scaled_size(text->textsize(), target));
  if (auto *menu = dynamic_cast<Fl_Menu_ *>(widget))
    menu->textsize(scaled_size(menu->textsize(), target));
  if (Fl_Group *group = widget->as_group()) {
    for (int i = 0; i < group->children(); i++) scale_widget(group->child(i), target);
  }
}

void scale_open_windows(int target) {
  if (target <= 0) return;
  if (target == g_ui_size && target == 12) return;
  scale_widget(Fluid.main_window, target);
  scale_widget(settings_window, target);
  for (Fl_Window *window = Fl::first_window(); window; window = Fl::next_window(window)) {
    if (window == Fluid.main_window || window == settings_window) continue;
    if (window == designer_visible_form()) continue;
    scale_widget(window, target);
  }
  g_ui_size = target;
  if (Fluid.main_window) Fluid.main_window->redraw();
  if (settings_window) settings_window->redraw();
}

std::string ui_font_summary_impl() {
  std::string family = saved_ui_family();
  if (family.empty()) return "Automatic";
  int size = saved_ui_size();
  if (size > 0) return family + ", " + std::to_string(size);
  return family;
}

bool edit_ui_font_impl(std::string &family, int &size) {
  Fl_Group *saved = Fl_Group::current();
  Fl_Group::current(nullptr);
  const int count = ensure_font_catalog_impl();
  std::vector<std::string> faces;
  for (int index = 0; index < count; index++) {
    int attributes = 0;
    const char *name = Fl::get_font_name((Fl_Font)index, &attributes);
    std::string key = family_key(name, attributes);
    if (key.empty()) continue;
    if (std::find(faces.begin(), faces.end(), key) == faces.end()) faces.push_back(key);
  }
  std::sort(faces.begin(), faces.end());
  int pick = size > 0 ? size : g_ui_size;
  if (pick < 8) pick = 8;
  Fl_Window win(420, 360, ui_text("Font"));
  win.set_modal();
  if (Fluid.main_window) win.position(Fluid.main_window->x() + 80, Fluid.main_window->y() + 60);
  auto *list = new Fl_Hold_Browser(12, 28, 280, 230);
  list->textsize(12);
  int selected = 1;
  for (int i = 0; i < (int)faces.size(); i++) {
    list->add(faces[i].c_str());
    if (faces[i] == family) selected = i + 1;
  }
  if (list->size() > 0) list->value(selected);
  auto *sizes = new Fl_Hold_Browser(302, 28, 106, 230);
  sizes->textsize(12);
  const int kSizes[] = {9, 10, 11, 12, 13, 14, 16, 18, 20, 24};
  for (int item : kSizes) sizes->add(std::to_string(item).c_str());
  for (int i = 1; i <= sizes->size(); i++) {
    if (sizes->text(i) && std::atoi(sizes->text(i)) == pick) sizes->value(i);
  }
  auto *preview = new Fl_Box(12, 266, 396, 40, "AaBbYyZz 界面字体");
  preview->box(FL_DOWN_BOX);
  preview->labelsize(pick > 28 ? 28 : pick);
  auto refresh = [&]() {
    int row = list->value();
    if (row > 0 && row <= (int)faces.size()) family = faces[row - 1];
    int size_row = sizes->value();
    if (size_row > 0 && sizes->text(size_row)) {
      pick = std::atoi(sizes->text(size_row));
      if (pick < 8) pick = 8;
    }
    int shown = pick > 28 ? 28 : pick;
    preview->labelsize(shown);
    preview->redraw();
  };
  list->callback([](Fl_Widget *, void *data) { (*static_cast<decltype(refresh) *>(data))(); }, &refresh);
  sizes->callback(list->callback(), &refresh);
  bool ok = false;
  bool automatic = false;
  auto *use_auto = new Fl_Button(12, 318, 110, 28, ui_text("Automatic"));
  auto *cancel = new Fl_Button(214, 318, 90, 28, ui_text("Cancel"));
  auto *accept = new Fl_Return_Button(312, 318, 96, 28, ui_text("OK"));
  use_auto->callback([](Fl_Widget *widget, void *flag) {
    *static_cast<bool *>(flag) = true;
    widget->window()->hide();
  }, &automatic);
  cancel->callback([](Fl_Widget *widget, void *) { widget->window()->hide(); });
  accept->callback([](Fl_Widget *widget, void *flag) {
    *static_cast<bool *>(flag) = true;
    widget->window()->hide();
  }, &ok);
  win.end();
  Fl_Group::current(saved);
  win.show();
  while (win.shown()) Fl::wait();
  if (automatic) {
    family.clear();
    size = 0;
    return true;
  }
  if (!ok) return false;
  refresh();
  size = pick;
  return !family.empty();
}

// The three platforms name Chinese fonts differently. Query the fonts the system has, then use one that is present.
// A font the user sets in the general settings takes priority over the automatic choice.
void apply_font() {
  std::string chosen = saved_ui_family();
  int wanted = saved_ui_size();
  if (wanted <= 0) wanted = 12;
  if (!chosen.empty() && bind_family(chosen)) {
    scale_open_windows(wanted);
    return;
  }
  if (!g_language) {
    scale_open_windows(wanted);
    return;
  }
  static const char *k_faces[] = {
    "Microsoft YaHei UI",
    "Microsoft YaHei",
    "PingFang SC",
    "Hiragino Sans GB",
    "Songti SC",
    "Noto Sans CJK SC",
    "Noto Sans SC",
    "Source Han Sans SC",
    "WenQuanYi Micro Hei",
    "Droid Sans Fallback",
    "SimSun",
    nullptr
  };
  static std::string plain_name;
  static std::string bold_name;
  const int count = ensure_font_catalog_impl();
  std::string regular;
  std::string bold;
  for (int face = 0; k_faces[face]; face++) {
    regular.clear();
    bold.clear();
    for (int index = 0; index < count; index++) {
      int attributes = 0;
      const char *name = Fl::get_font_name((Fl_Font)index, &attributes);
      if (!name || !contains_ci(name, k_faces[face])) continue;
      if (attributes == 0 && regular.empty()) regular = name;
      if ((attributes & FL_BOLD) && bold.empty()) bold = name;
    }
    if (!regular.empty()) break;
  }
  if (regular.empty()) {
    scale_open_windows(wanted);
    return;
  }
#if defined(__APPLE__) || (defined(USE_PANGO) && USE_PANGO)
  plain_name = regular;
  bold_name = bold.empty() ? regular : bold;
#else
  plain_name = std::string(" ") + regular;
  bold_name = std::string("B") + regular;
#endif
  Fl::set_font(FL_HELVETICA, plain_name.c_str());
  Fl::set_font(FL_HELVETICA_BOLD, bold_name.c_str());
  if (wanted > 0) scale_open_windows(wanted);
}

std::string icon_text(const char *label) {
  std::string text = label ? label : "";
  if (!text.empty() && text[0] == ' ') text.erase(0, 1);
  if (text.size() >= 3 && text.compare(text.size() - 3, 3, "...") == 0)
    text.erase(text.size() - 3);
  return text;
}

void hold_menu_item(Fl_Menu_Item *item) {
  if (!item || !item->text || g_menu_hold.count(item)) return;
  MenuHold hold;
  if (item->labeltype() == _FL_MULTI_LABEL) {
    auto *multi = reinterpret_cast<Fl_Multi_Label *>(const_cast<char *>(item->text));
    if (!multi || multi->typeb != FL_NORMAL_LABEL || !multi->labelb) return;
    hold.icon = true;
    hold.english = icon_text(multi->labelb);
  } else {
    if (!item->text[0]) return;
    hold.icon = false;
    hold.english = item->text;
  }
  g_menu_hold[item] = std::move(hold);
}

void walk_menu(Fl_Menu_Item *menu) {
  if (!menu) return;
  int depth = 0;
  for (;;) {
    if (!menu->text) {
      if (depth == 0) break;
      depth--;
      menu++;
      continue;
    }
    hold_menu_item(menu);
    if (menu->flags & FL_SUBMENU_POINTER)
      walk_menu(static_cast<Fl_Menu_Item *>(menu->user_data()));
    else if (menu->flags & FL_SUBMENU)
      depth++;
    menu++;
  }
}

void apply_menu_item(Fl_Menu_Item *item, MenuHold &hold) {
  const char *translated = translate(hold.english.c_str());
  if (hold.icon) {
    auto *multi = reinterpret_cast<Fl_Multi_Label *>(const_cast<char *>(item->text));
    hold.shown = std::string(" ") + translated + "...";
    multi->labelb = hold.shown.c_str();
  } else {
    item->label(translated);
  }
}

void capture_widget(Fl_Widget *widget) {
  if (!widget) return;
  if (widget->label() && widget->label()[0] && !g_widget_english.count(widget))
    g_widget_english[widget] = widget->label();
  if (widget->tooltip() && widget->tooltip()[0] && !g_widget_tooltip.count(widget))
    g_widget_tooltip[widget] = widget->tooltip();
  if (auto *menu = dynamic_cast<Fl_Menu_ *>(widget)) {
    if (menu->menu()) walk_menu(const_cast<Fl_Menu_Item *>(menu->menu()));
  }
  if (Fl_Group *group = widget->as_group()) {
    for (int i = 0; i < group->children(); i++) capture_widget(group->child(i));
  }
}

void apply_widget(Fl_Widget *widget) {
  auto label = g_widget_english.find(widget);
  if (label != g_widget_english.end()) widget->label(translate(label->second.c_str()));
  auto tip = g_widget_tooltip.find(widget);
  if (tip != g_widget_tooltip.end()) widget->tooltip(translate(tip->second.c_str()));
  if (Fl_Group *group = widget->as_group()) {
    for (int i = 0; i < group->children(); i++) apply_widget(group->child(i));
  }
}

void capture_menu() {
  walk_menu(GUI::menu_app_menu_bar);
}

void apply_menu() {
  for (auto &item : g_menu_hold) apply_menu_item(item.first, item.second);
  if (Fluid.gui.app_menu_bar) Fluid.gui.app_menu_bar->redraw();
}

void remember_window(Fl_Widget *window) {
  if (!window) return;
  for (Fl_Widget *known : g_windows) {
    if (known == window) return;
  }
  g_windows.push_back(window);
}

void apply_language() {
  apply_font();
  fl_ok = translate("OK");
  fl_cancel = translate("Cancel");
  fl_yes = translate("Yes");
  fl_no = translate("No");
  fl_close = translate("Close");
  Fl_File_Chooser::add_favorites_label = translate("Add to Favorites");
  Fl_File_Chooser::all_files_label = translate("All Files (*)");
  Fl_File_Chooser::custom_filter_label = translate("Custom Filter");
  Fl_File_Chooser::existing_file_label = translate("Please choose an existing file!");
  Fl_File_Chooser::favorites_label = translate("Favorites");
  Fl_File_Chooser::filename_label = translate("Filename:");
  Fl_File_Chooser::manage_favorites_label = translate("Manage Favorites");
  Fl_File_Chooser::new_directory_label = translate("New Directory?");
  Fl_File_Chooser::new_directory_tooltip = translate("Create a new directory.");
  Fl_File_Chooser::preview_label = translate("Preview the file");
  Fl_File_Chooser::save_label = translate("Save");
  Fl_File_Chooser::show_label = translate("Show:");
  Fl_File_Chooser::hidden_label = translate("Show hidden files");
  const char *systems = Fl_File_Chooser::filesystems_label;
  if (!systems || !strcmp(systems, "My Computer") || !strcmp(systems, translate("My Computer")))
    Fl_File_Chooser::filesystems_label = translate("My Computer");
  else
    Fl_File_Chooser::filesystems_label = translate("File Systems");
  apply_menu();
  for (Fl_Widget *window : g_windows) {
    apply_widget(window);
    window->redraw();
  }
  // The "code not generated" text in the title follows the current language. -1 does not change whether the project is saved.
  if (Fluid.main_window)
    Fluid.proj.set_modflag(-1);
  code_stage_refresh();
  code_console_apply_language();
  if (Fluid.proj.tree.first || Fluid.proj.tree.current)
    property_page_load(Fluid.proj.tree.current, true);
}

void language_choice_cb(Fl_Widget *widget, void *) {
  auto *choice = static_cast<Fl_Choice *>(widget);
  int index = choice->value();
  if (index < 0 || index >= (int)g_packs.size()) index = 0;
  g_language = index;
  Fluid.preferences.set("ui_language_id", g_packs[g_language].id);
  Fluid.preferences.flush();
  apply_language();
}

void install_language_choice() {
  Fl_Group *tab = w_settings_general_tab;
  const int count = tab->children();
  for (int i = 0; i < count; i++) {
    Fl_Widget *child = tab->child(i);
    if (child->y() < 78 || child->y() >= 500) continue;
    if (child->as_group())
      child->resize(child->x(), child->y() + 32, child->w(), child->h());
    else
      child->position(child->x(), child->y() + 32);
  }

  tab->begin();
  g_language_choice = new Fl_Choice(130, 78, 160, 25, "Language:");
  g_language_choice->labelfont(1);
  g_language_choice->labelsize(12);
  g_language_choice->textsize(12);
  g_language_choice->down_box(FL_BORDER_BOX);
  g_language_choice->align(FL_ALIGN_LEFT);
  g_language_items.resize(g_packs.size() + 1);
  std::memset(g_language_items.data(), 0, g_language_items.size() * sizeof(Fl_Menu_Item));
  for (size_t i = 0; i < g_packs.size(); i++) {
    g_language_items[i].label(g_packs[i].name.c_str());
    g_language_items[i].labelsize(12);
  }
  g_language_choice->menu(g_language_items.data());
  g_language_choice->callback(language_choice_cb);
  tab->end();
}

}  // namespace

std::string ui_font_summary() {
  return ui_font_summary_impl();
}

bool edit_ui_font(std::string &family, int &size) {
  return edit_ui_font_impl(family, size);
}

void apply_ui_font() {
  apply_font();
  for (Fl_Window *window = Fl::first_window(); window; window = Fl::next_window(window))
    window->redraw();
}

int ensure_font_catalog() {
  return ensure_font_catalog_impl();
}

const char *ui_text(const char *english) {
  return translate(english);
}

void localize_menu(Fl_Menu_Item *menu) {
  walk_menu(menu);
  apply_menu();
}

const char *menu_item_source(const Fl_Menu_Item *item) {
  if (!item) return nullptr;
  auto found = g_menu_hold.find(const_cast<Fl_Menu_Item *>(item));
  if (found != g_menu_hold.end() && !found->second.english.empty())
    return found->second.english.c_str();
  return item->label();
}

void localize_window(Fl_Widget *window) {
  if (!window) return;
  capture_widget(window);
  remember_window(window);
  apply_language();
}

void set_menu_label(Fl_Menu_Item *item, const char *english) {
  if (!item || !english) return;
  MenuHold &hold = g_menu_hold[item];
  hold.english = english;
  hold.icon = item->labeltype() == _FL_MULTI_LABEL;
  apply_menu_item(item, hold);
  if (Fluid.gui.app_menu_bar) Fluid.gui.app_menu_bar->redraw();
}

void set_widget_label(Fl_Widget *widget, const char *english) {
  if (!widget || !english) return;
  g_widget_english[widget] = english;
  widget->label(translate(g_widget_english[widget].c_str()));
  widget->redraw();
}

void init_ui_locale() {
  ensure_font_catalog_impl();
  load_packs();
  std::string language_id;
  if (!Fluid.preferences.get("ui_language_id", language_id, std::string("en"))) {
    int old_language = 0;
    if (Fluid.preferences.get("ui_language", old_language, 0) && old_language == 1)
      language_id = "zh-CN";
  }
  g_language = 0;
  for (int i = 0; i < (int)g_packs.size(); i++) {
    if (g_packs[i].id == language_id) g_language = i;
  }
  install_language_choice();
  g_language_choice->value(g_language);
  if (!g_captured) {
    capture_menu();
    if (Fluid.main_window) {
      capture_widget(Fluid.main_window);
      remember_window(Fluid.main_window);
    }
    if (settings_window) {
      capture_widget(settings_window);
      remember_window(settings_window);
    }
    g_captured = true;
  }
  apply_language();
}

}  // namespace fluid
