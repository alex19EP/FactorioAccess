#include "zoom.h"

#include "game.h"

#include <cmath>
#include <cstddef>
#include <cstring>

namespace fa::zoom {

namespace {

using game::layout;

// ZoomDirection.
constexpr int kZoomIn = 0;
constexpr int kZoomOut = 1;
constexpr int kNoZoom = -1;

// Set while PlayerInputSource::processZoom runs: the direction of a zoom key it found pressed.
// Input runs on one thread, but thread-local keeps another thread's isActive out of it.
thread_local bool t_inProcessZoom = false;
thread_local int t_keyDirection = kNoZoom;

std::byte* globalContext() { return *reinterpret_cast<std::byte**>(layout.globalContext); }

const std::byte* controlSettings() {
   return *reinterpret_cast<const std::byte* const*>(globalContext() + layout.globalControlSettings);
}

using GetZoomerFunction = const std::byte* (*)(const void* adapter);

// The steps that make one zoom action double or halve the view. ZoomUtil::computeNewZoom spaces
// its ladder by the steps it is given, log2(rate) * steps doublings apart, with the zoom limits as
// extra rungs; the rate is the controller's (2^(1/7) for the character, 2^(1/3) for the remote
// view), so the steps come from it: 7 and 3.
double doublingSteps() {
   const auto* game = *reinterpret_cast<const std::byte* const*>(globalContext() + layout.globalGame);
   const auto* player = *reinterpret_cast<const std::byte* const*>(game + layout.gameLocalPlayer);
   const auto* adapter = *reinterpret_cast<const std::byte* const*>(player + layout.playerLatencyAdapter);
   if (!adapter) adapter = player + layout.playerGameStateAdapter;
   const auto getZoomer = (*reinterpret_cast<const GetZoomerFunction* const*>(adapter))[layout.adapterGetZoomer];
   double rate;
   std::memcpy(&rate, getZoomer(adapter) + layout.zoomerRate, sizeof(rate));
   // A rate of 1 does not zoom at all.
   return rate > 1.0 ? 1.0 / std::log2(rate) : 1.0;
}

using ProcessZoomFunction = bool (*)(void* source, const void* event);
ProcessZoomFunction g_processZoomOriginal = nullptr;
using ZoomFunction = void (*)(void* source, int direction, double steps);

// The game takes a wheel notch, a zoom control triggered while not held: the press of a held key
// it leaves to the per-tick zoom, which isActive turns off. Here that press zooms instead.
bool detourProcessZoom(void* source, const void* event) {
   t_inProcessZoom = true;
   t_keyDirection = kNoZoom;
   const bool handled = g_processZoomOriginal(source, event);
   t_inProcessZoom = false;
   if (t_keyDirection == kNoZoom) return handled;

   // With "zoom towards cursor" off the game zooms around the middle of the screen.
   auto* settings = *reinterpret_cast<std::byte**>(globalContext() + layout.globalInterfaceSettings);
   bool& towardsCursor = *reinterpret_cast<bool*>(settings + layout.zoomTowardsCursor);
   const bool saved = towardsCursor;
   towardsCursor = false;
   reinterpret_cast<ZoomFunction>(layout.playerInputSourceZoom)(source, t_keyDirection, doublingSteps());
   towardsCursor = saved;
   return true;
}

} // namespace

bool isZoomControl(const void* control) {
   const std::byte* settings = controlSettings();
   return control == settings + layout.controlSettingsZoomIn || control == settings + layout.controlSettingsZoomOut;
}

bool isActive(const void* control, bool held) {
   if (!t_inProcessZoom) return false;
   if (held) t_keyDirection = control == controlSettings() + layout.controlSettingsZoomIn ? kZoomIn : kZoomOut;
   return held;
}

void* processZoomDetour() { return reinterpret_cast<void*>(&detourProcessZoom); }
void** processZoomOriginal() { return reinterpret_cast<void**>(&g_processZoomOriginal); }

} // namespace fa::zoom
