#pragma once

#include <cmath>
#include <optional>

// When to say the view's zoom, and the view the player entered, from what each frame shows. A zoom
// is said once it has held still for some frames, when a burst of wheel steps has ended and the
// render mode has caught up with it. A view the player entered is said with the zoom it opens at,
// once that has held still too, in place of the zoom alone. The first zoom seen after a load is
// only recorded, once it has settled: the game eases the camera's zoom in as a map starts, in steps
// that each hold still a few frames.
//
// It runs on frames, not ticks, so it goes on while the map is paused, as the map editor pauses it.
namespace fa::viewannounce {

// Frames a zoom must hold still to be said.
inline constexpr int kHeldFrames = 3;
// Frames a view the player entered waits at the least: remote view opens on the full map and is
// zoomed to where the jump left it a few frames later.
inline constexpr int kViewFrames = 8;
// Frames the first zoom after a load must hold still to be where the view settled.
inline constexpr int kSettleFrames = 30;

enum class Say { Nothing, Zoom, View };

class Announcer {
public:
   // The player entered another view.
   void entered() {
      _viewPending = true;
      _sinceEntered = 0;
   }

   bool viewPending() const { return _viewPending; }

   // A frame showing `zoom`: what to say now.
   Say frame(double zoom) {
      if (_viewPending) ++_sinceEntered;
      if (zoom != _seen) {
         _seen = zoom;
         _held = 0;
         return Say::Nothing;
      }
      if (_held < kSettleFrames) ++_held;
      if (_held < kHeldFrames) return Say::Nothing;
      if (_viewPending) {
         if (_sinceEntered < kViewFrames) return Say::Nothing;
         _viewPending = false;
         _spoken = zoom;
         return Say::View;
      }
      if (!_spoken) {
         if (_held >= kSettleFrames) _spoken = zoom;
         return Say::Nothing;
      }
      if (std::abs(zoom - *_spoken) <= 1e-6) return Say::Nothing;
      _spoken = zoom;
      return Say::Zoom;
   }

private:
   double _seen = 0;
   int _held = 0;
   std::optional<double> _spoken;
   bool _viewPending = false;
   int _sinceEntered = 0;
};

} // namespace fa::viewannounce
