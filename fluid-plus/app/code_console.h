//
// Fluid Plus code panel: comments, includes, declarations, and the rest.

#ifndef FLUID_PLUS_CODE_CONSOLE_H
#define FLUID_PLUS_CODE_CONSOLE_H

class Fl_Group;
class Node;

namespace fluid {

// Build the code panel. It is a dockable panel; the dock owns its caption, folding, and floating.
Fl_Group *make_code_console(int x, int y, int w, int h);

void code_console_load(Node *node);

// Drop the code panel's pointer before a node is deleted.
void code_console_reset();

// Rewrite the code panel's tab titles for the current language.
void code_console_apply_language();

}

#endif
