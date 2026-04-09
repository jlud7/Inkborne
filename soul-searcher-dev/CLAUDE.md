# Soul Searcher — E-Ink RPG for Xteink X4

## What This Is

A turn-based RPG built for the Xteink X4 e-ink reader (480×800, 1-bit black/white, 5 physical buttons, ESP32-C3). Currently a React prototype that simulates the exact display. The prototype will be iterated on in browser, then ported to C++ as a CrossPoint Reader firmware activity.

## Current State

`soul_searcher.jsx` is a working React component (~1170 lines) with:
- Intro sequence ("You died." line by line, black screen)
- Room-based overworld (18×18 tiles per room, deterministic generation)
- 7 demon types with unique dialogue, 4 approaches per encounter
- Encounter screen with portrait, dialogue, 4 choices
- Journal tracking encounters and player tendency
- Home base (your grave) with candle lore tracker, boss trophies, stats
- ESC = "close eyes" instant teleport home and back
- Minimap showing explored rooms
- Keeper NPC with progressive dialogue
- Lore fragment collection (7 needed to open gate)
- Area 2 (Ashen Forest) with dead trees, new demons, boss fight
- Soul collection → shrine delivery → resolve restoration loop

## Hardware Constraints (Xteink X4)

- **Display**: 480×800 pixels, 1-bit (black or white), e-ink
- **E-ink refresh**: Full screen refresh is slow (~300ms) and causes visible flash. MINIMIZE full refreshes. Room-based design means background is static, only player sprite position changes per move. Room transitions = "page turns" = acceptable full refresh.
- **Controls**: 5 buttons only — Up, Down, Left, Right, Confirm (Select). No touchscreen.
- **RAM**: ~380KB total on ESP32-C3. No large buffers. Sprites are drawn pixel-by-pixel.
- **Storage**: SD card for save files, assets cached to `/.crosspoint/`

## Design Philosophy

- **Every screen should look like a page in a book.** Static, beautiful, crisp. The player studies it, thinks, then acts.
- **Minimize pixel change per input.** Within a room, only the player moves. Background is completely static. This is critical for e-ink — scrolling the whole screen every step is unacceptable.
- **Turn-based, no animation.** Each button press = one discrete state change = one targeted screen update.
- **Combat is philosophical, not mechanical.** Four approaches (Confront, Comprehend, Absolve, Endure) aren't rock-paper-scissors — they're expressions of character that shape the story differently.
- **No grinding.** If a fight isn't interesting, it shouldn't exist. Every demon is visible, avoidable, and placed deliberately.

## What Needs Work (Priority Order)

### 1. Sprite Redesign (all 24×24, 1-bit)
Every sprite needs to be redrawn for maximum readability and visual impact at small size. Guidelines:
- **Player**: Cloaked wanderer with visible staff or lantern. Hood creates distinctive silhouette. Should feel like the protagonist.
- **Soul**: Ethereal wisp, peaceful closed eyes, body trails downward into dissolving wisps. Delicate and floaty.
- **Shrine**: Tall stone monument with flame/orb at top, decorative band, wide stepped base. MUST look architectural, NOT like a tree. This was a recurring problem.
- **Grave**: Rounded headstone with cross symbol, rectangular body, dirt mound at base. Short and squat.
- **Keeper**: Aged robed figure, slightly hunched, long robes. Distinguished from player by posture and width.
- **Tree**: Full round dense canopy, clear trunk. Classic tree shape.
- **Dead tree**: Bare skeletal branches reaching upward like fingers. Thin trunk. Clearly dead.
- **Rock**: Rounded boulder with surface crack details.
- **Wrathful**: Horned, jagged flame-like edges, aggressive wide stance, arms/claws reaching.
- **Grieving**: Hunched and small, head bowed, tear-drops falling below. Compressed posture.
- **Deceitful**: Two overlapping faces/masks (one looking each way). Unsettling.
- **Prideful**: Tall crowned figure, broad shoulders, looking downward. Regal and imposing.
- **Forgotten**: Scattered dissolving pixels, barely cohesive human outline. Parts missing.
- **Hollow**: Empty outline of a person — just the border pixels, void interior.
- **Ancient**: Massive, fills most of the tile. Large head with halo/crown rings. Overwhelming.
- **Devourer (Boss)**: Wide consuming maw, body widening downward, tendrils. Largest presence.
- **Gate**: Clear stone archway with pillars and arch.

### 2. UI Polish
- **Room border**: Double-line ornamental frame around the play area, decorative corners
- **Header**: Area name styled with decorative dividers, not just plain text
- **Encounter screen**: Demon portrait at 5× scale with a decorative frame. Dialogue in a proper bordered panel. Choices with clearer visual weight — selected choice should be unmistakable.
- **Home screen**: Grave drawn with surrounding ground detail. Trophies displayed as framed sprites. Candles should actually look like candles with visible wax and flame shapes.
- **Minimap**: Proper border, small legend showing what symbols mean. Current room more prominent.
- **HUD status panel**: Consider small icons instead of just text labels. Potions as filled circles. Resolve bar with end caps.
- **Message area**: Consider a subtle border/frame. Text should be easy to read.
- **Controls bar**: Consistent, clean, at the very bottom.

### 3. Gameplay Expansion
- More rooms with hand-placed features (not just random trees)
- Area 2 needs its own demon types with full dialogue pools (Ashen Walker, The Bound One already defined in DEMONS object but need area-2 room generation to spawn them)
- Save/load state to localStorage (for browser) or SD card (for hardware)
- Sound design placeholder hooks (for future e-ink piezo buzzer)

## Architecture

### Room System
World is an infinite grid of rooms at coordinates `(rx, ry)`. Each room is generated deterministically from a seeded RNG (`mulberry32(seed + rx * 1000 + ry)`), so rooms are consistent when revisited. Home is always `(0,0)`. Gate to Area 2 is at `(2,2)`.

### Entity Types
`grave`, `soul`, `shrine`, `keeper`, `gate`, `demon` (with subtypes: wrathful, grieving, deceitful, prideful, forgotten, hollow, ancient, devourer)

### Game States
`intro` → `overworld` → `encounter` (pushed from overworld) → back to `overworld`
Overlays: `homeView`, `keeperMsg`, `jOpen` (journal), `area2Msg`

### Input Mapping (X4 buttons → keyboard)
| X4 Button | Keyboard | Function |
|-----------|----------|----------|
| Left button (Menu) | ESC | Close eyes / go home |
| Rocker Up | ↑ | Move up / scroll up |
| Rocker Down | ↓ | Move down / scroll down |
| Left | ← | Move left |
| Right | → | Move right |
| Confirm | Enter | Interact / select |
| - | J | Open journal |

### Key State Variables
- `roomX, roomY`: Current room coordinates
- `plX, plY`: Player position within current room (0-17)
- `resolve`: 0-100, depleted by encounters, restored by guiding souls to shrines
- `held`: Souls currently carried
- `guided`: Total souls guided to rest
- `loreN`: Lore fragments found (7 needed for gate)
- `banished`: Set of entity IDs that have been defeated/collected
- `visited`: Set of room coordinate strings for minimap
- `tend`: Object tracking which approaches the player favors
- `bossTrophies`: Array of defeated boss sprites for home display

## Story Summary

You died. You woke up at your own grave — which is unusual. Most souls just wander and fade. A figure called the Keeper explains: you're a Searcher. You can see lost souls and guide them to shrines where they find rest. But something is wrong — souls aren't just getting lost, they're being held. Pulled downward. The demons in the Sacred Meadow carry fragments of truth about what's happening. Each one you face and overcome reveals a piece of the story. When you've gathered enough truth (7 lore fragments from 7 demon types), a gate opens to the Ashen Forest, where the roots go deeper. There, you face The Devourer — not evil, but a starving wound in the world. A hunger older than sin that eats what souls are made of.

## File Structure
```
soul-searcher-dev/
  CLAUDE.md          ← This file
  soul_searcher.jsx  ← Complete React prototype
```

## Running the Prototype
This is a React component. To test it:
1. Create a Vite + React project: `npm create vite@latest soul-searcher -- --template react`
2. Replace `src/App.jsx` with the contents of `soul_searcher.jsx` (or import it)
3. `npm run dev` and open in browser
4. Click the canvas area to give it keyboard focus, then use arrow keys + Enter

Or paste the component into any React sandbox (CodeSandbox, StackBlitz, etc).

## Future: Porting to CrossPoint C++
When the prototype is finalized, it gets ported as a `SoulSearcherActivity` in the CrossPoint firmware. The activity follows the same pattern as the existing game activities (Tetris, etc.) — extends `Activity`, implements `onEnter/onExit/loop/render`. Sprites become `const uint8_t[]` arrays. Room generation uses the same seeded RNG. State is saved to SD card as binary. The `GfxRenderer` API has `drawPixel`, `fillRect`, `drawRect`, `drawBitmap` — everything needed.

## Book Integration (Optional, Light)
If EPUBs exist on the device, occasionally a soul whispers a sentence from one of the user's books instead of a default whisper. Tagged with `[from your library]`. No gameplay dependency. If no books, default whispers are used. Implementation: CrossPoint's `Epub` library can extract page text (same method used for the page highlight feature).
