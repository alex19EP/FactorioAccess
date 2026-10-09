#pragma once

#include <string>
#include <string_view>

namespace fa::text {

// Turns Factorio GUI text into something a screen reader can say: drops formatting tags such as
// [color=red]...[/color] and [font=...], reads every icon ([item=iron-plate], [img=item/wood],
// [img=utility/clock]) as its localised name where it has one and as its own name otherwise, and
// collapses whitespace. An icon beside its own name reads the name once. [img=info] only marks a
// text with a tooltip and is not read. Translating names
// touches the game's locale cache, so this runs where the game itself translates (GUI and game
// hooks), never on a thread of our own.
std::string speakable(std::string_view text);

// While one lives, on its thread, speakable also leaves out the icons that only decorate a line
// the words beside them already tell: the quality info icon after a value, a tooltip category's
// icon before its heading. For short readings such as the tile reader's.
class WithoutDecorations {
public:
   WithoutDecorations();
   ~WithoutDecorations();
   WithoutDecorations(const WithoutDecorations&) = delete;
   WithoutDecorations& operator=(const WithoutDecorations&) = delete;

private:
   bool _outer;
};

} // namespace fa::text
