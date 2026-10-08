#pragma once

#include <string>

// What the game highlights around the preview of the entity in hand, said as a sighted player sees
// it: the poles that would power it, the poles it would wire to, its underground partner, what its
// build would replace or turn, the roboports it would link to. It is read from the game's own
// highlight drawing while the preview draws, so modded entities read the same way. The text goes
// after what the preview's tint means, once the frame has drawn the last of them.
namespace fa::highlights {

// Collects what this thread draws until it ends: the preview in hand drawing at a new place, facing
// a new way or as a new entity.
class Collecting {
public:
   Collecting();
   ~Collecting();
   Collecting(const Collecting&) = delete;
   Collecting& operator=(const Collecting&) = delete;
};

// The preview the highlights are for: `entity` (Entity const*), drawn by `settings`
// (EntityToBeBuiltSettings const*). `meaning` is what its tint means, and is said first. While the
// build control drags, only `meaning` is said.
void preview(const void* settings, const void* entity, std::string meaning, bool drag);

// MinHook detours for the game's highlight drawing, and where MinHook keeps the originals.
void* renderCursorBoxDetour();
void** renderCursorBoxOriginal();
void* renderDoubleCursorBoxDetour();
void** renderDoubleCursorBoxOriginal();
void* adapterRenderCursorBoxDetour();
void** adapterRenderCursorBoxOriginal();
void* adapterDestroyDetour();
void** adapterDestroyOriginal();
void* adapterSetDirectionDetour();
void** adapterSetDirectionOriginal();
void* drawPoleConnectionsDetour();
void** drawPoleConnectionsOriginal();
void* findMatchingNetworkDetour();
void** findMatchingNetworkOriginal();
void* roboportPostPrepareDetour();
void** roboportPostPrepareOriginal();
void* drawOnTilesBetweenDetour();
void** drawOnTilesBetweenOriginal();

} // namespace fa::highlights
