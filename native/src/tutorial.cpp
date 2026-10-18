#include "tutorial.h"

#include "game.h"

#include <cstddef>
#include <string_view>
#include <vector>

namespace fa::tutorial {

namespace {

using game::layout;

struct MsvcString {
   union {
      char buffer[16];
      const char* pointer;
   };
   size_t size;
   size_t capacity;
};

std::string_view view(const MsvcString& string) {
   return {string.capacity >= sizeof(string.buffer) ? string.pointer : string.buffer, string.size};
}

// std::vector<Mod*>, as the constructor reads it.
struct ModList {
   const std::byte* const* first;
   const std::byte* const* last;
   const std::byte* const* end;
};

constexpr std::string_view kScriptMods[] = {"FactorioAccess", "Kruise_Kontrol"};

// LuaContext::LuaContext(Path const&, PackagePath const*, MapDeserialiser*, TargetDeserialiser&,
// PrototypeMigrationList&, std::vector<Mod*> const&, Scenario*, Map&)
using Constructor = void* (*)(void* self, const void* scriptPath, const void* packagePath, void* deserialiser,
                              void* targetDeserialiser, void* migrations, const ModList* mods, void* scenario,
                              void* map);
Constructor g_original = nullptr;

// TutorialManager::activeTutorial, or null outside a game's global context.
const std::byte** activeTutorialSlot() {
   auto* context = *reinterpret_cast<std::byte* const*>(layout.globalContext);
   if (!context) return nullptr;
   auto* manager = *reinterpret_cast<std::byte* const*>(context + layout.globalTutorialManager);
   return manager ? reinterpret_cast<const std::byte**>(manager + layout.tutorialManagerActive) : nullptr;
}

void* construct(void* self, const void* scriptPath, const void* packagePath, void* deserialiser,
                void* targetDeserialiser, void* migrations, const ModList* mods, void* scenario, void* map) {
   const std::byte** active = activeTutorialSlot();
   const std::byte* tutorial = active ? *active : nullptr;
   if (!tutorial)
      return g_original(self, scriptPath, packagePath, deserialiser, targetDeserialiser, migrations, mods, scenario,
                        map);

   std::vector<const std::byte*> kept;
   for (const std::byte* const* mod = mods->first; mod != mods->last; ++mod) {
      std::string_view name = view(*reinterpret_cast<const MsvcString*>(*mod + layout.modName));
      for (std::string_view wanted : kScriptMods)
         if (name == wanted) kept.push_back(*mod);
   }
   const ModList filtered{kept.data(), kept.data() + kept.size(), kept.data() + kept.size()};

   // Put back even when loading the level throws.
   struct Restore {
      const std::byte** slot;
      const std::byte* value;
      ~Restore() { *slot = value; }
   } restore{active, tutorial};
   *active = nullptr;
   return g_original(self, scriptPath, packagePath, deserialiser, targetDeserialiser, migrations, &filtered, scenario,
                     map);
}

} // namespace

void* luaContextDetour() { return reinterpret_cast<void*>(&construct); }

void** luaContextOriginal() { return reinterpret_cast<void**>(&g_original); }

} // namespace fa::tutorial
