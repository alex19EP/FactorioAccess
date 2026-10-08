#pragma once

// Keyboard zoom in fixed steps. The game zooms a little every tick while a zoom key is held, so how
// far a tap goes depends on how long the key stays down. Here a press of a key bound to zoom in or
// zoom out makes one zoom action, as a wheel notch does, of as many of the game's steps as make a
// doubling in the current view: the game snaps such a zoom to powers of two, so every press
// doubles or halves the view and lands on the same zoom levels. Holding the key does nothing more. The wheel is left as it is. The key zooms
// around the middle of the screen, never towards the mouse, which a blind player does not place.
namespace fa::zoom {

// Whether `control` is the zoom in or zoom out ControlInput.
bool isZoomControl(const void* control);
// What ControlInput::isActive reports for a zoom control the game reads as `held`: whether it is
// held only to PlayerInputSource::processZoom, which then leaves a key press to this module.
bool isActive(const void* control, bool held);

// MinHook detour for PlayerInputSource::processZoom, and where MinHook keeps the original.
void* processZoomDetour();
void** processZoomOriginal();

} // namespace fa::zoom
