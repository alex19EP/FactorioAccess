#pragma once

// Pop-ups the game shows for a while and takes away again: the "New tip" notification over the
// side menu, speech bubbles over the map (scenarios and mods make them as speech-bubble entities),
// and the boxes for saving, autosaving and multiplayer (waiting for a player, reconnecting,
// desynced). Lua can't read any of them, so each is spoken as it appears.
namespace fa::popups {

// MinHook detours for the tip notification button's and the speech bubble GUI's constructors and
// for InfoBoxManager::update; and where MinHook keeps the originals.
void* tipDetour();
void** tipOriginal();
void* speechBubbleDetour();
void** speechBubbleOriginal();
void* infoBoxesDetour();
void** infoBoxesOriginal();

} // namespace fa::popups
