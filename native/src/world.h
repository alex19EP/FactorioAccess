#pragma once

#include <cstdint>
#include <optional>

// The game world under the FA cursor. The mod reports where its cursor is, and the game's own
// cursor position follows it: hover selection, building, mining, opening entities and every
// selection tool act there as they would at the mouse. The keys that do so are the game's own
// controls; helper-scripts/factorio-access-keys.ps1 gives every mouse button binding a key.
namespace fa::world {

// From the mod's Lua, on whatever thread runs it. `playerIndex` is LuaPlayer::index; only the
// local player's calls count. Positions are map positions in tiles.
void setCursor(int playerIndex, double x, double y);
// Gives the game cursor back to the mouse.
void releaseCursor(int playerIndex);
// Whether the mod drives the game cursor in the current game. The real mouse then plays no part in
// the world: a blind player does not know where it rests.
bool drivesCursor();
// Where the mod's cursor is in the current game, in the game's fixed point map units, while the
// mod drives the game cursor.
struct CursorPosition {
   int32_t x;
   int32_t y;
   bool operator==(const CursorPosition&) const = default;
};
std::optional<CursorPosition> cursorPosition();
// Where PlayerInputSource::getCursorMapPosition reads while the game handles the key it polled
// last, which then carries the position to every client in its input action (custom inputs have
// it as cursor_position); nothing for where the cursor is. The scanner sets it for each scanner
// key and clears it at every poll. Game thread.
void setKeyCursor(std::optional<CursorPosition> position);
// Whether `playerIndex` may be this client's player: false only once a game with a local player
// is up and that player is someone else.
bool mayBeLocalPlayer(int playerIndex);
// The direction the game builds the item in hand in (GameView::buildDirection, a 16-way
// defines.direction), or -1 when `playerIndex` is not this client's player or there is none. The
// game keeps it across items; rotate turns it as soon as the key is read, before the mod's Lua
// sees the key. A blueprint keeps its own rotation, which this is not.
int buildDirection(int playerIndex);

// The entity or blueprint in hand as this client would build it: rotate and flip change it as soon
// as the game reads the key, before the mod's Lua sees the key.
struct HeldBuild {
   bool blueprint = false;
   int direction = 0;          // 16-way defines.direction; a blueprint's rotation
   int width = 0;              // tiles east to west, as built
   int height = 0;             // tiles north to south, as built
   bool flippable = false;     // an entity: whether the flip keys change it at all
   bool mirrored = false;      // an entity that flips by mirroring: whether it is mirrored
   bool flipHorizontal = false; // a blueprint: flipped east to west, as it lies on the map
   bool flipVertical = false;   // a blueprint: flipped north to south, as it lies on the map
};
// Nothing when `playerIndex` is not this client's player or the hand holds no entity on the grid
// or blueprint: tiles, the rail planner and anything else.
std::optional<HeldBuild> heldBuild(int playerIndex);

// The game's drag building while the build control is held, from its first build to the release.
// A drag builds in a straight line. Rotate during a belt drag does not turn the belt in hand: the
// game turns the line at the cursor's next step off it (`turnPending` until then), and `turns`
// counts every such turn, on across drags, so the mod can say each new one.
struct DragBuild {
   bool turnPending = false;
   int turns = 0;
};
// Nothing when `playerIndex` is not this client's player or the build control holds no drag.
std::optional<DragBuild> dragBuild(int playerIndex);

// Buildings are held by their north-west corner: while the mod drives the cursor, this client
// builds and draws an entity or blueprint in hand so that the north-west corner of its footprint,
// rotated and flipped as it is built, is the cursor tile. Hover, mining and opening stay at the
// cursor. The position is changed before the build action is made, so the action carries it to
// every client; Player::buildFromCursor, which scripts run in the game state, is left alone. Tiles,
// the rail planner, off-grid entities, rail supports, diagonal directions, blueprints with a
// snapping grid and blueprints of only off-grid entities build as vanilla does.

// The build preview says what its tint means, never the colour: when the entity in hand's preview
// moves, turns or changes entity while the mod drives the cursor, this client says the game's
// reason it cannot be built, out of reach or already built, after what the mod says of the move;
// nothing where it can be built. While the build control drags, the mod says nothing of the move
// and this only what blocks the build.

// MinHook detours for Player::getCursorMapPosition, PlayerInputSource::getCursorMapPosition,
// GameView::getMapPosition, ClientDragBuildingContext::update, ControlInput::isActive, Player::getSimpleBuildInput,
// BuildingRenderer::prepareBuildingInGame, Player::buildFromCursor and
// EntityToBeBuiltSettings::draw, and where MinHook keeps the originals.
void* playerCursorDetour();
void** playerCursorOriginal();
void* sourceCursorDetour();
void** sourceCursorOriginal();
void* mapPositionDetour();
void** mapPositionOriginal();
void* dragUpdateDetour();
void** dragUpdateOriginal();
void* isActiveDetour();
void** isActiveOriginal();
void* simpleBuildInputDetour();
void** simpleBuildInputOriginal();
void* prepareBuildingDetour();
void** prepareBuildingOriginal();
void* buildFromCursorDetour();
void** buildFromCursorOriginal();
void* settingsDrawDetour();
void** settingsDrawOriginal();

} // namespace fa::world
