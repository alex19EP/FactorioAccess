#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The icons a sighted player sees on an entity: its status icons (no power, no fuel, no ammo ...),
// and in alt mode its info icons (recipe, contents, filters, modules, fluid, ammo, signals ...).
// The entity is drawn again, into a draw queue of our own that nothing renders, with the main
// view's render parameters, so what is read is what the game draws there: alt mode and the status
// icons setting decide as they do on screen, and modded entities read the same way.
namespace fa::entityicons {

// An info icon: the prototype it depicts as rich text names it, and its quality badge.
struct Icon {
   std::string_view kind; // "item", "fluid", "recipe", "virtual-signal" ...; empty for a utility sprite
   std::string name;      // the prototype's internal name, or the utility sprite's name
   std::string quality;   // the badge's quality, empty for none
   std::string comparison; // drawn before the badge's quality ("≥"), empty for none
   bool anyQuality = false; // the badge says any quality
   bool denied = false;     // a filter that denies rather than allows
};

struct Drawn {
   // Utility sprite names of the status icons, in drawing order ("electricity_icon").
   std::vector<std::string> status;
   std::vector<Icon> icons;
   // How much the draw queued: sprites, decaying corpses and special draw orders, leaving out the
   // empty 1 by 1 sprite. None for an entity the view does not show, such as the invisible helpers
   // mods place.
   size_t orders = 0;
};

// Draws `entity` (an Entity*) and returns what it showed. Nothing outside a game with a view. On
// the thread that runs the game's Lua, while the game state stands still.
std::optional<Drawn> read(const void* entity);

// The same with other render parameters (a RenderParameters*): a blueprint picture's own.
std::optional<Drawn> read(const void* entity, const void* renderParameters);

// Whether the view shows `entity` at all: its draw queues something. True outside a game with a
// view, where nothing can be drawn to tell. Where read() may run.
bool draws(const void* entity);

// MinHook detours for Entity::drawAlert and DrawQueue::drawInfoIcon, and where MinHook keeps the
// originals.
void* drawAlertDetour();
void** drawAlertOriginal();
void* drawInfoIconDetour();
void** drawInfoIconOriginal();

} // namespace fa::entityicons
