Last reviewed: 2026-10-10 (update this if reviewing this doc)

# Introduction

Upstream (factorio-access/FactorioAccess) declined the native DLL, so this fork follows its own
course. This page sorts upstream's issues by what the fork's architecture does about them: the DLL
reading the game's own windows, vanilla rules instead of FA's own building and travel, and the
cursor and scanner moving into the DLL.

It covers the 37 issues open upstream and the 25 closed as not planned on 2026-10-10. "Likely"
means the code says so but nobody has checked in the game. Issues marked (np) were closed upstream
as not planned.

Refresh the lists with:

```
gh issue list -R factorio-access/FactorioAccess --state open --limit 500
gh issue list -R factorio-access/FactorioAccess --state closed --limit 1000 --json number,title,stateReason
```

# Fixed

- [#282](https://github.com/factorio-access/FactorioAccess/issues/282) map view: remote view with
  the cursor kept on screen, map cells, pins and tags (9a07584c), and the map view options on
  `CONTROL + TAB` (e4d09127).
- [#192](https://github.com/factorio-access/FactorioAccess/issues/192) (np),
  [#251](https://github.com/factorio-access/FactorioAccess/issues/251) ghosts from far away: placing
  ghosts in remote view works (8ba025d9, f63681e8). Not under fog of war, as in the game. Placing on
  another surface is untested.
- [#284](https://github.com/factorio-access/FactorioAccess/issues/284) 8-way and 16-way building:
  building uses the game's build key and the game's build direction (e64f5531, 799252d2);
  `native/src/preview.cpp` reads 16 directions.
- [#145](https://github.com/factorio-access/FactorioAccess/issues/145) achievements: the
  achievements screen with why one failed (ba9b13fe, bf598842), and tracked achievements on the HUD
  (d5c1d42c).
- [#287](https://github.com/factorio-access/FactorioAccess/issues/287) per-save blueprint library:
  the library screen, opened with `B` (fa533b57).
- [#260](https://github.com/factorio-access/FactorioAccess/issues/260) chat history: the console
  screen (04aa4cc0).
- [#309](https://github.com/factorio-access/FactorioAccess/issues/309) `CONTROL + F` on the map: the
  game's map search, read natively (e10c7df5).
- [#150](https://github.com/factorio-access/FactorioAccess/issues/150) crafting list flooded with
  modded recipes: the character screen reads the game's own crafting menu (1ad832b4), which leaves
  out recipes that cannot be crafted by hand.
- [#137](https://github.com/factorio-access/FactorioAccess/issues/137) electric network overview:
  the electric network screen reads the pole window, its production and consumption included
  (c0f8c40d).
- [#345](https://github.com/factorio-access/FactorioAccess/issues/345) mod dependencies in the
  launcher: the launcher is gone (4730381a); the game's Mods screen is read natively
  (`native/src/screens/ModsScreen.cpp`) and handles dependencies and downloads itself.
- [#347](https://github.com/factorio-access/FactorioAccess/issues/347) an interface for other mods:
  the mod windows part on `CONTROL + TAB` (11854f11) and the generic walker read any mod's ordinary
  GUI, so a mod only needs normal GUI elements.
- [#301](https://github.com/factorio-access/FactorioAccess/issues/301) rocket auto-launch: the game's
  silo window, with its send to orbit automatically checkbox, is read natively. FA's silo tab was
  unreachable and is deleted.
- [#73](https://github.com/factorio-access/FactorioAccess/issues/73) (np) flying text read by every
  player: the DLL speaks the game's flying text on its own client only (b646a288).
- [#118](https://github.com/factorio-access/FactorioAccess/issues/118) (np) guessing Kruise
  Kontrol's state: upstream KK 2.1, with its remarks spoken (ea746375).
- [#61](https://github.com/factorio-access/FactorioAccess/issues/61) (np) sandbox crash: that code is
  gone; sandbox god mode is supported (8ca1bbac).

# Likely fixed, to check in the game

- [#277](https://github.com/factorio-access/FactorioAccess/issues/277) frozen entities,
  [#293](https://github.com/factorio-access/FactorioAccess/issues/293) quality,
  [#294](https://github.com/factorio-access/FactorioAccess/issues/294) spoiling: the tile reader says
  the status icons the game draws (d5cd5032), `Y` reads the game's info panel (a4bbd827) and slot
  tooltips are the game's. None was tried on Space Age content.
- [#129](https://github.com/factorio-access/FactorioAccess/issues/129) ore that needs a fluid to
  mine: the info panel and the game's flying text should say it. Check what hand mining uranium
  says.
- [#180](https://github.com/factorio-access/FactorioAccess/issues/180) (np) remote view muffling
  sounds: the mod's sounds play through the DLL (e6362538), not as the game's positional sounds.
  Check UI sounds in remote view.
- [#292](https://github.com/factorio-access/FactorioAccess/issues/292) agricultural towers,
  [#320](https://github.com/factorio-access/FactorioAccess/issues/320) segmented units: the game's
  windows and info panel may be enough. Nobody has played them yet.

# Obsolete under vanilla rules and the DLL

- [#79](https://github.com/factorio-access/FactorioAccess/issues/79) teleporting the character out
  of a construction's way: teleports are gone (f9ce47a1, 0b9b2f4d).
- [#115](https://github.com/factorio-access/FactorioAccess/issues/115),
  [#141](https://github.com/factorio-access/FactorioAccess/issues/141),
  [#250](https://github.com/factorio-access/FactorioAccess/issues/250) fast travel categories, local
  points, pairing with map pins: fast travel is gone (0b9b2f4d). The scanner lists the game's map
  tags (65783040), which is what #250 wanted.
- [#108](https://github.com/factorio-access/FactorioAccess/issues/108) elbow builder: dragging with
  the game's build key builds straight lines. Anything more belongs to the rail planner work.
- [#202](https://github.com/factorio-access/FactorioAccess/issues/202) (np) confirming belt
  junctions: building is vanilla; FA says what the preview highlights (ad29c3fc).
- [#339](https://github.com/factorio-access/FactorioAccess/issues/339) pulling Kruise Kontrol into
  the mod: upstream KK 2.1 instead.
- [#321](https://github.com/factorio-access/FactorioAccess/issues/321) API requests: the DLL reads
  what the Lua API lacks.
- [#157](https://github.com/factorio-access/FactorioAccess/issues/157) rate units,
  [#130](https://github.com/factorio-access/FactorioAccess/issues/130) (np) shorter phrases,
  [#312](https://github.com/factorio-access/FactorioAccess/issues/312) per-user presentation: native
  screens say the game's own text. Only FA's own views (belt analyzer and the like) could take
  options, as Mod settings.
- [#178](https://github.com/factorio-access/FactorioAccess/issues/178) (np),
  [#113](https://github.com/factorio-access/FactorioAccess/issues/113) (np),
  [#207](https://github.com/factorio-access/FactorioAccess/issues/207) (np) code cleanups: superseded
  by the UI rewrite and the move into the DLL.
- [#89](https://github.com/factorio-access/FactorioAccess/issues/89) (np) structure travel bug:
  structure travel no longer exists.
- [#75](https://github.com/factorio-access/FactorioAccess/issues/75) (np) mod mismatches in
  multiplayer, [#153](https://github.com/factorio-access/FactorioAccess/issues/153) (np) testing
  other mods: launcher era; mod windows are read generically now.

# Planned under the new architecture

- [#133](https://github.com/factorio-access/FactorioAccess/issues/133) cursor skip modes: skips moved
  into the DLL on `native-cursor` (e9f0dd07; stopping where the fog-of-war picture changes,
  b21296e1). New modes go there.
- [#90](https://github.com/factorio-access/FactorioAccess/issues/90) (np) ghost rail planner,
  [#245](https://github.com/factorio-access/FactorioAccess/issues/245) reachable rail ends: the rail
  planner plan, speaking the vanilla rail planner natively. #245 could be one of its readouts.
- [#136](https://github.com/factorio-access/FactorioAccess/issues/136) production overview: no
  native screen for the production statistics window yet. It is on the side menu.
- [#285](https://github.com/factorio-access/FactorioAccess/issues/285) space platforms: platform
  notices are spoken; the hub, the platform schedule and building on a platform need native screens
  or testing.
- [#304](https://github.com/factorio-access/FactorioAccess/issues/304) (np),
  [#305](https://github.com/factorio-access/FactorioAccess/issues/305) (np) train station tree and
  filters: a native screen for the game's Trains window comes first; its search may cover #305.
- [#257](https://github.com/factorio-access/FactorioAccess/issues/257) drawing audio rulers on the
  ground: done with the audio ruler tool (scripts/rulers.lua): 100 tiles along each arm for every
  player, and a short stretch where the character crosses a line further out. The tool also gives
  several rulers per player, which skips stop at by #242's rising-edge rule.

# Open, no plan

- [#96](https://github.com/factorio-access/FactorioAccess/issues/96) reselecting a blueprint's last
  area: the game's select new contents button may help, through the blueprint setup screen.
- [#203](https://github.com/factorio-access/FactorioAccess/issues/203) `SHIFT + K` from the bookmark:
  still the distance from the character.
- [#310](https://github.com/factorio-access/FactorioAccess/issues/310) Space Age solar panels: left
  as is.
- [#335](https://github.com/factorio-access/FactorioAccess/issues/335) mouse support for sighted
  developers: worth a fresh look now that clicks go through the game's own input.
- [#248](https://github.com/factorio-access/FactorioAccess/issues/248) a better trains tutorial
  (documentation).
- [#128](https://github.com/factorio-access/FactorioAccess/issues/128) (np) pipe fluid conflict
  analyzer.
- [#174](https://github.com/factorio-access/FactorioAccess/issues/174) (np) more accurate train track
  warnings.
- [#181](https://github.com/factorio-access/FactorioAccess/issues/181) (np) remnants.
- [#198](https://github.com/factorio-access/FactorioAccess/issues/198) (np) recorded tutorial
  keystrokes: the dev server's `/key` could replay them.
- [#218](https://github.com/factorio-access/FactorioAccess/issues/218) (np) map searcher,
  [#252](https://github.com/factorio-access/FactorioAccess/issues/252) (np) blueprint binding,
  [#306](https://github.com/factorio-access/FactorioAccess/issues/306) (np) accessibility quickbar.
- [#247](https://github.com/factorio-access/FactorioAccess/issues/247) (np) train building lookup
  tables, [#283](https://github.com/factorio-access/FactorioAccess/issues/283) (np)
  `physical_surface`: upstream code housekeeping, not checked.
