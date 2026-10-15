#include "footprint.h"

#include "speech.h"
#include "vocab.h"
#include "world.h"

#include <algorithm>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace fa::footprint {

namespace {

// Pieces are gathered again this often while the build stays put: what is built or cleared around
// it changes what fits.
constexpr int kRefreshFrames = 30;

// An entity on the map that building the pieces changes.
struct Alteration {
   std::string words;
   int32_t left;
   int32_t top;
   int32_t right;
   int32_t bottom;
};

struct Pieces {
   Placement placement{};
   bool blueprint = false;
   bool drag = false;
   std::vector<Piece> pieces;
   std::vector<Alteration> alterations;
};

struct Announcement {
   std::string lead;
   int32_t tileX;
   int32_t tileY;
   int32_t x; // where the build is pinned, in fixed point
   int32_t y;
};

std::mutex g_mutex;
std::optional<Pieces> g_pieces;
std::optional<Announcement> g_announcement;
// Whether the cursor's last step was onto the build, to say once that it left.
bool g_wasInside = false;

// The thread drawing the preview alone touches these.
Pieces g_gathering;
int g_framesSinceGathered = 0;
thread_local bool t_gathering = false;

template <class Area>
bool covers(const Area& area, int32_t tileX, int32_t tileY) {
   return tileX >= area.left && tileX <= area.right && tileY >= area.top && tileY <= area.bottom;
}

// Within the box around every piece.
bool inside(const Pieces& pieces, int32_t tileX, int32_t tileY) {
   bool any = false;
   int32_t left = 0, top = 0, right = 0, bottom = 0;
   for (const Piece& piece : pieces.pieces) {
      left = any ? std::min(left, piece.left) : piece.left;
      top = any ? std::min(top, piece.top) : piece.top;
      right = any ? std::max(right, piece.right) : piece.right;
      bottom = any ? std::max(bottom, piece.bottom) : piece.bottom;
      any = true;
   }
   return any && tileX >= left && tileX <= right && tileY >= top && tileY <= bottom;
}

void append(std::string& out, const std::string& part) {
   if (part.empty()) return;
   if (!out.empty()) out += ", ";
   out += part;
}

// The piece read on the tile `tileX`, `tileY`: "Assembling machine 2 East planned, already built,
// producing Iron gear wheel".
std::string describe(const Piece& piece, int32_t tileX, int32_t tileY) {
   std::string out = piece.direction >= 0
                        ? vocab::kFootprintPieceFacing(piece.name, vocab::direction(piece.direction / 2))
                        : vocab::kFootprintPiece(piece.name);
   if (piece.fit == Fit::AlreadyBuilt) append(out, std::string(vocab::kAlreadyBuilt));
   if (piece.fit == Fit::Blocked) append(out, piece.reason);
   const size_t index = size_t(tileY - piece.top) * size_t(piece.right - piece.left + 1) + size_t(tileX - piece.left);
   if (index < piece.tileFacts.size()) append(out, piece.tileFacts[index]);
   append(out, piece.drawn);
   return out;
}

// The pieces on the tile, then what building them does to the map there: "Landfill planned,
// Transport belt East planned, removes Tree".
std::string tileWords(const Pieces& pieces, int32_t tileX, int32_t tileY) {
   std::string out;
   for (const Piece& piece : pieces.pieces)
      if (covers(piece, tileX, tileY)) append(out, describe(piece, tileX, tileY));
   for (const Alteration& alteration : pieces.alterations)
      if (covers(alteration, tileX, tileY)) append(out, alteration.words);
   return out;
}

// "2 blocked, Tree is in the way, 1 already built"; nothing when it all fits. A drag leaves out
// what it has just built.
std::string summary(const Pieces& pieces) {
   int blocked = 0, built = 0;
   const Piece* firstBlocked = nullptr;
   for (const Piece& piece : pieces.pieces) {
      if (piece.fit == Fit::Blocked && !blocked++) firstBlocked = &piece;
      if (piece.fit == Fit::AlreadyBuilt) ++built;
   }
   std::string out;
   if (blocked) out = vocab::kFootprintBlocked(blocked, firstBlocked->reason);
   if (built && !pieces.drag) out += (out.empty() ? "" : ", ") + vocab::kFootprintAlreadyBuilt(built);
   return out;
}

// The announcement's words, from the pieces of the build where it is pinned.
std::string compose(const Announcement& announcement, const Pieces& pieces) {
   std::string out;
   if (pieces.blueprint) append(out, summary(pieces));
   append(out, tileWords(pieces, announcement.tileX, announcement.tileY));
   std::string lead = announcement.lead;
   append(lead, out);
   return lead;
}

} // namespace

bool wants(const Placement& placement) {
   {
      std::scoped_lock lock(g_mutex);
      if (g_announcement || !g_pieces || g_pieces->placement != placement) return true;
   }
   return placement.pinned && ++g_framesSinceGathered >= kRefreshFrames;
}

void begin(const Placement& placement, bool blueprint, bool drag) {
   g_gathering = {placement, blueprint, drag, {}, {}};
   g_framesSinceGathered = 0;
   t_gathering = true;
}

bool gathering() { return t_gathering; }

bool gatheringDetails() { return t_gathering && g_gathering.placement.pinned; }

void add(Piece piece) { g_gathering.pieces.push_back(std::move(piece)); }

void alter(std::string words, int32_t left, int32_t top, int32_t right, int32_t bottom) {
   for (const Alteration& alteration : g_gathering.alterations)
      if (alteration.words == words && alteration.left == left && alteration.top == top) return;
   g_gathering.alterations.push_back({std::move(words), left, top, right, bottom});
}

void end() {
   t_gathering = false;
   std::string words;
   {
      std::scoped_lock lock(g_mutex);
      g_pieces = std::move(g_gathering);
      const Placement& placement = g_pieces->placement;
      // Following the cursor, the build is gathered only as it moves or turns.
      if (!placement.pinned) {
         if (g_pieces->blueprint) words = summary(*g_pieces);
      } else if (g_announcement && g_announcement->x == placement.x && g_announcement->y == placement.y) {
         words = compose(*g_announcement, *g_pieces);
         g_announcement.reset();
      }
   }
   if (!words.empty()) speech::say(std::move(words));
}

void clear() {
   std::scoped_lock lock(g_mutex);
   g_pieces.reset();
   g_announcement.reset();
   g_wasInside = false;
}

std::string tilePrefix(int32_t tileX, int32_t tileY) {
   std::scoped_lock lock(g_mutex);
   if (!g_pieces || !g_pieces->placement.pinned) return {};
   const bool wasInside = std::exchange(g_wasInside, inside(*g_pieces, tileX, tileY));
   if (std::string words = tileWords(*g_pieces, tileX, tileY); !words.empty()) return words;
   return wasInside && !g_wasInside ? std::string(vocab::kFootprintOutside) : std::string();
}

void announce(std::string lead, int32_t tileX, int32_t tileY) {
   const std::optional<world::CursorPosition> pinned = world::pinnedBuild();
   if (!pinned) return;
   std::scoped_lock lock(g_mutex);
   g_announcement = Announcement{std::move(lead), tileX, tileY, pinned->x, pinned->y};
}

} // namespace fa::footprint
