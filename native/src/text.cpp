#include "text.h"

#include <array>
#include <cctype>

namespace fa::text {

namespace {

constexpr std::array kFormattingTags{std::string_view("color"), std::string_view("font"),
                                     std::string_view("tooltip")};

bool isTagNameChar(char c) { return (c >= 'a' && c <= 'z') || c == '-' || c == '_'; }

char lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

// Whether `text`, past leading spaces, starts with the words of `name` (any case), ending there.
bool startsWithName(std::string_view text, std::string_view name) {
   while (!text.empty() && text.front() == ' ') text.remove_prefix(1);
   if (name.empty() || text.size() < name.size()) return false;
   for (size_t i = 0; i < name.size(); ++i)
      if (lower(text[i]) != lower(name[i])) return false;
   return text.size() == name.size() || !std::isalnum(static_cast<unsigned char>(text[name.size()]));
}

// Appends what a tag starting at text[0] == '[' should read as, and returns its length, or 0 when
// the bracket does not start a rich text tag. An icon right before its own name ("[item=iron-plate]
// Iron plate") is read once.
size_t appendTag(std::string_view text, std::string& out) {
   size_t close = text.find(']');
   if (close == std::string_view::npos) return 0;
   std::string_view body = text.substr(1, close - 1);
   bool closing = body.starts_with('/');
   if (closing) body.remove_prefix(1);
   size_t eq = body.find('=');
   std::string_view name = body.substr(0, eq);
   if (name.empty()) return 0;
   for (char c : name)
      if (!isTagNameChar(c)) return 0;

   bool formatting = false;
   for (auto tag : kFormattingTags) formatting |= name == tag;
   // The info icon only marks a text with a tooltip, which the tooltip key reads.
   bool infoIcon = name == "img" && body.substr(eq + 1) == "info";
   if (!closing && !formatting && !infoIcon && eq != std::string_view::npos) {
      // Icon tags name a prototype; "iron-plate" reads better as "iron plate".
      std::string iconName;
      for (char c : body.substr(eq + 1)) iconName.push_back(c == '-' || c == '_' ? ' ' : c);
      if (!startsWithName(text.substr(close + 1), iconName)) {
         out.push_back(' ');
         out += iconName;
         out.push_back(' ');
      }
   }
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
