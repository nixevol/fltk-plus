//
// Fluid Plus property page on the left.

#ifndef FLUID_PLUS_PROPERTY_SHEET_H
#define FLUID_PLUS_PROPERTY_SHEET_H

class Fl_Group;
class Node;

namespace fluid {

// Build a "Widget Properties" page. The label is used as the tab title.
Fl_Group *make_property_page(int x, int y, int w, int h);

// Refresh the choice and the properties for the current node. An empty node clears the page.
// When force is false, a node that is already shown is not rebuilt.
void property_page_load(Node *node, bool force = false);

// Drop pointers held by the property page before a node is deleted. Do not touch the node.
void property_page_reset();

// Whether the property page is currently showing this node.
bool property_page_shows(const Node *node);

}

#endif
