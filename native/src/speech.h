#pragma once

#include <string>

namespace fa::speech {

// Starts the speech thread, which owns Prism: a Prism backend instance is not thread-safe, so
// every backend call happens there and other threads only queue text. A game started without a
// player (a benchmark, a dedicated server, save creation) gets no thread and no Prism.
void start();

// Queues text for the screen reader. `interrupt` cuts off whatever is being said and drops
// anything still queued.
void say(std::string text, bool interrupt);

} // namespace fa::speech
