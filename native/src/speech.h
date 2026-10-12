#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace fa::speech {

// False for a game nobody plays at the keyboard: a benchmark (the mod's test runs), a dedicated
// server, save creation. Such a game is left silent.
bool hasPlayer();

// Starts the speech thread, which owns Prism: a Prism backend instance is not thread-safe, so
// every backend call happens there and other threads only queue text. A game started without a
// player (a benchmark, a dedicated server, save creation) gets no thread and no Prism.
//
// A game an AI agent drives over the dev server starts muted, so the developer's screen reader is
// left alone: the log and the dev server's /speech still get every line. Muted are a game whose
// `directory` (the DLL's) holds `factorio-access-mute.once`, which `just run` and `just start`
// put there for an agent and which this deletes, so only that launch is muted; and a game started
// straight from an agent's shell (AI_AGENT ending in `_agent`, as Claude Code's tool calls have).
// FA_MUTE=1 or 0 overrides both; setMuted sets it, so a reloaded core keeps the choice.
void start(const std::filesystem::path& directory);

// Ends the speech thread and Prism with it, dropping what is still queued; before the core unloads.
void stop();

// Queues text for the screen reader. `interrupt` cuts off whatever is being said and drops
// anything still queued.
void say(std::string text, bool interrupt);

// Queues text the game shows on its own (flying text, console lines) as it reads: a line at a
// time, through text::speakable. Never interrupts, so a burst is read through.
void sayShown(std::string_view text);

// Whether lines stay off the screen reader; see start. The dev server's /mute sets it.
bool muted();
void setMuted(bool muted);

} // namespace fa::speech
