#pragma once

#include <string>

namespace fa::speech {

// Starts the speech thread, which owns Prism: a Prism backend instance is not thread-safe, so
// every backend call happens there and other threads only queue text.
void start();

// Queues text for the screen reader. `interrupt` cuts off whatever is being said and drops
// anything still queued.
void say(std::string text, bool interrupt);

} // namespace fa::speech
