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

struct Pieces {
   bool blueprint = false;
   int32_t x = 0;
   int32_t y = 0;
   std::vector<Piece> pieces;
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

const Piece* pieceAt(const Pieces& pieces, int32_t tileX, int32_t tileY) {
   for (const Piece& piece : pieces.pieces)
      if (tileX >= piece.left && tileX <= piece.right && tileY >= piece.top && tileY <= piece.bottom) return &piece;
   return nullptr;
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

// "Iron chest East planned, already built".
std::string describe(const Piece& piece) {
   std::string out = piece.direction >= 0
                        ? vocab::kFootprintPieceFacing(piece.name, vocab::direction(piece.direction / 2))
                        : vocab::kFootprintPiece(piece.name);
   if (piece.fit == Fit::AlreadyBuilt) out += ", " + std::string(vocab::kAlreadyBuilt);
   if (piece.fit == Fit::Blocked && !piece.reason.empty()) out += ", " + piece.reason;
   return out;
}

// "2 blocked, Tree is in the way, 1 already built", or that it all fits.
std::string summary(const Pieces& pieces) {
   int blocked = 0, built = 0;
   const Piece* firstBlocked = nullptr;
   for (const Piece& piece : pieces.pieces) {
      if (piece.fit == Fit::Blocked && !blocked++) firstBlocked = &piece;
      if (piece.fit == Fit::AlreadyBuilt) ++built;
   }
   if (!blocked && !built) return vocab::kFootprintFits;
   std::string out;
   if (blocked) out = vocab::kFootprintBlocked(blocked, firstBlocked->reason);
   if (built) out += (out.empty() ? "" : ", ") + vocab::kFootprintAlreadyBuilt(built);
   return out;
}

void append(std::string& out, const std::string& part) {
   if (part.empty()) return;
   if (!out.empty()) out += ", ";
   out += part;
}

// The announcement's words, from the pieces of the build where it is pinned.
std::string compose(const Announcement& announcement, const Pieces& pieces) {
   std::string out;
   if (pieces.blueprint) append(out, summary(pieces));
   if (const Piece* piece = pieceAt(pieces, announcement.tileX, announcement.tileY))
      append(out, describe(*piece));
   else if (inside(pieces, announcement.tileX, announcement.tileY))
      append(out, vocab::kFootprintEmpty);
   std::string lead = announcement.lead;
   append(lead, out);
   return lead;
}

} // namespace

bool wants(int32_t x, int32_t y) {
   {
      std::scoped_lock lock(g_mutex);
      if (g_announcement || !g_pieces || g_pieces->x != x || g_pieces->y != y) return true;
   }
   return ++g_framesSinceGathered >= kRefreshFrames;
}

void begin(bool blueprint) {
   g_gathering = {blueprint, 0, 0, {}};
   g_framesSinceGathered = 0;
}

void add(Piece piece) { g_gathering.pieces.push_back(std::move(piece)); }

void end(int32_t x, int32_t y) {
   g_gathering.x = x;
   g_gathering.y = y;
   std::string words;
   {
      std::scoped_lock lock(g_mutex);
      g_pieces = std::move(g_gathering);
      if (g_announcement && g_announcement->x == x && g_announcement->y == y) {
         words = compose(*g_announcement, *g_pieces);
         g_announcement.reset();
      }
   }
   if (!words.empty()) speech::say(std::move(words), false);
}

void clear() {
   std::scoped_lock lock(g_mutex);
   g_pieces.reset();
   g_announcement.reset();
   g_wasInside = false;
}

std::string tilePrefix(int32_t tileX, int32_t tileY) {
   std::scoped_lock lock(g_mutex);
   if (!g_pieces) return {};
   const bool wasInside = std::exchange(g_wasInside, inside(*g_pieces, tileX, tileY));
   if (const Piece* piece = pieceAt(*g_pieces, tileX, tileY)) return describe(*piece);
   if (g_wasInside) return vocab::kFootprintEmpty;
   return wasInside ? std::string(vocab::kFootprintOutside) : std::string();
}

void announce(std::string lead, int32_t tileX, int32_t tileY) {
   const std::optional<world::CursorPosition> pinned = world::pinnedBuild();
   if (!pinned) return;
   std::scoped_lock lock(g_mutex);
   g_announcement = Announcement{std::move(lead), tileX, tileY, pinned->x, pinned->y};
}

} // namespace fa::footprint
