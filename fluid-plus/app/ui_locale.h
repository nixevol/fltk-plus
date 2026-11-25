//
// FLUID interface language. English is written into the UI.
// Other languages are read from JSON packs in the locale directory.

#ifndef FLUID_APP_UI_LOCALE_H
#define FLUID_APP_UI_LOCALE_H

#include <string>

struct Fl_Menu_Item;
class Fl_Widget;

namespace fluid {

// Read the saved language, fill the choice, and refresh the UI in that language.
void init_ui_locale();

// Return interface text for the current language. Returns the source when there is no translation.
const char *ui_text(const char *english);

// Remember and translate one of FLUID's own windows. Do not use this on a window the user is designing.
void localize_window(Fl_Widget *window);

// Remember and translate a menu. Submenus are included.
void localize_menu(Fl_Menu_Item *menu);

// The English source of a menu item before translation.
// Code generation and project loading use this, not the text shown in the UI.
const char *menu_item_source(const Fl_Menu_Item *item);

// Replace a menu item's English source and show it in the current language.
void set_menu_label(Fl_Menu_Item *item, const char *english);

// Replace a widget's English source and show it in the current language.
void set_widget_label(Fl_Widget *widget, const char *english);

// Apply the saved interface font. An empty family keeps the automatic font for the language.
void apply_ui_font();

// Full path of the running program. This is the executable file, not a library
// that happens to share the same base name.
std::string executable_path();

// Scan system fonts and load fonts from the fonts directory next to the executable.
// Returns the length of the font table.
int ensure_font_catalog();

// Open the cross-platform font dialog. Returns false when cancelled.
// An empty family means switch back to the automatic font.
bool edit_ui_font(std::string &family, int &size);

// Description of the current interface font, for example "Microsoft YaHei, 12" or "Automatic".
std::string ui_font_summary();

}

#endif
