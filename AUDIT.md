# Diptych — Senior Audit

*Date: 2026-04-17. Scope: web game (`soul-searcher/`), mobile (same build, touch controls), XTeink X4 e-ink firmware port (`xteink-firmware/` snapshots + live tree at `~/Downloads/crosspoint-reader`). Deliverable only — no code changed.*

---

## Executive Summary

**Diptych is a more finished product than it looks and a more fragile codebase than it should be.** The three-chapter, 15-shard game is content-complete on both platforms, the writing and 1-bit aesthetic are distinctive, and the e-ink port is genuinely well-engineered (static allocation, correct dual-refresh model, 29% RAM headroom, clean Activity integration — `xteink-firmware/2026-04-13-full-port/`). The *biggest strength* is that the e-ink port isn't a toy: Ch1–Ch3, ghouls, plates, mirrors, and 8 easter eggs all work on-device.

The *biggest weaknesses* are structural: the web and e-ink versions are **total forks** — room layouts, sprites, dialogue, and shard coordinates have been manually re-typed into C++, with no generated data format. There is **no save state anywhere** (I can find no localStorage writes in the web, and the firmware calls `resetGame()` on every `onEnter()`). Dev cheat keys (`0` → skip to Ch2, `9` → skip to Ch3) ship to production, un-gated. The deploy pipeline pins React 18 in the standalone build while the source uses React 19, and `docs/index.html` has already drifted from `soul-searcher-standalone.html` (different MD5s as of 2026-04-17). None of these are hard to fix; all of them will bite.

---

## Top 10 Highest-Leverage Improvements

| # | What | Why | Effort | Risk | Where |
|---|---|---|---|---|---|
| 1 | **Add save state (web + firmware)** | Zero persistence on any platform. A mobile user who backgrounds the tab loses all progress. E-ink users lose progress on sleep/reset. Single largest retention killer. | M | Low | `soul-searcher/src/SoulSearcher.jsx:1086–1116` (all `useState`, no `useEffect(..., localStorage)`); firmware `DiptychActivity.cpp` `onEnter()` resets state |
| 2 | **Remove dev chapter-skip keys from prod** | Any player pressing `0` or `9` on the intro screen skips directly to Ch2/Ch3 with prior shards granted. Literally breaks the game. | S | Low | `SoulSearcher.jsx:1704–1713` |
| 3 | **Shared room-data format (JSON → both renderers)** | Web and C++ are total forks. Every room edit must be made twice. This is the #1 reason the port will bit-rot. Define one JSON schema, consume from both builds. | L | Med | Export path from one of the level editors (`tools/diptych-level-builder.html` already has "Generate C++ snippet" — extend to full room JSON + codegen) |
| 4 | **Fix build-standalone.sh: React 19, wire to npm, regenerate docs/** | Script pins `react@18` UMD while `package.json` ships `^19.2.4`. `docs/index.html` (the actual GitHub Pages artifact) has a different MD5 than `soul-searcher-standalone.html` — they've already drifted. No CI re-runs this. | S | Low | `soul-searcher/build-standalone.sh:16–17`; `package.json:13`; `docs/index.html` |
| 5 | **Tests around core rules (push, split, mirror, plates)** | 2,138-line single component with zero tests. Dual-world interdependence (plate in Light opens Door in Shadow) is exactly the kind of invariant that silently breaks. Node-runnable unit tests on the pure logic functions would take hours, not days. | M | Low | `SoulSearcher.jsx:1764–1776` (movement), `1778–1837` (split), `1887–1910` (push), `1980–1999` (mirror) |
| 6 | **Mobile: portrait lock + safe-area + bigger D-pad** | 48×48 buttons hit WCAG minimum, not comfortable thumb targets on 6"+ phones. No `viewport-fit=cover` → iPhone notch overlaps. No orientation handling → landscape squashes a 480×800 canvas into a sliver. | S | Low | `soul-searcher/index.html:6` (viewport meta); `SoulSearcher.jsx:2098` (dpadBtn sizing); standalone HTML viewport |
| 7 | **Drop one of the two level editors, document the survivor** | `level-editor.html` (root, 871 lines) and `tools/diptych-level-builder.html` (769 lines) both exist. Output formats overlap but aren't identical; only the `tools/` one emits C++ snippets. No doc says which is authoritative. | S | Low | `level-editor.html` vs `tools/diptych-level-builder.html`; `tools/README.md` |
| 8 | **Add a dirty-rect refresh to the firmware** | `DiptychActivity::render()` redraws the full 480×800 framebuffer every partial-refresh frame (both world panels, status bar, minimap, meters). It's fine for discrete moves but leaves faint trails on rapid input. Tracking the changed world + HUD regions and issuing partial refresh on only those costs maybe a day and meaningfully improves feel. | M | Low | `xteink-firmware/2026-04-13-full-port/activities-game/DiptychActivity.cpp` `render()` / `drawWorld()` around line 1990–2039 |
| 9 | **"Inkborne" residue cleanup** | Old repo name is still in the standalone HTML `<title>` ("Soul Searcher — Inkborne" in `build-standalone.sh:14`), in the git remote URL, in the soul-searcher README, and hanging around as `soul-searcher-dev/` + `soul-searcher-dev.zip`. Confuses the brand. | S | Low | `build-standalone.sh:14`; `.git/config`; `soul-searcher/`; delete `soul-searcher-dev/` + zip |
| 10 | **Play Ch1 front-to-back on the XTeink device and *watch* the refresh** | Static analysis is not enough. The whole question "does the e-ink experience actually work?" can only be answered by holding the device. Target: 10 minutes of real-hardware play, note every time ghosting or partial-refresh artifacts hurt comprehension. | S | Low | Device at `/dev/cu.usbmodem1101`; build/flash from `~/Downloads/crosspoint-reader` |

---

## Findings by Category

### 1. Cross-Platform Architecture

**Strength:** The firmware port is not bolted onto the web game — it's a ground-up C++ implementation that deliberately mirrors the web's game rules. Room coordinates, shard positions, split-step counts, ghoul AI all match. Static allocation throughout (`World light_`, `World shadow_` are stack-resident structs with inline `Tile tiles[GRID][GRID]` and fixed 8-entity arrays).

**🔴 Critical — No shared data format between platforms.** Every hand-crafted room (31 of them), every NPC dialogue string (including the 8 easter-egg signs), every shard coordinate is *manually transcribed* from `SoulSearcher.jsx` into C++ const arrays (e.g. `SHARDS_CH1[5]` in `DiptychActivity.cpp` ~line 48). The `2026-04-13-full-port` README acknowledges this in words ("line-by-line transcribed"). There is no codegen, no shared JSON, no validation that the two worlds agree. Any content change silently diverges. This will bite the next chapter/room added.

> **Fix:** Define `rooms.json` with the room layout + entities in one place. In the web build, import it at module load. In the firmware build, write a ~80-line Python script that reads the JSON and emits a `rooms_gen.h` with the same const structures already used in C++. Cost: ~1 day. Payoff: permanent.

**🟠 Major — `soul-searcher-dev/` is a second, abandoned fork.** `soul_searcher.jsx` in that folder is a 91KB separate file with different constants (`ROOM_COLS=18`) and a different sprite set. No README, no explanation. Either it's dead (delete) or it's a branch someone forgot about (merge or note).

**🟡 Minor — "clean core engine layer" does not exist on web side.** Rendering, input, state, room data, and sprites are all in one component. The firmware side is slightly better factored (drawing helpers split out), but also monolithic within one `.cpp`. That's fine at this scale; it becomes a problem the moment you want a second-front (e.g. native desktop build). Revisit at Ch4.

---

### 2. E-Ink Adaptation (highest priority)

**Strength:** The refresh model is right. `DiptychActivity` uses `HalDisplay::FAST_REFRESH` on every gameplay update (fast partial, ~300ms, tolerates ghosting) and sets `needsFullRefresh_ = true` only on screen transitions, room changes, and modal opens (`onEnter()` at ~line 238; `loadRoom()` at ~line 1123; dialogue/victory transitions at lines 1713, 1723, 1756, 1784). This is the right discipline — full refresh where the picture changes completely, partial everywhere else. No animation timers, no frame-rate assumptions, no idle redraws. `preventAutoSleep()` correctly returns true only during `Screen::Play`, so sleep works normally outside gameplay.

**Strength:** Memory is disciplined. 29.2% RAM used (95KB of 327KB), no heap allocation in the hot path, `std::string` and `std::vector<std::string>` only for dialogue buffers that are cleared and reused. `std::initializer_list` used cleanly for wall/door/water placement.

**🟠 Major — No dirty-rect culling.** Every partial refresh redraws the whole 480×800 framebuffer (both worlds, status bar, shard rows, minimap, split/mirror meters). On an e-ink panel that's already doing a partial-mode refresh, this is *not* a correctness bug — but it does mean that when the only pixel that changed was the player sprite, the whole panel is being rewritten. That's more work for the controller, more opportunity for faint ghosting on fast input, and more battery cost. For a movement-driven game, tracking "the world the player is in + the HUD row" as the dirty region and only updating that is a ~1-day win.

**🟠 Major — Flash is 88.9% full (5.83 MB of 6.55 MB).** This is the one hard resource ceiling. Every new sprite, every new room, every new dialogue line costs flash. The cushion is ~700 KB. One Ch4 could eat it. Options: strip unused games from the suite; compress sprites; move dialogue to a lookup table that can be shared with web. This is monitoring-not-blocking, but track it now before it's an emergency.

**🟡 Minor — Procedural rooms (filler) use `xorshift32` while web uses `mulberry32`.** Documented, but means "walking into a non-hand-crafted room" is a different experience on the two platforms. Low impact because all *content* rooms are hand-crafted, but a user who compares will notice.

**🔴 Critical (assumption) — No save between device sleeps.** I can't find a `Prefs.put…`-style persistence call in `DiptychActivity.cpp`; `onEnter()` calls `resetGame()`. On a device whose whole value prop is "pick it up whenever," losing progress on sleep is wrong. **Flag as Q1 below** — verify with the user before coding a fix.

---

### 3. Web & Mobile Feel

**Strength:** Touch controls exist and are correctly wired. `SoulSearcher.jsx:2098–2132` adds a 48px D-pad, 44px Split buttons, 72px Act button, all with `touchAction:"manipulation"` and `WebkitTapHighlightColor:"transparent"`. Click handlers route through `triggerKey()` so the same input path handles keyboard and touch.

**🟠 Major — Mobile polish is superficial.**
- **No safe-area handling.** `soul-searcher/index.html:6` has `viewport-fit=cover` *absent*; no `env(safe-area-inset-*)` anywhere. iPhone notches and home indicators will overlap the UI.
- **No orientation handling.** The canvas is 480×800 portrait. In landscape, the layout still centers but the D-pad+Act row falls beneath a drastically scaled-down canvas. Either CSS-lock to portrait or rebuild the control layout for landscape.
- **D-pad at 48px is the floor.** Apple HIG target is 44pt ≈ 44 CSS px, but *comfortable* phone play is 56–60 px. Easy to bump.
- **No haptic feedback.** E-ink's partial-refresh slowness on the device is offset by the click-feel of real buttons. On mobile, there are no real buttons and no `navigator.vibrate()`. Every "I pushed a thing" moment is silent and unverified.
- **No swipe input.** A 4-directional game on a touchscreen is the textbook swipe case.

**🟡 Minor — First paint on mobile.** `soul-searcher-standalone.html` is 246 KB (not split, no lazy loading, CDN-loaded Babel + React UMD — so a cold mobile connection fetches `unpkg.com` twice). Fine on wifi, painful on spotty 4G. Once you kill the CDN deps and ship a real Vite bundle, this goes away.

**🟡 Minor — Web experience is a canvas rectangle in a dark frame.** The outer chrome (`background:"#222", padding:"14px", borderRadius:"16px"`) is generic. On desktop, the canvas sits lonely on a black page. Nothing *wrong*, but nothing distinctive either. Consider a sparse landing page or parchment-textured backdrop that matches the cream palette of the game.

---

### 4. Core Gameplay

**Strength:** The core loop is tight and the mechanics layer well. Ch1 introduces split-mode and push; Ch2 adds pressure plates with cross-world linkage; Ch3 adds mirror-inverted movement. Each mechanic is a single new rule that composes with everything prior. This is classic Zelda-box puzzle design and it works.

**Strength:** Writing. The Watcher, the Sage, the graveyard epitaphs, `@JLUDDY7` signature in the headstone — the Souls/Bloodborne voice is consistent and sparse. Nothing explains itself. That's the correct register for this game.

**🔴 Critical — No save state.** Covered above. On web, `SoulSearcher.jsx:1086–1116` declares 30+ `useState` hooks and none of them are mirrored to `localStorage`. Closing the tab resets everything. On mobile this is ~inevitable.

**🔴 Critical — Dev keys in production.** `SoulSearcher.jsx:1704–1713`:
```jsx
// DEV: press 0 to skip to Chapter 2, press 9 to skip to Chapter 3
if (key === "0") {
  setShards(new Set(["shard_0", ...])); setChapter(2); setGs("play"); ...
}
if (key === "9") {
  setShards(new Set([...])); setVoidShards(new Set([...])); setChapter(3); ...
}
```
No `import.meta.env.DEV` guard. Any player who touches `0` on the intro screen leapfrogs the entire Ch1 design. Wrap in `if (import.meta.env.DEV)` or require a query string (`?dev=1`) before registering the handlers.

**🟠 Major — No chapter-complete "beat."** Victory screens exist (`SoulSearcher.jsx:1190–1232`) and the dialogue does announce the transition, but there's no sound, no distinct screen state, no "save point" ritual. For a 15-shard arc, the player should *feel* crossing the threshold into Ch2. On e-ink this is even more important — absent sound, the screen change has to carry the whole moment.

**🟡 Minor — Difficulty curve depends on the 5-step split budget.** The 5-step limit (`SPLIT_STEPS=5`) is baked in. If late-Ch3 puzzles need more distance, the game silently enforces "fail and reset." Consider whether that's the intended lesson or just a constraint.

---

### 5. Code Quality

**Strength (firmware):** Clean C++11. Constants in an anonymous namespace. Enums for Screen/Tile/EntityType instead of magic ints. `constexpr` where appropriate. Bounds checked on every grid access (`inside(x,y)`). Exactly one `const_cast` in `filterCollectedShardsFromWorld()` at ~line 924 and it's justified (side-effect filter).

**🟠 Major — `SoulSearcher.jsx` is one 2,138-line component with no tests and no types.** `handleKey` is ~400 lines; `render` is ~900. The interdependent rules (pressure plates in Shadow open Doors in Light only while weighted; mirror mode snaps back on expiration; ghouls tick only in the split-active world) are exactly the shape of bug surface where a silent regression costs a chapter.

**🟠 Major — No linting enforced in CI.** `eslint.config.js` exists, `package.json` has a `lint` script, but no GitHub Action runs it. Unused variables, missing dependency arrays in `useEffect`, dead branches — all invisible.

**🟡 Minor — Cryptic locals.** `gs`, `lp`, `sp`, `lOk`, `sOk` scattered through `handleKey`. Fine if you wrote it last week. Not fine if you come back in 3 months.

**🟡 Minor — Magic numbers in the web code.** `DOOR_LO=5, DOOR_HI=9` (line ~17) determines doorway snap logic. Fine, but the constant is used implicitly in more than one spot. Worth naming the invariant rather than the range.

**🔵 Nitpick — `soul-searcher-dev/` and `soul-searcher-dev.zip` sit at repo root doing nothing.** Delete.

---

### 6. UI & Visual Design

**Strength:** 1-bit aesthetic is intentional, not default. Pure `#000` / `#f4f1e8` (cream), Courier New monospace, 24×24 pixel sprites with `imageRendering:"pixelated"`. It feels like an *art direction*, not a limitation. This is rare and hard to do accidentally.

**Strength:** Full state coverage. Title, controls, gameplay, shard collection modal ("A PIECE RETURNS"), ghoul catch modal with heart display, chapter transitions, game-over — all present (`SoulSearcher.jsx` renders each at ~1139, 1159, 1189, 1529, 1544, 1190, 1682). Nothing feels like an unfinished state.

**🟡 Minor — Typography leans heavily on Courier New.** It's the whole typographic voice. Works for an ink-like 1-bit game, but consider whether a display face (even just for the word "DIPTYCH" on the title screen) would give the hinge-moment a stronger silhouette on e-ink's limited gradation.

**🟡 Minor — Desktop framing is generic dark-panel-in-dark-page.** See §3.

**🔵 Nitpick — The outer page background is `#121212`, inside a `#222` panel, with a `#2a2a2a` cell inside.** Three nearly-identical near-blacks. Pick two.

---

### 7. UX & Interaction

**Strength:** Onboarding works without a tutorial wall. (0,0) Watcher → (1,0) Split Light sign → (2,0) Split Shadow sign → (3,0) wall. A player can't get lost in the first 30 seconds.

**🟠 Major — Feedback on action is text-only.** `setMsg("They want to be one. Step onto them.")` is how the game tells you anything. On web, the message renders in 13px Courier New above the play area. On e-ink, that same text on a slow-refresh panel needs to be *large* and *persistent*. Review the font-size ladder on device — what's legible on a laptop at 100% is not necessarily legible on 4.7" e-ink.

**🟠 Major — Pause semantics are implicit.** There's no pause. Ghouls only advance on player movement, so "standing still" is effectively paused — fine design, *but* no UI affordance communicates this. Players conditioned on action games will panic.

**🟡 Minor — No undo.** A puzzle game with pushing and limited step budgets without undo is a rage quit waiting to happen. Even one-step undo (Z) would soften the curve.

---

### 8. Performance

**Strength (web):** No `requestAnimationFrame` loop. Render is `useEffect(() => render(), [render])` with `render` wrapped in `useCallback` — so the canvas redraws only when state changes. For a turn-based 1-bit game this is correct. Idle tabs cost nothing.

**Strength (firmware):** Covered in §2. Static allocation, no heap in hot path, 29% RAM.

**🟡 Minor — The standalone HTML pulls React, React-DOM, and Babel from `unpkg.com` via three separate `<script>` tags** (`build-standalone.sh:16–18`). A cold-start on a bad connection is three CDN round-trips plus a JIT-transpile of ~90KB of JSX at boot. The fix is to ship the Vite-built bundle (already in `dist/`) inlined, not the JSX source. This is also what would let you drop Babel entirely.

**🟡 Minor — Render clears and redraws the whole canvas on each state change.** Fine for 480×800 at low frequency. Irrelevant until you add animation or larger rooms.

---

### 9. Product & Strategic Direction

**Strength:** *The product is not half-built.* Three chapters, 15 shards, 8 easter eggs, 31 hand-crafted rooms, a bounded world with sealed edges, consistent voice, both platforms running the same content. DIPTYCH.md describes this as `Currently deployed` and it's true. This is a *finished* indie puzzle game in need of polish, not a vertical slice in need of completion.

**🔴 Critical — The e-ink-first positioning is aspirational, not realized in the codebase.** The web is the source of truth (Vite project, npm scripts, deploy pipeline); the e-ink is a downstream port. If the pitch is "this game is meant for e-ink," the architecture should reflect that — at minimum a shared data format (see §1 / Top-10 #3), and honestly, a set of design choices in the web that *sacrifice web polish to match e-ink constraints* (no smooth scroll, no hover states, harder-to-read small text replaced by larger). Right now the web is the "best" version and e-ink is the port. If that's intentional (web is the product, e-ink is the marketing hook), say so. If not, invert the gravity.

**🟠 Major — Two level editors is one too many.** `level-editor.html` (root, 871 lines) and `tools/diptych-level-builder.html` (769 lines) overlap. Pick the one with C++ emit (`tools/`), delete the other. Document how to use it (`tools/README.md` exists — expand it).

**🟠 Major — No analytics, no telemetry, no way to know if anyone plays.** For a finished game about to sit on a GitHub Pages URL, a single "session started" ping (or even just checking GH Pages traffic) would tell you whether to invest another 40 hours of polish. Not asking for invasive tracking — asking for a thermometer.

**🟡 Minor — Old-name residue.** "Soul Searcher," "Inkborne" still appear in title tags, folder names, and the git remote. Pick the name (Diptych) and go.

---

## Quick Wins (under 30 min each)

1. **Gate dev keys with `import.meta.env.DEV`** — 3 lines, `SoulSearcher.jsx:1704–1713`. Stops shipping a cheat code.
2. **Update `build-standalone.sh` to pin `react@19`** — one edit, `build-standalone.sh:16–17`. Removes a latent version-skew bug.
3. **Rebuild `docs/index.html` from current source and commit** — the deployed artifact is drifting. Just run the build once.
4. **Delete `soul-searcher-dev/` and `soul-searcher-dev.zip`** — the abandoned fork. Confirm with `git log -- soul-searcher-dev/` first.
5. **Fix the `<title>` in the standalone build from "Soul Searcher — Inkborne" to "Diptych"** — `build-standalone.sh:14`.
6. **Add `viewport-fit=cover` to `soul-searcher/index.html:6`** — one meta tag, fixes iPhone notch overlap.
7. **Delete one level editor** — `level-editor.html` is the redundant one (`tools/diptych-level-builder.html` emits C++, which is more useful).
8. **Add a `lint` step to a minimal GitHub Action** — even just `run: npm run lint` on push. Catches silent regressions.

---

## Longer-Term Bets

1. **Generated room data.** `rooms.json` as the single source of truth; web imports it; firmware build has a ~80-line codegen script that emits `rooms_gen.h`. Cost: ~1 day. Eliminates the #1 structural risk. If you only do one thing from this audit beyond the Top 10, do this.

2. **Save state (persistent + resumable).** Web: `useEffect` → `localStorage.setItem('diptych-save', JSON.stringify({chapter, shards, voidShards, echoShards, roomCoord, hp}))`. Firmware: device has NVS; call `Prefs.put*` from `onPause()` and restore in `onEnter()`. Cost: 1–2 days across both. Transforms mobile playability from "single sitting only" to "real."

3. **Audio on web/mobile.** The game's voice and pacing read like it *should* have sound — slow wind, a chime on shard collection, a heartbeat in the ghoul rooms. 10–15 short samples, Howler.js or bare `AudioContext`. Dramatically raises production value for low effort.

4. **Native desktop build of the port.** The C++ port is clean enough that a desktop wrapper (SDL2 or raylib framebuffer) would let you publish a second "1-bit retro" version to itch.io. Reuses all gameplay code. Expands audience beyond "people who own an XTeink X4."

5. **Compress the firmware suite or strip unused games.** Flash is at 88.9%. Diptych is the centerpiece; the 12 other games (Caro, Chess, DeepMines, Minesweeper, …) are nice-to-haves. Shipping a "Diptych Device" image that omits the rest buys ~1 MB and future-proofs Ch4.

6. **Design e-ink-native pacing.** Right now the e-ink port mirrors the web pacing. E-ink's 300ms partial refresh *invites* a more meditative design: longer pauses between moves, dialogue that sets type once and lingers, a "breathing" animation (full-refresh every N moves as an aesthetic, not just a ghosting mitigation). This is a design project, not just engineering.

---

## Questions for Me (the auditor, of you)

Please answer these before the next pass — some findings change depending on intent.

1. **Is there save state on the firmware that I missed?** I grep'd `DiptychActivity.cpp` for `Prefs`, `nvs`, `save`, `load`, `Preferences` — nothing obvious. Assumed no. If there *is* and I missed it, tell me where.

2. **Is the XTeink port the product, or a demo?** If the device ships with Diptych and that's the business, then the e-ink-first inversion in §9 matters a lot and the web is effectively marketing. If the web is the product and the device is a portfolio piece, the priorities flip.

3. **Is `soul-searcher-dev/` a dead experiment or an active branch?** I'm going to recommend deleting it, but want to confirm.

4. **What's the target player for the web version?** "Indie puzzle nerds who find it on itch"? "Friends you send the URL to"? Neither requires much; a broader launch audience would raise the bar on mobile polish, analytics, and save state.

5. **Are the dev chapter-skip keys (`0`, `9`) intended to stay for playtesters?** If yes, gate them behind `?dev=1` rather than removing. If no, remove.

6. **Is a Ch4 / post-Ch3 content planned?** Flash headroom (§2) and room-data drift (§1) both get worse with more content. If you're done, neither is urgent. If Ch4 is coming, both jump up the list.

7. **"Inkborne" → "Diptych" — has the rename finished in your head, or is it still in motion?** If finished, I'll recommend a branding sweep. If in motion, I'll wait.

---

*End of audit.*
