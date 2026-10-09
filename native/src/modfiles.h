#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace fa::modfiles {

// The bytes of a file inside an enabled mod, by its resource path ("__FactorioAccess__/audio/x.ogg"),
// read through the game's own package code, so a zipped mod reads like a folder. Nothing when the
// path names no enabled mod or no file. Call it where the game runs Lua: the mod list must not
// change underneath.
std::optional<std::string> read(std::string_view resourcePath);

} // namespace fa::modfiles
