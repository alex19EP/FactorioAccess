#pragma once

#include <filesystem>
#include <string_view>

// A loopback HTTP server for driving and observing the live game from outside: what was spoken,
// what the navigator shows, and keys typed into the game's own event stream. It starts only when
// `factorio-access-dev.enable` sits next to the DLL (or FA_DEV=1), so players never get it.
//
//   GET  /health              ok and the frame count
//   GET  /speech?since=N      every line spoken since sequence N, and the next N
//   GET  /screen              live screens, the focused node and every node by Tab-stop
//   POST /key                 body: chords such as `tab shift+tab down enter wait=60 escape`;
//                             answers with what was spoken while they ran
//   GET  /dump                the widget tree of every visible window, as gui-dumps writes it
namespace fa::dev {

// Reads the gate and, when it is open, starts the server thread. `directory` is the DLL's.
void start(const std::filesystem::path& directory);

// Once per frame on the game thread: runs the requests that read the game.
void pump();

// Records a spoken line (any thread).
void onSpeech(std::string_view text, bool interrupt);

} // namespace fa::dev
