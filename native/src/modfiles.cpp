#include "modfiles.h"

#include "game.h"
#include "log.h"

#include <cstddef>
#include <cstdint>

namespace fa::modfiles {

namespace {

using game::layout;

template <class Function>
Function virtualAt(const void* object, uint32_t slot) {
   return (*reinterpret_cast<Function* const*>(object))[slot];
}

// MSVC std::wstring: an 8-wchar small buffer or a heap pointer, then size and capacity.
struct MsvcWString {
   union {
      wchar_t buffer[8];
      wchar_t* pointer;
   };
   size_t size;
   size_t capacity;
};

// A PackagePath the game built in our storage. Its Filesystem::Path is a std::wstring the game
// allocated, so the game's operator delete frees it.
struct PackagePath {
   alignas(8) std::byte storage[64];
   bool built = false;

   ~PackagePath() {
      if (!built) return;
      auto* path = reinterpret_cast<MsvcWString*>(storage + layout.packagePathPath);
      if (path->capacity < std::size(path->buffer)) return;
      const size_t bytes = (path->capacity + 1) * sizeof(wchar_t);
      // MSVC over-aligns blocks of 4 KiB and more and frees them from a shifted pointer; no path
      // is that long.
      if (bytes >= 4096) {
         log::error("A mod path of {} bytes is left allocated", bytes);
         return;
      }
      reinterpret_cast<void (*)(void*, size_t)>(layout.operatorDelete)(path->pointer, bytes);
   }
};

// The game's UniquePointer<ReadStream>: one pointer, deleted through the virtual destructor.
struct ReadStream {
   void* stream = nullptr;

   ~ReadStream() {
      if (stream) virtualAt<void* (*)(void*, unsigned)>(stream, layout.readStreamDestructor)(stream, 1);
   }
};

} // namespace

std::optional<std::string> read(std::string_view resourcePath) {
   const auto* context = *reinterpret_cast<const std::byte* const*>(layout.globalContext);
   const void* modManager = context ? *reinterpret_cast<const void* const*>(context + layout.globalModManager) : nullptr;
   if (!modManager) return std::nullopt;
   if (layout.packagePathSize > sizeof(PackagePath::storage)) {
      log::error("PackagePath is {} bytes, more than the {} we hold", layout.packagePathSize,
                 sizeof(PackagePath::storage));
      return std::nullopt;
   }

   PackagePath path;
   ReadStream stream;
   try {
      // A string_view argument goes by pointer to a copy; the PackagePath comes back in our storage.
      std::string_view argument = resourcePath;
      using Resolve = void* (*)(const void* modManager, void* out, std::string_view* path);
      reinterpret_cast<Resolve>(layout.resolveResourcePath)(modManager, path.storage, &argument);
      path.built = true;
      using Open = void* (*)(const void* packagePath, void* out);
      reinterpret_cast<Open>(layout.packagePathOpen)(path.storage, &stream.stream);
   } catch (...) {
      log::error("Cannot open {}", resourcePath);
      return std::nullopt;
   }
   if (!stream.stream) return std::nullopt;

   // A zip stream asserts (and the game exits) when asked to read past its end, so read exactly
   // what remains.
   const auto remaining = virtualAt<uint64_t (*)(void*)>(stream.stream, layout.readStreamRemaining);
   const auto readSome = virtualAt<uint64_t (*)(void*, char*, uint64_t)>(stream.stream, layout.readStreamRead);
   std::string bytes;
   try {
      bytes.resize(static_cast<size_t>(remaining(stream.stream)));
      size_t used = 0;
      while (used < bytes.size()) {
         const uint64_t got = readSome(stream.stream, bytes.data() + used, bytes.size() - used);
         if (got == 0) break;
         used += static_cast<size_t>(got);
      }
      bytes.resize(used);
   } catch (...) {
      log::error("Cannot read {}", resourcePath);
      return std::nullopt;
   }
   return bytes;
}

} // namespace fa::modfiles
