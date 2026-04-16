# Diptych tools

## `diptych-level-builder.html`

A standalone browser room editor you can use to design custom Diptych puzzle rooms **without modifying the game runtime**.

### What it does

- Edits both 15×15 worlds (Light + Shadow) side-by-side
- Paints tiles (`empty`, `wall`, `tree`, `water`, `door`, `switch`, `mirror`)
- Paints entities (`npc`, `sign`, `half-light`, `half-shadow`, `ghoul`)
- Cell inspector for advanced authoring metadata:
  - Switch behavior (`hold` momentary vs `latched` one-time permanent)
  - Switch target world and specific door coordinates to open
  - NPC id/name, dialogue lines, and response options
- Exports/imports a JSON room file for iteration
- Generates a C++ snippet compatible with `DiptychActivity` helper methods, plus metadata comments for switch/NPC wiring

### How to use

1. Open `tools/diptych-level-builder.html` in any browser.
2. Build the room in Light and Shadow panels.
3. Click a cell to configure switch links or NPC dialogue/response data.
4. Click **Export JSON** to save your room draft.
5. Click **Generate C++ snippet** when you want to manually port the room into a branch.

This keeps your design workflow separate from the game code and lets you "build" levels Mario-Maker style before integrating them.
