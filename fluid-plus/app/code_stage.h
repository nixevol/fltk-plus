//
// Code mode inside the design area. Source, header, strings, and the project share one editor.

#ifndef FLUID_PLUS_CODE_STAGE_H
#define FLUID_PLUS_CODE_STAGE_H

class Fl_Group;

namespace fluid {

Fl_Group *make_code_stage(int x, int y, int w, int h);

// When opened, embed the code view and refresh it. When closed, only hide it and stop generation.
void code_stage_set_active(bool on);

// After the language or font changes, lay out the file tabs and the find bar again so the text does not overlap.
void code_stage_refresh();

}

#endif
