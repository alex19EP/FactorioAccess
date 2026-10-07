#include "text.h"

#include "prototypes.h"

#include <array>

namespace fa::text {

namespace {

constexpr std::array kFormattingTags{std::string_view("color"), std::string_view("font")};

bool isTagNameChar(char c) { return (c >= 'a' && c <= 'z') || c == '-' || c == '_'; }

// Letters and digits of any script: UTF-8 lead and continuation bytes count as letters.
bool isWordChar(char c) {
   auto byte = static_cast<unsigned char>(c);
   return byte >= 0x80 || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

std::string_view trimStart(std::string_view text) {
   while (!text.empty() && text.front() == ' ') text.remove_prefix(1);
   return text;
}

std::string_view trimEnd(std::string_view text) {
   while (!text.empty() && text.back() == ' ') text.remove_suffix(1);
   return text;
}

// Whether `text`, past leading spaces, starts with the word or words `name`.
bool startsWithName(std::string_view text, std::string_view name) {
   text = trimStart(text);
   return !name.empty() && text.starts_with(name) && (text.size() == name.size() || !isWordChar(text[name.size()]));
}

// Whether `text`, before trailing spaces, ends with the word or words `name`.
bool endsWithName(std::string_view text, std::string_view name) {
   text = trimEnd(text);
   return !name.empty() && text.ends_with(name) &&
          (text.size() == name.size() || !isWordChar(text[text.size() - name.size() - 1]));
}

// "iron-plate" reads better as "iron plate".
std::string readable(std::string_view name) {
   std::string out;
   for (char c : name) out.push_back(c == '-' || c == '_' ? ' ' : c);
   return out;
}

// What an icon shows: its prototype's localised name and, unless normal, its quality's.
struct Icon {
   std::string name;
   std::string quality;
};

// A sprite path, "item/iron-plate" or "item.iron-plate", names a prototype by its category. Any
// other sprite (utility/clock, quantity-time) reads as its own name.
Icon spriteIcon(std::string_view path) {
   path = path.substr(0, path.find(';')); // [img=infinity;tint=...]
   size_t separator = path.find_first_of("/.");
   if (separator == std::string_view::npos) return {readable(path), {}};
   std::string_view category = path.substr(0, separator);
   std::string_view name = path.substr(separator + 1);
   if (auto localised = prototypes::localisedName(category, name)) return {*localised, {}};
   return {readable(name), {}};
}

// [item=iron-plate,quality=rare]: a prototype icon with optional parameters.
Icon prototypeIcon(std::string_view tag, std::string_view value) {
   std::string_view name = value.substr(0, value.find(','));
   Icon icon;
   if (auto localised = prototypes::localisedName(tag, name))
      icon.name = *localised;
   else
      icon.name = readable(value);
   if (size_t at = value.find(",quality="); at != std::string_view::npos) {
      std::string_view quality = value.substr(at + 9);
      quality = quality.substr(0, quality.find(','));
      if (quality != "normal") icon.quality = prototypes::localisedName("quality", quality).value_or(readable(quality));
   }
   return icon;
}

void appendWords(std::string& out, std::string_view words) {
   out.push_back(' ');
   out += words;
   out.push_back(' ');
}

// Appends what a tag starting at text[0] == '[' reads as, and returns its length, or 0 when the
// bracket does not start a rich text tag. Every icon is read, as its prototype's localised name
// where it has one. An icon beside its own name ("[item=iron-plate] Iron plate") reads the name
// once, and its quality once.
size_t appendTag(std::string_view text, std::string& out) {
   size_t close = text.find(']');
   if (close == std::string_view::npos) return 0;
   std::string_view body = text.substr(1, close - 1);
   bool closing = body.starts_with('/');
   if (closing) body.remove_prefix(1);
   size_t eq = body.find('=');
   std::string_view tag = body.substr(0, eq);
   if (tag.empty()) return 0;
   for (char c : tag)
      if (!isTagNameChar(c)) return 0;

   bool formatting = false;
   for (auto name : kFormattingTags) formatting |= tag == name;
   if (closing || formatting || eq == std::string_view::npos) return close + 1;
   std::string_view value = body.substr(eq + 1);

   // [tooltip=text,locale-key] shows the text; the tooltip key reads the rest.
   if (tag == "tooltip") {
      appendWords(out, value.substr(0, value.rfind(',')));
      return close + 1;
   }

   Icon icon = tag == "img" ? spriteIcon(value) : prototypeIcon(tag, value);
   std::string_view after = text.substr(close + 1);
   after = after.substr(0, after.find('\n'));
   bool named = startsWithName(after, icon.name) || endsWithName(out, icon.name);
   if (!icon.quality.empty() && !(named && after.find(icon.quality) != std::string_view::npos))
      appendWords(out, icon.quality);
   if (!named) appendWords(out, icon.name);
   return close + 1;
}

} // namespace

std::string speakable(std::string_view text) {
   std::string raw;
   raw.reserve(text.size());
   for (size_t i = 0; i < text.size();) {
      if (text[i] == '[') {
         if (size_t length = appendTag(text.substr(i), raw)) {
            i += length;
            continue;
         }
      }
      raw.push_back(text[i++]);
   }

   std::string out;
   out.reserve(raw.size());
   bool pendingSpace = false;
   for (char c : raw) {
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
         pendingSpace = !out.empty();
         continue;
      }
      if (pendingSpace) out.push_back(' ');
      pendingSpace = false;
      out.push_back(c);
   }
   return out;
}

} // namespace fa::text
