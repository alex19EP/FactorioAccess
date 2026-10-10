#!/usr/bin/env python3
"""In-game checks of the native cursor and its camera, over the DLL's dev server (src/devserver.h).

The mod's Lua tests run without graphics, so no view is shown there and the DLL's cursor cannot run.
These checks drive a game that shows one: movement keys and the screen's edge, the pull back onto the
screen when zooming in, a far jump opening remote view, the camera following the cursor, a camera the
game moves bringing the cursor, leaving remote view, the full map's cells, the skip and cycle keys, and
reading the fog of war's picture.

Before running:
- a game with the DLL and the dev server on (FA_DEV=1, FA_DEV_PORT for another port than 8773), in a
  throwaway freeplay save (the checks clear a strip of ground and build on it), past the intro, in the
  character view, with no window open;
- FA's keys moved off W, A, S and D (helper-scripts/factorio-access-keys.ps1), or walking moves the
  character and remote view pans with the cursor keys;
- the game in English: the cycle check reads "1 of 2".

    python native/tools/cursor-checks.py --port 8775 --script-output <game folder>/script-output

Each check prints PASS or FAIL with what it saw; the exit code is the number that failed.
"""

import argparse
import math
import os
import re
import sys
import urllib.request

CURSOR = 'package.loaded["__FactorioAccess__/scripts/cursor.lua"]'
OUTPUT = "fa-cursor-check.txt"


class Game:
    def __init__(self, port, script_output):
        self.base = f"http://localhost:{port}"
        self.output = os.path.join(script_output, OUTPUT)

    def get(self, path, data=None):
        body = data.encode() if data is not None else None
        with urllib.request.urlopen(self.base + path, data=body, timeout=120) as reply:
            return reply.read().decode("utf-8", "replace")

    def keys(self, spec):
        """Presses the keys and returns what was said while they ran."""
        return self.get("/key", spec)

    def wait(self, frames):
        self.keys(f"wait={frames}")

    def console(self, code, mod=False):
        """Runs Lua from the game's console, in the mod's own state when `mod`."""
        command = ("/c __FactorioAccess__ " if mod else "/c ") + code
        # The dev server types "_" as a space and "%XX" as the byte XX.
        typed = command.replace("%", "%25").replace("_", "%5F").replace(" ", "_")
        assert not any(c.isspace() for c in typed), "one line of Lua"
        return self.keys(f"grave wait=20 text={typed} enter wait=20")

    def value(self, expression, mod=False):
        """What a Lua expression gives, as tostring writes it."""
        if os.path.exists(self.output):
            os.remove(self.output)
        self.console(f'helpers.write_file("{OUTPUT}",tostring({expression}))', mod)
        with open(self.output, encoding="utf-8") as f:
            return f.read()

    def state(self):
        """The view and the cursor, from /cursor."""
        out = {}
        for line in self.get("/cursor").splitlines():
            name, _, value = line.partition("=")
            out[name] = value
        if out.get("view") == "none":
            return {"controller": "none"}
        x, y = (float(v) for v in out["centre"].split(","))
        left, top, right, bottom = (int(v) for v in out["screen"].split(","))
        cursor = None if out["cursor"] == "none" else tuple(int(v) for v in out["cursor"].split(","))
        return {
            "controller": out["controller"],
            "centre": (x, y),
            "zoom": float(out["zoom"]),
            "screen": (left, top, right, bottom),
            "cell": int(out["cell"]),
            "cursor": cursor,
            "anchored": out["anchored"] == "true",
        }

    def until(self, test, frames=180):
        """Waits up to `frames` for the state to pass `test`, and gives the last state."""
        state = self.state()
        waited = 0
        while not test(state) and waited < frames:
            self.wait(10)
            waited += 10
            state = self.state()
        return state

    def jump(self, x, y):
        """Jumps the cursor as the mod's own jumps do (Cursor.jump)."""
        self.console(f"{CURSOR}.jump(game.player.index,{{x={mid(x)},y={mid(y)}}})", mod=True)

    def home(self):
        """The character view at zoom 1, with the cursor on the character."""
        if self.state()["controller"] == "remote":
            self.console("game.player.exit_remote_view()")
            self.until(lambda s: s["controller"] in ("character", "god"))
        self.console("game.player.zoom=1")
        self.console(f"{CURSOR}.jump(game.player.index,game.player.physical_position)", mod=True)
        self.wait(20)
        return self.state()


def tile_of(coordinate):
    return math.floor(coordinate)


def mid(tile):
    """The middle of a tile along one axis, as Lua reads it."""
    return f"{tile + 0.5}"


def centre_tile(state):
    return (tile_of(state["centre"][0]), tile_of(state["centre"][1]))


def near_cursor(state):
    """Whether the camera is on the cursor's tile."""
    x, y = state["cursor"]
    return abs(state["centre"][0] - (x + 0.5)) <= 1 and abs(state["centre"][1] - (y + 0.5)) <= 1


def cell_target(tile, step, cell):
    """The centre of the cell beyond, as src/cursormoves.h moves on the full map."""
    nxt = tile + step * cell
    return nxt // cell * cell + cell // 2


CHECKS = []


def check(fn):
    CHECKS.append(fn)
    return fn


@check
def movement_keys_step_a_tile(game):
    x, y = game.home()["cursor"]
    seen = []
    for key, want in (("d", (x + 1, y)), ("s", (x + 1, y + 1)), ("a", (x, y + 1)), ("w", (x, y))):
        game.keys(key)
        seen.append(game.state()["cursor"])
        if seen[-1] != want:
            return False, f"after {key} the cursor is at {seen[-1]}, not {want}"
    return True, f"{seen}"


@check
def screen_edge_stops_the_cursor(game):
    state = game.home()
    _, top, right, _ = state["screen"]
    y = state["cursor"][1]
    game.jump(right, y)
    state = game.until(lambda s: s["cursor"] == (right, y), 60)
    if state["cursor"] != (right, y) or state["controller"] != "character":
        return False, f"a jump to the edge left the cursor at {state['cursor']}, view {state['controller']}"
    game.keys("d")
    state = game.state()
    if state["cursor"] != (right, y):
        return False, f"d at the edge moved the cursor to {state['cursor']}"
    return True, f"stays at {right},{y}"


@check
def zooming_in_pulls_the_cursor_onto_the_screen(game):
    state = game.home()
    right = state["screen"][2]
    y = state["cursor"][1]
    game.jump(right, y)
    game.until(lambda s: s["cursor"] == (right, y), 60)
    game.console("game.player.zoom=2")
    state = game.until(lambda s: s["screen"][2] < right and s["screen"][0] <= s["cursor"][0] <= s["screen"][2], 60)
    left, top, new_right, bottom = state["screen"]
    want = centre_tile(state) if state["anchored"] else (new_right, min(max(y, top), bottom))
    if state["cursor"] != want:
        return False, f"cursor {state['cursor']}, screen {state['screen']}, wanted {want}"
    return True, f"pulled from {right},{y} to {state['cursor']}"


@check
def far_jump_opens_remote_view_on_the_cursor(game):
    state = game.home()
    x, y = state["cursor"]
    zoom = state["zoom"]
    game.jump(x + 400, y)
    state = game.until(lambda s: s["controller"] == "remote" and near_cursor(s))
    if state["controller"] != "remote":
        return False, f"view {state['controller']} after a jump 400 tiles away"
    if state["cursor"] != (x + 400, y) or not near_cursor(state):
        return False, f"cursor {state['cursor']}, camera {state['centre']}"
    state = game.until(lambda s: abs(s["zoom"] - zoom) < 0.01, 60)
    if abs(state["zoom"] - zoom) >= 0.01:
        return False, f"remote view at zoom {state['zoom']}, the character view was at {zoom}"
    return True, f"remote view on {state['cursor']} at zoom {state['zoom']}"


@check
def camera_follows_the_cursor(game):
    state = game.state()
    if state["controller"] != "remote":
        return False, "needs the remote view the far jump opened"
    x, y = state["cursor"]
    game.keys("d d d")
    state = game.until(lambda s: s["cursor"] == (x + 3, y) and near_cursor(s))
    if state["cursor"] != (x + 3, y) or not near_cursor(state):
        return False, f"cursor {state['cursor']}, camera {state['centre']}"
    return True, f"camera {state['centre']} on cursor {state['cursor']}"


@check
def camera_the_game_moves_brings_the_cursor(game):
    state = game.state()
    if state["controller"] != "remote":
        return False, "needs remote view"
    x, y = state["cursor"]
    game.console(f"game.player.teleport({{{mid(x + 50)},{mid(y + 10)}}})")
    state = game.until(lambda s: s["cursor"] == (x + 50, y + 10))
    if state["cursor"] != (x + 50, y + 10):
        return False, f"camera {state['centre']}, cursor {state['cursor']}"
    return True, f"cursor {state['cursor']}"


@check
def leaving_remote_view_brings_the_cursor_back(game):
    game.console("game.player.exit_remote_view()")
    state = game.until(lambda s: s["controller"] == "character" and s["cursor"] == centre_tile(s))
    if state["controller"] != "character" or state["cursor"] != centre_tile(state):
        return False, f"view {state['controller']}, cursor {state['cursor']}, camera {state['centre']}"
    return True, f"cursor {state['cursor']} on the character"


@check
def map_key_at_the_character_keeps_the_cursor(game):
    x, y = game.home()["cursor"]
    game.keys("d d")
    game.console(
        "game.player.set_controller({type=defines.controllers.remote,position=game.player.physical_position})"
    )
    state = game.until(lambda s: s["controller"] == "remote" and near_cursor(s))
    ok = state["controller"] == "remote" and state["cursor"] == (x + 2, y) and near_cursor(state)
    detail = f"view {state['controller']}, cursor {state['cursor']}, camera {state['centre']}"
    game.home()
    return ok, detail


@check
def remote_view_opened_elsewhere_brings_the_cursor(game):
    x, y = game.home()["cursor"]
    game.console(
        f"game.player.set_controller({{type=defines.controllers.remote,position={{{mid(x + 300)},{mid(y - 200)}}}}})"
    )
    state = game.until(lambda s: s["controller"] == "remote" and s["cursor"] == (x + 300, y - 200))
    ok = state["controller"] == "remote" and state["cursor"] == (x + 300, y - 200)
    detail = f"view {state['controller']}, cursor {state['cursor']}"
    game.home()
    return ok, detail


@check
def full_map_moves_by_cells(game):
    game.home()
    game.console(
        "game.player.set_controller({type=defines.controllers.remote,position=game.player.physical_position})"
    )
    game.until(lambda s: s["controller"] == "remote")
    game.console("game.player.zoom=0.05")
    state = game.until(lambda s: s["cell"] > 1, 120)
    if state["cell"] <= 1:
        game.home()
        return False, f"no map cells at zoom {state['zoom']}"
    cell = state["cell"]
    x, y = state["cursor"]
    game.keys("d")
    want = (cell_target(x, 1, cell), cell_target(y, 0, cell))
    state = game.until(lambda s: s["cursor"] == want, 60)
    ok = state["cursor"] == want
    detail = f"cells of {cell}: from {x},{y} to {state['cursor']}, wanted {want}"
    game.home()
    return ok, detail


def build_strip(game):
    """A strip of bare grass below the character, with a chest 8 tiles along it and a small pole,
    whose box leaves ground around it in its tile, 11 along. Gives the strip's start."""
    x, y = game.home()["cursor"]
    row = y + 3
    game.console(
        f"local s=game.player.surface "
        f"for _,e in pairs(s.find_entities_filtered{{area={{{{{x - 2},{row - 1}}},{{{x + 14},{row + 2}}}}}}}) do "
        f"if e.type~='character' then e.destroy() end end "
        f"local t={{}} for i={x - 2},{x + 14} do for j={row - 1},{row + 1} do "
        f"t[#t+1]={{name='grass-1',position={{i,j}}}} end end s.set_tiles(t) "
        f"s.create_entity{{name='wooden-chest',position={{{mid(x + 8)},{mid(row)}}},force='player'}} "
        f"s.create_entity{{name='small-electric-pole',position={{{mid(x + 11)},{mid(row)}}},force='player'}}"
    )
    game.wait(10)
    return x, row


@check
def skip_stops_at_the_first_tile_that_differs(game):
    x, row = build_strip(game)
    game.jump(x, row)
    game.until(lambda s: s["cursor"] == (x, row), 60)
    said = game.keys("shift+d")
    state = game.state()
    if state["cursor"] != (x + 8, row):
        return False, f"skip from {x},{row} stopped at {state['cursor']}, the chest is at {x + 8},{row}; said {said!r}"
    game.keys("shift+d")
    state = game.state()
    if state["cursor"] != (x + 9, row):
        return False, f"skip off the chest stopped at {state['cursor']}"
    return True, f"stopped on the chest, then the ground after it; said {said.strip()!r}"


@check
def cycle_goes_through_the_tile_and_wraps(game):
    x, row = build_strip(game)
    game.jump(x + 11, row)
    game.until(lambda s: s["cursor"] == (x + 11, row), 60)
    # Each press goes on from what the mouse selects, the pole at the tile's centre at first.
    said = [game.keys("shift+f")]
    found = re.search(r"(\d+) of (\d+)", said[0])
    if not found or int(found.group(2)) < 2:
        return False, f"the first press said {said[0]!r}, wanted one of at least 2 stops (the pole and the ground)"
    count = int(found.group(2))
    said += [game.keys("shift+f") for _ in range(count)]
    stops = [int(m.group(1)) if (m := re.search(rf"(\d+) of {count}\b", s)) else None for s in said]
    if stops != [(int(found.group(1)) - 1 + i) % count + 1 for i in range(count + 1)]:
        return False, f"said {said}: the stops did not go on one at a time and wrap"
    state = game.state()
    if state["cursor"] != (x + 11, row):
        return False, f"the cycle left the tile: {state['cursor']}"
    return True, f"{[s.strip() for s in said]}"


@check
def fog_of_war_is_read_from_the_map(game):
    x, y = game.home()["cursor"]
    game.jump(x + 120, y)
    state = game.until(lambda s: s["controller"] == "remote" and near_cursor(s))
    if state["controller"] != "remote":
        game.home()
        return False, "the jump did not open remote view"
    said = game.keys("d")
    skipped = game.keys("shift+d")
    state = game.state()
    game.home()
    if said.strip() in ("", "(silence)"):
        return False, "a step in the fog said nothing"
    if state["cursor"] is None or state["cursor"][0] <= x + 121:
        return False, f"a skip in the fog did not move on: {state['cursor']}, said {skipped!r}"
    return True, f"step said {said.strip()!r}, skip said {skipped.strip()!r}"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", type=int, default=8773)
    parser.add_argument("--script-output", required=True, help="the game's script-output folder")
    parser.add_argument("checks", nargs="*", help="names of the checks to run (all by default)")
    args = parser.parse_args()

    game = Game(args.port, args.script_output)
    state = game.state()
    if state["controller"] not in ("character", "god"):
        print(f"The game must be in the character or god view, not {state['controller']}", file=sys.stderr)
        return 1
    # The first Lua command of a save only warns that achievements go.
    game.console("local x=1")
    game.console("local x=1")

    failed = 0
    for fn in CHECKS:
        if args.checks and fn.__name__ not in args.checks:
            continue
        try:
            ok, detail = fn(game)
        except Exception as e:  # a check that cannot run is a failure, and the rest still run
            ok, detail = False, f"{type(e).__name__}: {e}"
        failed += not ok
        print(f"{'PASS' if ok else 'FAIL'} {fn.__name__}: {detail}", flush=True)
    game.home()
    print(f"\n{len(CHECKS) if not args.checks else len(args.checks)} checks, {failed} failed")
    return failed


if __name__ == "__main__":
    sys.exit(main())
