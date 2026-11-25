//
// A choice that owns its labels and menu items. The original fluid directory does not use this file.

#ifndef FLUID_PLUS_OWNED_CHOICE_H
#define FLUID_PLUS_OWNED_CHOICE_H

#include <FL/Fl_Choice.H>
#include <FL/Fl_Menu_Item.H>

#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace fluid {

// Fl_Choice::add shares one global menu buffer. Creating and destroying choices in a row frees it twice.
// Each choice keeps its own labels and menu items.
class Owned_Choice : public Fl_Choice {
public:
  std::vector<std::string> labels;
  std::vector<Fl_Menu_Item> items;
  Owned_Choice(int x, int y, int w, int h) : Fl_Choice(x, y, w, h) {}
  ~Owned_Choice() override { menu(nullptr); }

  // Each entry in source is "text, value". The entry whose value equals current becomes the current item.
  void set_items(const std::vector<std::pair<std::string, int>> &source, int current) {
    menu(nullptr);
    labels.clear();
    labels.reserve(source.size());
    for (const auto &item : source) labels.push_back(item.first);
    items.assign(source.size() + 1, Fl_Menu_Item());
    int selected = 0;
    bool matched = false;
    for (size_t i = 0; i < source.size(); i++) {
      std::memset(&items[i], 0, sizeof(Fl_Menu_Item));
      items[i].text = labels[i].c_str();
      items[i].user_data((void *)(fl_intptr_t)source[i].second);
      if (!matched && source[i].second == current) {
        selected = (int)i;
        matched = true;
      }
    }
    std::memset(&items[source.size()], 0, sizeof(Fl_Menu_Item));
    menu(items.data());
    if (!source.empty()) value(selected);
  }
};

}

#endif
