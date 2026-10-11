#include "cues.h"

#include "audio.h"
#include "log.h"
#include "modfiles.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace fa::cues {

namespace {

// The "next-machine" sound of data.lua.
constexpr const char* kNextMachinePath = "__FactorioAccess__/audio/sonifiers/grid/crafting_machine/new_machine.ogg";

std::atomic<bool> g_nextMachine{false};
std::mutex g_mutex;
bool g_read = false;
std::shared_ptr<const std::string> g_nextMachineBytes;
// Each sound its own id, so a quick second step starts it again rather than retuning the first.
std::atomic<uint64_t> g_played{0};

} // namespace

void setNextMachine(bool enabled) {
   g_nextMachine = enabled;
   std::scoped_lock lock(g_mutex);
   if (std::exchange(g_read, true)) return;
   if (auto bytes = modfiles::read(kNextMachinePath)) {
      log::info("Cues: read {}, {} bytes", kNextMachinePath, bytes->size());
      g_nextMachineBytes = std::make_shared<const std::string>(std::move(*bytes));
   } else
      log::error("Cues: cannot read {}", kNextMachinePath);
}

void nextMachine() {
   if (!g_nextMachine) return;
   std::shared_ptr<const std::string> bytes;
   {
      std::scoped_lock lock(g_mutex);
      bytes = g_nextMachineBytes;
   }
   if (!bytes) return;
   log::info("Cues: next machine");
   audio::Patch patch;
   patch.id = "fa-next-machine-" + std::to_string(++g_played);
   patch.source.name = "new_machine.ogg";
   patch.source.bytes = std::move(bytes);
   audio::submit({std::move(patch)});
}

} // namespace fa::cues
