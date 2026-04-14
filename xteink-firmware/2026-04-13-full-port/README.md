# 2026-04-13 full-port snapshot

The first full-parity port of Diptych onto the XTeink X4. This snapshot is what's currently flashed to the device.

## What changed since the 2026-04-12 baseline

The Apr 10 build (preserved in `../2026-04-12-baseline/`) was a Chapter 1 prototype — 5 shards, basic light/shadow split, no extra mechanics. This snapshot ports the full web game across.

### Game changes

- **3 chapters, 15 shards** (was: 1 chapter, 5 shards)
  - Chapter I — The Surface Rift (5 shards, push puzzles)
  - Chapter II — The Wound Beneath (5 void shards, pressure plate / door puzzles)
  - Chapter III — The Hinge (5 echo shards, mirror-mode puzzles)
- **9×9 bounded world** with sealed boundary walls. Trying to walk off the edge of the world hits a wall.
- **31 hand-crafted rooms** + procedural fallback for the rest:
  - Tutorial (Watcher, Split Light, Split Shadow, Wall, Sage)
  - 3 ghoul encounters (Ambush + 2 patrol rooms)
  - 5 Ch1 push puzzles, 5 Ch2 plate puzzles, 5 Ch3 mirror puzzles
  - 8 easter egg rooms at the corners and edges (Graveyard with the @JLUDDY7 headstone, First Diptych, Empty Watcher, Bell, Sunken Shrine, Two Wells, Still Pool, Reflected Grave)
- **New tile types**: Water, Door, Switch (pressure plate), Mirror — with renderers for each
- **New entity types**: Ghoul (with Manhattan-priority chase AI), additional sign sprite variants (Headstone, Bell, MirrorEye, PlayerLight/Shadow)
- **Pressure plates**: plates in one world open doors in the other. Linked-plate rooms (Ch3) require both plates pressed simultaneously to latch the doors.
- **Mirror mode**: stepping on a Mirror tile gives 5 inverted-movement steps. Shadow walks opposite to light. Ends with a snap-back to the mirror anchor.
- **Hearts (HP)**: 5 hearts. Ghoul contact costs a heart and shows a flavor-text catch modal. Losing all 5 resets the current chapter's progress and respawns at the Watcher.
- **Title + Controls** intro screens (replaces the single-screen intro)
- **Three-tier victory screen** (Ch1 → Ch2 → Ch3 → final)
- **Watcher and Sage dialogue** rewritten to chapter- and shard-count-aware Bloodborne voice with directional shard hints
- **All 30+ easter egg sign texts** ported including the @JLUDDY7 graveyard signature
- **Visited-room minimap** that shows current room, home, and unclaimed shards in surrounding 7×7 area

### Code changes

| File | Before | After |
|---|---|---|
| `DiptychActivity.cpp` | 938 lines | 2392 lines |
| `DiptychActivity.h` | 123 lines | 266 lines |
| **Total** | **1061** | **2658** |

The state struct expanded with chapter tracking, hearts, three shard masks, mirror mode anchors, door-latch flags, linked-plate flag, visited-room bitmap, and a CaughtKind enum for the ghoul catch modal. `MAX_ENTITIES` was bumped from 4 to 8 to fit the 6-headstone graveyard rooms.

### Source of truth for the port

The C++ port mirrors `soul-searcher/src/SoulSearcher.jsx` from the parent Diptych repo (~2086 lines of React/JS). All room layouts, dialogue, pickup lines, and easter eggs were transcribed line-by-line from that file. The PRNG used for procedural rooms is xorshift32 instead of the JS mulberry32, so procedural rooms won't match the web game pixel-for-pixel — but the hand-crafted rooms (tutorial, shard puzzles, easter eggs) do.

### Sprites that are intentionally *not* matched

The C++ port draws sprites procedurally via `renderer.fillRect`/`drawLine`/`drawDiamond` instead of the pixel-string bitmaps the web game uses. The intent matches but the exact pixel art is hand-translated:

- Player, NPC, Wall, Tree, Sign — visually equivalent to the web sprites
- Ghoul, Door, Plate, Mirror, Headstone, Bell, Water — new procedural drawings, designed to read clearly at 24×24 on e-ink with the same role as the web equivalents

## Build details

- Built from `/Users/james/Downloads/crosspoint-reader` against upstream commit `1df543d` on `crosspoint-reader/master`
- `pio run -e default` succeeded in 11.45 seconds
- RAM: 29.2% used (95,732 / 327,680 bytes)
- Flash: 88.9% used (5,828,329 / 6,553,600 bytes)
- `pio run -e default -t upload --upload-port /dev/cu.usbmodem1101` flashed in 51.89 s
- Hash of data verified by esptool after upload

## Contents

- `firmware.bin` — flashed ESP32-C3 image (5.8 MB), SHA256 `13bc36c530f2a7f9d7535d90ba84da9a322477038d2fddb37c075d0f5c9cc4fa`
- `SHA256SUMS` — checksums for everything in this folder
- `activities-game/` — every `.cpp`/`.h` in `src/activities/game/` at time of snapshot. Diptych is the only file changed from the previous snapshot; the rest of the games suite is unchanged
- `game/` — every `.cpp`/`.h` in `src/game/` (shared infrastructure, unchanged)

## Known caveats

- This is a first-pass port. The build links cleanly and runs (5.8 MB image flashed and verified), but the game has not yet been fully playtested on the device — bugs and visual issues are expected. Report problems against `DiptychActivity.cpp` in this snapshot.
- Procedural rooms use a different PRNG than the web version (xorshift32 vs mulberry32) so room layouts in the procedural fill area will differ. The hand-crafted rooms (tutorial / Ch1 / Ch2 / Ch3 / easter eggs) match exactly.
- The 8 easter egg signs are blocking-style (you read them from an adjacent tile, like NPCs). The web version's "step into the headstone to trigger" is approximated this way.
- Mirror mode only activates from coupled walking; once active, room transitions are blocked until the 5 inverted breaths are spent or you toggle out via the split buttons.
- `firmware.elf` and `firmware.map` are excluded for size — locally available at `/Users/james/Downloads/crosspoint-reader/.pio/build/default/`.
