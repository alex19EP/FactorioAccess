#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// The mod's own sounds (sonifiers, radars, the health bar), which the game's sound API cannot
// play: panned, filtered when behind the player, looping, retuned while playing, or generated
// tones. The mod's scripts/launcher-audio.lua builds the commands; fa_native.audio hands them
// here. They change nothing in the game, only what this client hears.
namespace fa::audio {

// A value over the seconds since its sound began: a constant, or points joined by straight lines
// or held until the next point (`jump`). Before the first point it is the first value, after the
// last the last.
struct Parameter {
   struct Point {
      double time = 0;
      double value = 0;
      bool jump = false;
   };
   double constant = 0;
   std::vector<Point> points;

   double at(double seconds) const;
};

enum class Wave { Sine, Square, Triangle, Saw };

struct Source {
   // An encoded file (WAV, FLAC, MP3, OGG Vorbis), or a generated tone when `bytes` is null.
   std::string name;
   std::shared_ptr<const std::string> bytes;
   Wave wave = Wave::Sine;
   double frequency = 0;
   std::optional<double> duration; // a tone that does not loop ends after this
   std::optional<double> fadeOut;  // over the last seconds of `duration`
};

// Starts the sound `id`, or retunes it when it already plays: then its source, start time and
// filter stay, its parameters are replaced, and looping can only turn off.
struct Patch {
   std::string id;
   Source source;
   Parameter volume{1.0};
   Parameter pan{0.0}; // -1 left to 1 right, equal power
   Parameter playbackRate{1.0};
   bool looping = false;
   double startTime = 0; // seconds from now
   // A low-pass filter blended in by filterGain (0 dry, 1 filtered).
   std::optional<double> lpfCutoff;
   std::optional<Parameter> filterGain;
};

struct Stop {
   std::string id;
};

struct StopAll {};

using Command = std::variant<Patch, Stop, StopAll>;

// Starts the audio thread, which owns the device and every sound. A game without a player gets
// none, and commands to it are dropped.
void start();

// Queues commands to run together, in order.
void submit(std::vector<Command> commands);

} // namespace fa::audio
