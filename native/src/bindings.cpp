#include "bindings.h"

#include "game.h"

#include <cstddef>

namespace fa::bindings {

namespace {

using game::layout;

template <class T>
const T& at(const void* object, uint32_t offset) {
   return *reinterpret_cast<const T*>(static_cast<const std::byte*>(object) + offset);
}

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

template <class T>
struct MsvcVector {
   T* first;
   T* last;
   T* end;
};

// SDL_Keycode SDL_GetKeyFromScancode(SDL_Scancode, SDL_Keymod, bool key_event)
using KeyFromScancode = uint32_t (*)(uint32_t scancode, uint16_t modifiers, bool keyEvent);

// ControlInputValue::modifiers, as ControlInputValue::modifierStringForm reads them in 2.1.21.
constexpr uint8_t kControl = 1;
constexpr uint8_t kShift = 2;
constexpr uint8_t kAlt = 4;

} // namespace

std::vector<Key> customInput(std::string_view name) {
   std::vector<Key> keys;
   auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   const std::byte* settings = context ? at<const std::byte*>(context, layout.globalControlSettings) : nullptr;
   if (!settings) return keys;
   const auto& inputs = at<MsvcVector<const std::byte>>(settings, layout.customInputs);
   for (const std::byte* input = inputs.first; input < inputs.last; input += layout.controlInputSize) {
      const std::byte* prototype = at<const std::byte*>(input, layout.controlInputPrototype);
      if (!prototype || view(at<MsvcString>(prototype, layout.prototypeName)) != name) continue;
      for (uint32_t binding : {layout.controlInputKey1, layout.controlInputKey2}) {
         const std::byte* value = input + binding;
         if (at<uint8_t>(value, layout.inputValueType) != layout.inputValueKeyboard) continue;
         // As the key events carry it: with the keycode options the events apply, unmodified.
         uint32_t key = reinterpret_cast<KeyFromScancode>(layout.keyFromScancode)(
            at<uint32_t>(value, layout.inputValueScancode), 0, true);
         uint8_t modifiers = at<uint8_t>(value, layout.inputValueModifiers);
         keys.push_back({key, (modifiers & kShift) != 0, (modifiers & kControl) != 0, (modifiers & kAlt) != 0});
      }
      break;
   }
   return keys;
}

} // namespace fa::bindings
