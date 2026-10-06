#pragma once

// Raw widget trees of the game's windows, for working out recipes. Each window class gets one
// file, gui-dumps/<class>.txt beside the log, holding every distinct structure the window showed
// this run (a tabbed pane only holds its selected page, so each tab is a state of its own), each at
// its latest texts and values. The folder fills up as the menus are browsed.
//
// A line per widget, hidden ones included: class, offset inside the window object when the widget
// is embedded in it (matches the PDB member layout), flags, text, tooltip and control state.

#include <string>
#include <string_view>

#include "agui.h"

namespace fa::screens
{

/// Records `window`'s tree as it is now, rewriting its class's dump file when that adds a state or
/// changes one. Cheap when nothing changed: call it on every build.
void DumpWindow(const agui::Widget* window, std::string_view className);

/// `window`'s tree as it is now, in the dump file's format, without recording it.
std::string DescribeTree(const agui::Widget* window);

} // namespace fa::screens
