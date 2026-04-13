import { useState, useEffect, useCallback, useRef } from "react";

// ── Constants ───────────────────────────────────────────────────
const W = 480, H = 800;
const TILE = 24;
const GRID = 15;
const VIEW_W = GRID * TILE;
const VIEW_H = GRID * TILE;
const VIEW_OX = (W - VIEW_W) / 2;
const LIGHT_OY = 24;
const SHADOW_OY = 408;
const STATUS_Y = 772;
const BLK = "#000", WHT = "#f4f1e8";
const T = { EMPTY: 0, WALL: 1, TREE: 2, WATER: 3, DOOR: 4, SWITCH: 5, MIRROR: 6 };
const TOTAL_SHARDS = 5;
const SPLIT_STEPS = 5;
const DOOR_LO = 5, DOOR_HI = 9; // doorway range on each edge
const GHOUL_DMG = 1;
const MAX_HEARTS = 5;
const MIRROR_STEPS = 5;

// ── Utilities ───────────────────────────────────────────────────
function mulberry32(a) {
  return function () {
    a |= 0; a = a + 0x6D2B79F5 | 0;
    var t = Math.imul(a ^ a >>> 15, 1 | a);
    t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t;
    return ((t ^ t >>> 14) >>> 0) / 4294967296;
  };
}
function isWalkable(tile, doorsOpen) {
  return tile === T.EMPTY || tile === T.SWITCH || tile === T.MIRROR || (tile === T.DOOR && doorsOpen);
}
const snap = v => Math.max(DOOR_LO, Math.min(DOOR_HI, v));
function drawPixelDiamond(ctx, cx, cy, r, filled) {
  for (let dy = -r; dy <= r; dy++) {
    const w = r - Math.abs(dy);
    if (filled) ctx.fillRect(cx - w, cy + dy, w * 2 + 1, 1);
    else {
      ctx.fillRect(cx - w, cy + dy, 1, 1);
      if (w > 0) ctx.fillRect(cx + w, cy + dy, 1, 1);
    }
  }
}

// ── 24x24 Sprites ───────────────────────────────────────────────
const S = {};
const def = (name, rows) => { S[name] = rows; };

def("player", [
  "000000000011110000000000","000000001111111100000000","000000011111111110000000",
  "000000011111111110000000","000000011111111110000000","000000001111111100000000",
  "000000000111111000000000","000001111111111111100000","000011111111111111110000",
  "000111111111111111111000","000111111111111111111000","000011111111111111110000",
  "000001111111111111100000","000000111111111111000000","000000111111111111000000",
  "000000011111111110000000","000000011111111110000000","000000111100001111000000",
  "000000111000000111000000","000001110000000011100000","000001110000000011100000",
  "000011100000000001110000","000011110000000011110000","000111110000000011111000",
]);
def("wall", [
  "111111111111111111111111","111111111111111111111111","110000011100000111000001",
  "110000011100000111000001","111111111111111111111111","111111111111111111111111",
  "100011100001110000111000","100011100001110000111000","111111111111111111111111",
  "111111111111111111111111","110000011100000111000001","110000011100000111000001",
  "111111111111111111111111","111111111111111111111111","100011100001110000111000",
  "100011100001110000111000","111111111111111111111111","111111111111111111111111",
  "110000011100000111000001","110000011100000111000001","111111111111111111111111",
  "111111111111111111111111","100011100001110000111000","100011100001110000111000",
]);
def("tree", [
  "000000000001100000000000","000000000011110000000000","000000000111111000000000",
  "000000001111111100000000","000000011111111110000000","000000111111111111000000",
  "000001111111111111100000","000011111111111111110000","000011111111111111110000",
  "000011111111111111110000","000001111111111111100000","000000111111111111000000",
  "000000011111111110000000","000000001111111100000000","000000000111111000000000",
  "000000000011110000000000","000000000001100000000000","000000000001100000000000",
  "000000000001100000000000","000000000001100000000000","000000000001100000000000",
  "000000000001100000000000","000000000011110000000000","000000000111111000000000",
]);
def("npc", [
  "000000000011110000000000","000000000111111000000000","000000001101101100000000",
  "000000001111111100000000","000000000111111000000000","000000000011110000000000",
  "000000000111111000000000","000000001111111100000000","000000011111111110000000",
  "000000111111111111000000","000001111111111111100000","000011111111111111110000",
  "000011111111111111110000","000011111111111111110000","000011111111111111110000",
  "000011111111111111110000","000011111111111111110000","000001111111111111100000",
  "000001111111111111100000","000000111111111111000000","000000111111111111000000",
  "000000011111111110000000","000000001100001100000000","000000011000000110000000",
]);
def("sign", [
  "000000000011110000000000","000000000011110000000000","000001111111111111100000",
  "000001111111111111100000","000001111111111111100000","000001111111111111100000",
  "000001111111111111100000","000001111111111111100000","000001111111111111100000",
  "000001111111111111100000","000000000011110000000000","000000000011110000000000",
  "000000000011110000000000","000000000011110000000000","000000000011110000000000",
  "000000000011110000000000","000000000011110000000000","000000000011110000000000",
  "000000000011110000000000","000000000011110000000000","000000000011110000000000",
  "000000000011110000000000","000000001111111100000000","000000001111111100000000",
]);
def("half_light", [
  "000000000001100000000000","000000000011110000000000","000000000111111000000000",
  "000000001111111100000000","000000011111111110000000","000000111111111111000000",
  "000001111111111111100000","000011111111111111110000","000000000000000000000000",
  "000000000000000000000000","000000000000000000000000","000000000000000000000000",
  "000000000000000000000000","000000000000000000000000","000000000000000000000000",
  "000000000000000000000000","000100000000000000010000","000000000000000000000000",
  "000000000000000000000000","000000000000000000000000","000000000000000000000000",
  "000000000000000000000000","000000000000000000000000","000000000000000000000000",
]);
def("half_shadow", [
  "000000000000000000000000","000000000000000000000000","000000000000000000000000",
  "000000000000000000000000","000000000000000000000000","000000000000000000000000",
  "000000000000000000000000","000011111111111111110000","000001111111111111100000",
  "000000111111111111000000","000000011111111110000000","000000001111111100000000",
  "000000000111111000000000","000000000011110000000000","000000000001100000000000",
  "000000000000000000000000","000000000000000000000000","000000000000000000000000",
  "000100000000000000010000","000000000000000000000000","000000000000000000000000",
  "000000000000000000000000","000000000000000000000000","000000000000000000000000",
]);
def("shard_merged", [
  "000000000001100000000000","000000000011110000000000","000000000111111000000000",
  "000000001111111100000000","000000011111111110000000","000000111111111111000000",
  "000001111111111111100000","000011111111111111110000","000001111111111111100000",
  "000000111111111111000000","000000011111111110000000","000000001111111100000000",
  "000000000111111000000000","000000000011110000000000","000000000001100000000000",
  "000000000000000000000000","011000000000000000001100","000000000000000000000000",
  "000110000000000000110000","000000000000000000000000","000001100000000011000000",
  "000000000000000000000000","000000001100001100000000","000000000000000000000000",
]);
def("door", [
  "000000011111111100000000","000000111111111110000000","000001110000000111000000",
  "000011100000000011100000","000011000000000001100000","000110000000000000110000",
  "000110000000000000110000","000110000000000000110000","000110000001100000110000",
  "000110000011110000110000","000110000011110000110000","000110000001100000110000",
  "000110000000000000110000","000110000000000000110000","000110000000000000110000",
  "000110000000000000110000","000110000000000000110000","000110000000000000110000",
  "000110000000000000110000","000110000000000000110000","000111111111111111110000",
  "000111111111111111110000","000000000000000000000000","000000000000000000000000",
]);
def("ghoul", [
  "000000000111110000000000","000000001111111000000000","000000011101101110000000",
  "000000011111111110000000","000000011111111110000000","000000001111111100000000",
  "000000001111111100000000","000000011111111110000000","000001111111111111100000",
  "000011111111111111110000","000111111111111111111000","001111111111111111111100",
  "001111111111111111111100","001111111111111111111100","000111111111111111111000",
  "000011111111111111110000","000001111111111111100000","000000111111111111000000",
  "000000011101110110000000","000000001100001100000000","000000011000000110000000",
  "000000110000000011000000","000001100000000001100000","000011000000000000110000",
]);
def("plate", [
  "000000000000000000000000","000000000000000000000000","000000000000000000000000",
  "000000000000000000000000","000000000000000000000000","000000000000000000000000",
  "000000000000000000000000","000000000000000000000000","000000000000000000000000",
  "000000000000000000000000","000000000000000000000000","000000000000000000000000",
  "000000000000000000000000","000000000000000000000000","000000000000000000000000",
  "000000000000000000000000","000001111111111111100000","000011111111111111110000",
  "000011000000000001110000","000011000000000001110000","000011111111111111110000",
  "000001111111111111100000","000000000000000000000000","000000000000000000000000",
]);

def("mirror_tile", [
  "000000000000000000000000","000000000111111000000000","000000011000000110000000",
  "000000100000000001000000","000001000000000000100000","000010000011100000010000",
  "000010000111110000010000","000100001111111000001000","000100001111111000001000",
  "000100001111111000001000","000010000111110000010000","000010000011100000010000",
  "000001000000000000100000","000000100000000001000000","000000011000000110000000",
  "000000000111111000000000","000000000111111000000000","000000011000000110000000",
  "000000100000000001000000","000001000011100000100000","000010000111110000010000",
  "000010001111111000010000","000001001111111000100000","000000111111111110000000",
]);
def("headstone", [
  "000000000000000000000000","000000000000000000000000","000000011111111100000000",
  "000000111111111110000000","000001111111111111000000","000011111111111111100000",
  "000011111111111111100000","000011110000001111100000","000011110011001111100000",
  "000011110011001111100000","000011110011001111100000","000011110000001111100000",
  "000011111111111111100000","000011111111111111100000","000011111111111111100000",
  "000011111111111111100000","000011111111111111100000","000011111111111111100000",
  "000011111111111111100000","000011111111111111100000","000011111111111111100000",
  "000011111111111111100000","000111111111111111110000","000111111111111111110000",
]);
def("bell", [
  "000000000000000000000000","000000000001100000000000","000000000011110000000000",
  "000000000111111000000000","000000001111111100000000","000000011111111110000000",
  "000000111111111111000000","000001111111111111100000","000011111111111111110000",
  "000011111111111111110000","000011111111111111110000","000011111111111111110000",
  "000011111111111111110000","000011111111111111110000","000011111111111111110000",
  "000011111111111111110000","000111111111111111111000","000111111111111111111000",
  "000011111111111111110000","000000000011110000000000","000000000011110000000000",
  "000000000001100000000000","000000000000000000000000","000000000000000000000000",
]);

// ── Parse room ──────────────────────────────────────────────────
function parseRoom(str) {
  return str.trim().split("\n").map(row => {
    const r = row.trim(); const tiles = [];
    for (let i = 0; i < GRID; i++) {
      const c = i < r.length ? r[i] : ".";
      tiles.push(c === "#" ? T.WALL : c === "T" ? T.TREE : c === "~" ? T.WATER : c === "D" ? T.DOOR : c === "S" ? T.SWITCH : c === "M" ? T.MIRROR : T.EMPTY);
    }
    return tiles;
  });
}

// ── Shard locations (each has a light-half and shadow-half) ─────
const SHARD_LOCS = [
  { rx: 0, ry: -1, id: "shard_0", lx: 11, ly: 4, sx: 4, sy: 10 },
  { rx: -1, ry: 0, id: "shard_1", lx: 11, ly: 3, sx: 3, sy: 11 },
  { rx: 1, ry: 1, id: "shard_2", lx: 11, ly: 11, sx: 3, sy: 3 },
  { rx: 2, ry: -1, id: "shard_3", lx: 12, ly: 2, sx: 3, sy: 12 },
  { rx: 0, ry: 2, id: "shard_4", lx: 12, ly: 3, sx: 2, sy: 11 },
];
const SHARD_PICKUP_LINES = [
  "I. A star wakes beneath the ice.",
  "II. The dark learns a name it cannot say.",
  "III. Two roads remember one sky.",
  "IV. Something below begins to sing.",
  "V. Bring what is whole to the one who waits.",
];
const TOTAL_VOID_SHARDS = 5;
const VOID_SHARD_LOCS = [
  { rx: 0, ry: -3, id: "void_0" },
  { rx: -3, ry: 0, id: "void_1" },
  { rx: 3, ry: 3, id: "void_2" },
  { rx: -2, ry: -2, id: "void_3" },
  { rx: 2, ry: 3, id: "void_4" },
];
const VOID_PICKUP_LINES = [
  "I. The deep answers in its own voice.",
  "II. Roots find water that does not reflect.",
  "III. The glass cracks but will not part.",
  "IV. Silence takes a shape. You recognize it.",
  "V. The last seam is near. So is something else.",
];
const TOTAL_ECHO_SHARDS = 5;
const ECHO_SHARD_LOCS = [
  { rx: 0, ry: -2, id: "echo_0" },
  { rx: -2, ry: 0, id: "echo_1" },
  { rx: 2, ry: 2, id: "echo_2" },
  { rx: -2, ry: 2, id: "echo_3" },
  { rx: 2, ry: -2, id: "echo_4" },
];
const ECHO_PICKUP_LINES = [
  "I. A voice speaks your line before you.",
  "II. What you did, undoes. What you undid, does.",
  "III. Two paths. One step. No witness.",
  "IV. The mirror says the name you had forgotten.",
  "V. The reflection lowers its hand. You lower yours.",
];
const isBlockingEntity = e => e.type === "npc" || (e.type === "sign" && e.blocking);

// ── Ghoul AI ───────────────────────────────────────────────────
function moveGhouls(entities, tiles, playerPos) {
  return entities.map(e => {
    if (e.type !== "ghoul") return e;
    const dx = Math.sign(playerPos.x - e.x);
    const dy = Math.sign(playerPos.y - e.y);
    // Try primary axis (larger distance first), then secondary
    const adx = Math.abs(playerPos.x - e.x), ady = Math.abs(playerPos.y - e.y);
    const moves = adx >= ady
      ? [{ dx, dy: 0 }, { dx: 0, dy }]
      : [{ dx: 0, dy }, { dx, dy: 0 }];
    for (const m of moves) {
      if (m.dx === 0 && m.dy === 0) continue;
      const nx = e.x + m.dx, ny = e.y + m.dy;
      if (nx < 0 || nx >= GRID || ny < 0 || ny >= GRID) continue;
      if (tiles[ny][nx] === T.WALL || tiles[ny][nx] === T.TREE || tiles[ny][nx] === T.WATER) continue;
      // Don't stack on other ghouls
      if (entities.some(o => o !== e && o.type === "ghoul" && o.x === nx && o.y === ny)) continue;
      return { ...e, x: nx, y: ny };
    }
    return e; // stuck
  });
}

function checkGhoulCollision(entities, pos) {
  return entities.some(e => e.type === "ghoul" && e.x === pos.x && e.y === pos.y);
}

// ── Pressure plate logic ───────────────────────────────────────
function isPlatePressed(world, playerPos) {
  if (world.tiles[playerPos.y]?.[playerPos.x] === T.SWITCH) return true;
  return world.entities.some(e =>
    (e.type === "half_light" || e.type === "half_shadow") &&
    world.tiles[e.y]?.[e.x] === T.SWITCH
  );
}

// ── Room definitions ────────────────────────────────────────────
// R = helper for full-height wall column
const wallCol = (col) => {
  let s = "";
  for (let y = 0; y < GRID; y++) {
    let row = "";
    for (let x = 0; x < GRID; x++) row += x === col ? "#" : ".";
    s += row + (y < GRID - 1 ? "\n" : "");
  }
  return s;
};
const emptyRoom = () => {
  let s = "";
  for (let y = 0; y < GRID; y++) { s += "..............."; if (y < GRID - 1) s += "\n"; }
  return s;
};
const pointWalls = (pts) => {
  const rows = Array.from({ length: GRID }, () => Array(GRID).fill("."));
  pts.forEach(([x, y]) => { if (x >= 0 && x < GRID && y >= 0 && y < GRID) rows[y][x] = "#"; });
  return rows.map(r => r.join("")).join("\n");
};

function getTutorialRoom(rx, ry) {
  if (rx === 0 && ry === 0) return {
    light: {
      tiles: parseRoom(
        "...............\n...............\n..T.........T..\n...............\n...............\n...............\n...............\n" +
        "...............\n...............\n...............\n...............\n...............\n..T.........T..\n...............\n..............."),
      entities: [{ id: "watcher", type: "npc", sprite: "npc", x: 5, y: 7, dialogue: "watcher" }]
    },
    shadow: {
      tiles: parseRoom(
        "...............\n...............\n...............\n...T.......T...\n...............\n...............\n...............\n" +
        "...............\n...............\n...............\n...............\n...T.......T...\n...............\n...............\n..............."),
      entities: []
    }
  };
  if (rx === 1 && ry === 0) return {
    light: { tiles: parseRoom(wallCol(9)),
      entities: [{ id: "sign_1L", type: "sign", sprite: "sign", x: 3, y: 7, text: "Split the light. Five breaths before the thread pulls taut." }] },
    shadow: { tiles: parseRoom(emptyRoom()), entities: [] }
  };
  if (rx === 2 && ry === 0) return {
    light: { tiles: parseRoom(emptyRoom()), entities: [] },
    shadow: { tiles: parseRoom(wallCol(9)),
      entities: [{ id: "sign_2S", type: "sign", sprite: "sign", x: 3, y: 7, text: "Split the dark. Five breaths. No more." }] }
  };
  if (rx === 3 && ry === 0) return {
    light: { tiles: parseRoom(wallCol(10)), entities: [] },
    shadow: { tiles: parseRoom(wallCol(5)), entities: [] }
  };
  if (rx === 4 && ry === 0) return {
    light: {
      tiles: parseRoom(
        "...............\n...............\n..T...T...T....\n...............\n...............\n...............\n...............\n" +
        "...............\n...............\n...............\n...............\n...............\n..T...T...T....\n...............\n..............."),
      entities: [{ id: "sage", type: "npc", sprite: "npc", x: 7, y: 6, dialogue: "sage" }]
    },
    shadow: {
      tiles: parseRoom(
        "...............\n...............\n...............\n...T...T...T...\n...............\n...............\n...............\n" +
        "...............\n...............\n...............\n...............\n...T...T...T...\n...............\n...............\n..............."),
      entities: []
    }
  };

  // ── The Ambush (1,-1): unwinnable ghoul encounter ──
  if (rx === 1 && ry === -1) return {
    light: { tiles: parseRoom(pointWalls([[3,3],[4,3],[10,3],[11,3],[3,11],[4,11],[10,11],[11,11]])),
      entities: [
        { id: "g_l0", type: "ghoul", sprite: "ghoul", x: 1, y: 1 },
        { id: "g_l1", type: "ghoul", sprite: "ghoul", x: 13, y: 1 },
        { id: "g_l2", type: "ghoul", sprite: "ghoul", x: 1, y: 13 },
        { id: "g_l3", type: "ghoul", sprite: "ghoul", x: 13, y: 13 },
      ] },
    shadow: { tiles: parseRoom(pointWalls([[3,3],[4,3],[10,3],[11,3],[3,11],[4,11],[10,11],[11,11]])),
      entities: [
        { id: "g_s0", type: "ghoul", sprite: "ghoul", x: 7, y: 1 },
        { id: "g_s1", type: "ghoul", sprite: "ghoul", x: 7, y: 13 },
      ] }
  };

  // ── Ghoul patrol rooms — avoidable encounters ──
  if (rx === -1 && ry === -1) return {
    light: { tiles: parseRoom(pointWalls([[5,4],[6,4],[8,4],[9,4],[5,10],[6,10],[8,10],[9,10]])),
      entities: [{ id: "gp_l0", type: "ghoul", sprite: "ghoul", x: 3, y: 7 }] },
    shadow: { tiles: parseRoom(pointWalls([[4,5],[4,6],[4,8],[4,9],[10,5],[10,6],[10,8],[10,9]])),
      entities: [{ id: "gp_s0", type: "ghoul", sprite: "ghoul", x: 11, y: 7 }] }
  };
  if (rx === 1 && ry === 2) return {
    light: { tiles: parseRoom(pointWalls([[4,4],[5,4],[9,4],[10,4],[7,7]])),
      entities: [
        { id: "gp2_l0", type: "ghoul", sprite: "ghoul", x: 2, y: 2 },
        { id: "gp2_l1", type: "ghoul", sprite: "ghoul", x: 12, y: 12 },
      ] },
    shadow: { tiles: parseRoom(pointWalls([[4,10],[5,10],[9,10],[10,10],[7,7]])),
      entities: [{ id: "gp2_s0", type: "ghoul", sprite: "ghoul", x: 7, y: 2 }] }
  };

  // ── Shard rooms — Zelda-style push puzzles ──
  // Walls in the SAME world shape the push path (stop-walls, channels).
  // Walls in the OTHER world restrict combined-mode positioning, forcing creative split approaches.
  // Each half requires 2-3 split sessions with L-shaped push paths.

  // Shard 0 (0,-1): "The Corner Push" — L-bend: push down then left / up then right. 4 splits.
  if (rx === 0 && ry === -1) return {
    light: { tiles: parseRoom(pointWalls([[10,4],[10,5],[10,6], [11,8], [6,7]])),
      entities: [{ id: "shard_0_l", type: "half_light", sprite: "half_light", x: 11, y: 4 }] },
    shadow: { tiles: parseRoom(pointWalls([[5,10],[5,9], [3,10],[3,9], [4,6], [8,7]])),
      entities: [{ id: "shard_0_s", type: "half_shadow", sprite: "half_shadow", x: 4, y: 10 }] }
  };

  // Shard 1 (-1,0): "The Box" — push left then down / right then up. 4 splits.
  if (rx === -1 && ry === 0) return {
    light: { tiles: parseRoom(pointWalls([[6,3], [7,8], [3,12], [2,11]])),
      entities: [{ id: "shard_1_l", type: "half_light", sprite: "half_light", x: 11, y: 3 }] },
    shadow: { tiles: parseRoom(pointWalls([[8,11], [7,6], [11,2], [12,3]])),
      entities: [{ id: "shard_1_s", type: "half_shadow", sprite: "half_shadow", x: 3, y: 11 }] }
  };

  // Shard 2 (1,1): "The Spiral" — diagonal L-bends from opposite corners. 4 splits.
  if (rx === 1 && ry === 1) return {
    light: { tiles: parseRoom(pointWalls([[11,6], [6,7], [3,2], [2,3]])),
      entities: [{ id: "shard_2_l", type: "half_light", sprite: "half_light", x: 11, y: 11 }] },
    shadow: { tiles: parseRoom(pointWalls([[3,8], [8,7], [11,12], [12,11]])),
      entities: [{ id: "shard_2_s", type: "half_shadow", sprite: "half_shadow", x: 3, y: 3 }] }
  };

  // Shard 3 (2,-1): "The Gauntlet" — 3-segment push paths. 6 splits.
  if (rx === 2 && ry === -1) return {
    light: { tiles: parseRoom(pointWalls([[12,4], [6,3], [7,6], [10,2]])),
      entities: [{ id: "shard_3_l", type: "half_light", sprite: "half_light", x: 12, y: 2 }] },
    shadow: { tiles: parseRoom(pointWalls([[3,8], [8,9], [7,4], [11,1],[11,2], [4,12],[4,11]])),
      entities: [{ id: "shard_3_s", type: "half_shadow", sprite: "half_shadow", x: 3, y: 12 }] }
  };

  // Shard 4 (0,2): "The Vault" — 3-segment push paths, hardest Ch1 puzzle. 6 splits.
  if (rx === 0 && ry === 2) return {
    light: { tiles: parseRoom(pointWalls([[9,3], [10,8], [6,7], [12,4], [3,10],[3,9]])),
      entities: [{ id: "shard_4_l", type: "half_light", sprite: "half_light", x: 12, y: 3 }] },
    shadow: { tiles: parseRoom(pointWalls([[5,11], [4,6], [8,7], [2,10], [11,4],[11,3]])),
      entities: [{ id: "shard_4_s", type: "half_shadow", sprite: "half_shadow", x: 2, y: 11 }] }
  };

  // ── Easter eggs at the far corners of the bounded world ────────

  // (4, -4): The Graveyard — rows of headstones, one bears a name.
  if (rx === 4 && ry === -4) return {
    light: {
      tiles: parseRoom(emptyRoom()),
      entities: [
        { id: "grave_l1", type: "sign", sprite: "headstone", x: 3, y: 4, blocking: true,
          lines: ["Here rests one who was two.", "They forgot which half was first."] },
        { id: "grave_l2", type: "sign", sprite: "headstone", x: 7, y: 4, blocking: true,
          lines: [
            "Here begins the one who carved the hinge.",
            "\u2014  J A M E S  \u2014",
            "@JLUDDY7",
            "x.com/JLUDDY7",
          ] },
        { id: "grave_l3", type: "sign", sprite: "headstone", x: 11, y: 4, blocking: true,
          lines: ["Almost a shape again.", "Almost."] },
        { id: "grave_l4", type: "sign", sprite: "headstone", x: 3, y: 10, blocking: true,
          lines: ["Name forgotten.", "Halves never met."] },
        { id: "grave_l5", type: "sign", sprite: "headstone", x: 7, y: 10, blocking: true,
          lines: ["They lost the thread."] },
        { id: "grave_l6", type: "sign", sprite: "headstone", x: 11, y: 10, blocking: true,
          lines: ["They looked too long at the light."] },
      ]
    },
    shadow: {
      tiles: parseRoom(emptyRoom()),
      entities: [
        { id: "grave_s1", type: "sign", sprite: "headstone", x: 3, y: 4, blocking: true,
          lines: ["The dark was kind.", "Or so she said."] },
        { id: "grave_s2", type: "sign", sprite: "headstone", x: 7, y: 4, blocking: true,
          lines: ["The stone does not know who lies beneath.", "Neither do you."] },
        { id: "grave_s3", type: "sign", sprite: "headstone", x: 11, y: 4, blocking: true,
          lines: ["She walked through the mirror.", "She did not walk back."] },
        { id: "grave_s4", type: "sign", sprite: "headstone", x: 3, y: 10, blocking: true,
          lines: ["The hinge forgot them."] },
        { id: "grave_s5", type: "sign", sprite: "headstone", x: 7, y: 10, blocking: true,
          lines: ["They thought they were one.", "They were not yet two."] },
        { id: "grave_s6", type: "sign", sprite: "headstone", x: 11, y: 10, blocking: true,
          lines: ["He came twice.", "Once as light. Once as dark.", "Neither returned."] },
      ]
    }
  };

  // (-4, 4): The First Diptych — two figures standing as one piece.
  if (rx === -4 && ry === 4) return {
    light: {
      tiles: parseRoom(emptyRoom()),
      entities: [
        { id: "fd_fig1", type: "sign", sprite: "player", x: 6, y: 7, blocking: true,
          lines: ["A figure of light.", "Standing as it was before the panels parted."] },
        { id: "fd_fig2", type: "sign", sprite: "player", x: 8, y: 7, blocking: true,
          lines: ["A figure of shadow.", "Standing as it was before the panels parted."] },
        { id: "fd_plaque", type: "sign", sprite: "sign", x: 7, y: 10, blocking: true,
          lines: [
            "Before the panels parted.",
            "Before the hinge.",
            "There was only a figure.",
          ] },
      ]
    },
    shadow: {
      tiles: parseRoom(emptyRoom()),
      entities: [
        { id: "fd_fig3", type: "sign", sprite: "player", x: 6, y: 7, blocking: true,
          lines: ["A figure of light.", "It is watching you. So are you."] },
        { id: "fd_fig4", type: "sign", sprite: "player", x: 8, y: 7, blocking: true,
          lines: ["A figure of shadow.", "It is watching you. So are you."] },
      ]
    }
  };

  // (-4, 0): The Empty Watcher — mirror of the starting room, but she is gone.
  if (rx === -4 && ry === 0) return {
    light: {
      tiles: parseRoom(
        "...............\n...............\n..T.........T..\n...............\n...............\n...............\n...............\n" +
        "...............\n...............\n...............\n...............\n...............\n..T.........T..\n...............\n..............."),
      entities: [
        { id: "ew_sign", type: "sign", sprite: "sign", x: 5, y: 7, blocking: true,
          lines: [
            "The one who waits is not here.",
            "She has gone to look for what she lost.",
            "Or perhaps she was never here at all.",
          ] },
      ]
    },
    shadow: {
      tiles: parseRoom(
        "...............\n...............\n...............\n...T.......T...\n...............\n...............\n...............\n" +
        "...............\n...............\n...............\n...............\n...T.......T...\n...............\n...............\n..............."),
      entities: []
    }
  };

  // (0, 4): The Bell — a bell remembered by the Sage.
  if (rx === 0 && ry === 4) return {
    light: {
      tiles: parseRoom(emptyRoom()),
      entities: [
        { id: "bell_l", type: "sign", sprite: "bell", x: 7, y: 7, blocking: true,
          lines: [
            "The bell has already rung.",
            "You could not have heard it.",
            "You were not yet two.",
          ] },
      ]
    },
    shadow: {
      tiles: parseRoom(emptyRoom()),
      entities: [
        { id: "bell_s", type: "sign", sprite: "bell", x: 7, y: 7, blocking: true,
          lines: [
            "The sound is still in the stone.",
            "The stone is still listening.",
          ] },
      ]
    }
  };

  return null;
}

// ── Chapter 2: Void Shard rooms ────────────────────────────────
function getChapter2Room(rx, ry) {
  // Void 0 (0,-3): "The Fortress" — 3-segment L-bends. 6 splits.
  if (rx === 0 && ry === -3) return {
    light: { tiles: parseRoom(pointWalls([[11,6], [6,5], [7,8], [10,1]])),
      entities: [{ id: "void_0_l", type: "half_light", sprite: "half_light", x: 11, y: 2 }] },
    shadow: { tiles: parseRoom(pointWalls([[3,8], [8,9], [7,6], [4,13]])),
      entities: [{ id: "void_0_s", type: "half_shadow", sprite: "half_shadow", x: 3, y: 12 }] }
  };
  // Void 1 (-3,0): "The Maze" — corner shards with tight channels. 6 splits.
  if (rx === -3 && ry === 0) return {
    light: { tiles: parseRoom(pointWalls([[3,6], [8,3], [7,8], [4,1]])),
      entities: [{ id: "void_1_l", type: "half_light", sprite: "half_light", x: 3, y: 3 }] },
    shadow: { tiles: parseRoom(pointWalls([[11,8], [6,11], [7,6], [10,13]])),
      entities: [{ id: "void_1_s", type: "half_shadow", sprite: "half_shadow", x: 11, y: 11 }] }
  };
  // Void 2 (3,3): "The Gatekeeper" — shadow half trapped behind doors.
  // Push light half onto light switch → shadow doors open → push shadow half out.
  // Then push light half off switch to meeting point → collect both.
  if (rx === 3 && ry === 3) {
    const lTiles = Array.from({ length: GRID }, () => Array(GRID).fill(T.EMPTY));
    lTiles[7][7] = T.SWITCH; // pressure plate at center
    // Walls: stop light half on switch, guide pushes
    [[7,6],[6,7],[8,3]].forEach(([x,y]) => { lTiles[y][x] = T.WALL; });
    const sTiles = Array.from({ length: GRID }, () => Array(GRID).fill(T.EMPTY));
    // Door cage around shadow half — fully enclosed, only opens via light switch
    [[6,2],[7,2],[8,2],[6,3],[8,3],[6,4],[7,4],[8,4]].forEach(([x,y]) => { sTiles[y][x] = T.DOOR; });
    // Walls to guide shadow half to meeting point after escape
    [[8,7],[7,10]].forEach(([x,y]) => { sTiles[y][x] = T.WALL; });
    return {
      light: { tiles: lTiles,
        entities: [{ id: "void_2_l", type: "half_light", sprite: "half_light", x: 7, y: 11 }] },
      shadow: { tiles: sTiles,
        entities: [{ id: "void_2_s", type: "half_shadow", sprite: "half_shadow", x: 7, y: 3 }] }
    };
  }
  // Void 3 (-2,-2): "The Echo" — mirrored diagonal approach. 6 splits.
  if (rx === -2 && ry === -2) return {
    light: { tiles: parseRoom(pointWalls([[11,8], [8,11], [6,7], [10,10]])),
      entities: [{ id: "void_3_l", type: "half_light", sprite: "half_light", x: 11, y: 11 }] },
    shadow: { tiles: parseRoom(pointWalls([[3,6], [6,3], [8,7], [4,4]])),
      entities: [{ id: "void_3_s", type: "half_shadow", sprite: "half_shadow", x: 3, y: 3 }] }
  };
  // Void 4 (2,3): "The Double Gate" — both halves trapped behind doors.
  // Light half behind DOOR cage in light world (opened by shadow switch).
  // Shadow half behind DOOR cage in shadow world (opened by light switch).
  // Must use split to stand on one switch, freeing the other world's half.
  if (rx === 2 && ry === 3) {
    const lTiles = Array.from({ length: GRID }, () => Array(GRID).fill(T.EMPTY));
    lTiles[7][4] = T.SWITCH; // light switch → opens shadow doors
    // Light half trapped behind door cage (opened by shadow switch at (10,7))
    [[9,2],[10,2],[11,2],[9,3],[11,3],[9,4],[10,4],[11,4]].forEach(([x,y]) => { lTiles[y][x] = T.DOOR; });
    [[6,7],[7,10]].forEach(([x,y]) => { lTiles[y][x] = T.WALL; });
    const sTiles = Array.from({ length: GRID }, () => Array(GRID).fill(T.EMPTY));
    sTiles[7][10] = T.SWITCH; // shadow switch → opens light doors
    // Shadow half trapped behind door cage (opened by light switch at (4,7))
    [[3,10],[4,10],[5,10],[3,11],[5,11],[3,12],[4,12],[5,12]].forEach(([x,y]) => { sTiles[y][x] = T.DOOR; });
    [[8,7],[7,4]].forEach(([x,y]) => { sTiles[y][x] = T.WALL; });
    return {
      light: { tiles: lTiles,
        entities: [{ id: "void_4_l", type: "half_light", sprite: "half_light", x: 10, y: 3 }] },
      shadow: { tiles: sTiles,
        entities: [{ id: "void_4_s", type: "half_shadow", sprite: "half_shadow", x: 4, y: 11 }] }
    };
  }
  return null;
}

// ── Chapter 3: Echo Shard rooms (require mirror mode) ──────────
function getChapter3Room(rx, ry) {
  // Each room has MIRROR tiles that the player must step on.
  // In mirror mode, shadow moves opposite to light — needed to push halves apart toward meeting point.

  // Echo 0 (0,-2): "The Reflection" — MANDATORY mirror puzzle.
  // Both halves behind door cages. Light switch at (3,7), shadow switch at (11,7).
  // Combined mode: both at same pos, can only reach one switch.
  // Split mode: one snaps back, losing the switch.
  // Mirror mode from (7,7): press LEFT 4x → light(3,7) shadow(11,7). Both pressed!
  if (rx === 0 && ry === -2) {
    const lTiles = Array.from({ length: GRID }, () => Array(GRID).fill(T.EMPTY));
    lTiles[7][3] = T.SWITCH; lTiles[7][7] = T.MIRROR;
    // Light shard behind shadow-controlled door cage
    [[9,2],[10,2],[11,2],[9,3],[11,3],[9,4],[10,4],[11,4]].forEach(([x,y]) => { lTiles[y][x] = T.DOOR; });
    // Block combined approach to shadow switch position
    [[11,6],[11,8]].forEach(([x,y]) => { lTiles[y][x] = T.WALL; });
    const sTiles = Array.from({ length: GRID }, () => Array(GRID).fill(T.EMPTY));
    sTiles[7][11] = T.SWITCH; sTiles[7][7] = T.MIRROR;
    // Shadow shard behind light-controlled door cage
    [[3,10],[4,10],[5,10],[3,11],[5,11],[3,12],[4,12],[5,12]].forEach(([x,y]) => { sTiles[y][x] = T.DOOR; });
    // Block combined approach to light switch position
    [[3,6],[3,8]].forEach(([x,y]) => { sTiles[y][x] = T.WALL; });
    return {
      linkedPlates: true,
      light: { tiles: lTiles,
        entities: [{ id: "echo_0_l", type: "half_light", sprite: "half_light", x: 10, y: 3 }] },
      shadow: { tiles: sTiles,
        entities: [{ id: "echo_0_s", type: "half_shadow", sprite: "half_shadow", x: 4, y: 11 }] }
    };
  }

  // Echo 1 (-2,0): "The Divide" — vertical mirror puzzle.
  // Mirror tile triggers inverse vertical movement. Push halves apart vertically.
  if (rx === -2 && ry === 0) {
    const lTiles = Array.from({ length: GRID }, () => Array(GRID).fill(T.EMPTY));
    lTiles[7][7] = T.MIRROR;
    [[7,3],[6,7],[8,7]].forEach(([x,y]) => { lTiles[y][x] = T.WALL; });
    const sTiles = Array.from({ length: GRID }, () => Array(GRID).fill(T.EMPTY));
    sTiles[7][7] = T.MIRROR;
    [[7,11],[6,7],[8,7]].forEach(([x,y]) => { sTiles[y][x] = T.WALL; });
    return {
      light: { tiles: lTiles,
        entities: [{ id: "echo_1_l", type: "half_light", sprite: "half_light", x: 7, y: 5 }] },
      shadow: { tiles: sTiles,
        entities: [{ id: "echo_1_s", type: "half_shadow", sprite: "half_shadow", x: 7, y: 9 }] }
    };
  }

  // Echo 2 (2,2): "The Crossroads" — mirror + walls force diagonal-like convergence.
  if (rx === 2 && ry === 2) {
    const lTiles = Array.from({ length: GRID }, () => Array(GRID).fill(T.EMPTY));
    lTiles[10][7] = T.MIRROR;
    [[3,7],[7,3],[6,10],[8,10]].forEach(([x,y]) => { lTiles[y][x] = T.WALL; });
    const sTiles = Array.from({ length: GRID }, () => Array(GRID).fill(T.EMPTY));
    sTiles[10][7] = T.MIRROR;
    [[11,7],[7,11],[6,10],[8,10]].forEach(([x,y]) => { sTiles[y][x] = T.WALL; });
    return {
      light: { tiles: lTiles,
        entities: [{ id: "echo_2_l", type: "half_light", sprite: "half_light", x: 5, y: 10 }] },
      shadow: { tiles: sTiles,
        entities: [{ id: "echo_2_s", type: "half_shadow", sprite: "half_shadow", x: 9, y: 10 }] }
    };
  }

  // Echo 3 (-2,2): "The Paradox" — two mirror tiles creating a sequence.
  if (rx === -2 && ry === 2) {
    const lTiles = Array.from({ length: GRID }, () => Array(GRID).fill(T.EMPTY));
    lTiles[4][7] = T.MIRROR; lTiles[10][7] = T.MIRROR;
    [[3,4],[11,4],[7,2],[5,10],[9,10]].forEach(([x,y]) => { lTiles[y][x] = T.WALL; });
    const sTiles = Array.from({ length: GRID }, () => Array(GRID).fill(T.EMPTY));
    sTiles[4][7] = T.MIRROR; sTiles[10][7] = T.MIRROR;
    [[3,10],[11,10],[7,12],[5,4],[9,4]].forEach(([x,y]) => { sTiles[y][x] = T.WALL; });
    return {
      light: { tiles: lTiles,
        entities: [{ id: "echo_3_l", type: "half_light", sprite: "half_light", x: 7, y: 3 }] },
      shadow: { tiles: sTiles,
        entities: [{ id: "echo_3_s", type: "half_shadow", sprite: "half_shadow", x: 7, y: 11 }] }
    };
  }

  // Echo 4 (2,-2): "The Final Mirror" — hardest. Mirror + doors + switches + push.
  // Same dual-switch pattern but switches are farther (3 steps) and shards need L-push after freeing.
  // Mirror at (7,7). Switches at (4,7) light and (10,7) shadow. Press LEFT 3x.
  if (rx === 2 && ry === -2) {
    const lTiles = Array.from({ length: GRID }, () => Array(GRID).fill(T.EMPTY));
    lTiles[7][4] = T.SWITCH; lTiles[7][7] = T.MIRROR;
    // Light shard behind shadow-controlled doors + walls forming L-path
    [[9,2],[10,2],[11,2],[11,3],[11,4],[9,4],[10,4]].forEach(([x,y]) => { lTiles[y][x] = T.DOOR; });
    [[10,6],[10,8],[9,7]].forEach(([x,y]) => { lTiles[y][x] = T.WALL; });
    const sTiles = Array.from({ length: GRID }, () => Array(GRID).fill(T.EMPTY));
    sTiles[7][10] = T.SWITCH; sTiles[7][7] = T.MIRROR;
    // Shadow shard behind light-controlled doors + walls forming L-path
    [[3,10],[4,10],[5,10],[3,11],[3,12],[5,12],[5,11]].forEach(([x,y]) => { sTiles[y][x] = T.DOOR; });
    [[4,6],[4,8],[5,7]].forEach(([x,y]) => { sTiles[y][x] = T.WALL; });
    return {
      linkedPlates: true,
      light: { tiles: lTiles,
        entities: [{ id: "echo_4_l", type: "half_light", sprite: "half_light", x: 10, y: 3 }] },
      shadow: { tiles: sTiles,
        entities: [{ id: "echo_4_s", type: "half_shadow", sprite: "half_shadow", x: 4, y: 11 }] }
    };
  }

  return null;
}

// ── Procedural generation ───────────────────────────────────────
function generateDualRoom(rx, ry, seed, chapter) {
  const tutorial = getTutorialRoom(rx, ry);
  if (tutorial) return tutorial;
  if (chapter >= 2) {
    const ch2 = getChapter2Room(rx, ry);
    if (ch2) return ch2;
  }
  if (chapter >= 3) {
    const ch3 = getChapter3Room(rx, ry);
    if (ch3) return ch3;
  }

  const lrng = mulberry32(seed + rx * 7919 + ry * 6271);
  const srng = mulberry32(seed + rx * 7919 + ry * 6271 + 500000);
  const light = Array.from({ length: GRID }, () => Array(GRID).fill(T.EMPTY));
  const shadow = Array.from({ length: GRID }, () => Array(GRID).fill(T.EMPTY));

  // Perimeter walls with doorways (DOOR_LO..DOOR_HI open)
  for (let i = 0; i < GRID; i++) {
    if (i < DOOR_LO || i > DOOR_HI) {
      light[0][i] = T.WALL; light[GRID - 1][i] = T.WALL;
      light[i][0] = T.WALL; light[i][GRID - 1] = T.WALL;
      shadow[0][i] = T.WALL; shadow[GRID - 1][i] = T.WALL;
      shadow[i][0] = T.WALL; shadow[i][GRID - 1] = T.WALL;
    }
  }

  // Interior walls — much sparser than before.
  // Per-world chance max 18%. Since a tile is blocked if EITHER world has a wall,
  // combined-mode blocked rate ≈ 33% which is navigable.
  const dist = Math.abs(rx) + Math.abs(ry);
  const wc = Math.min(0.18, 0.06 + dist * 0.015);
  for (let y = 1; y < GRID - 1; y++) {
    for (let x = 1; x < GRID - 1; x++) {
      // Larger central safe zone (5x5 around center)
      if (Math.abs(x - 7) <= 2 && Math.abs(y - 7) <= 2) continue;
      // Keep doorway corridors clear (3 tiles deep)
      const inDoor = (x >= DOOR_LO && x <= DOOR_HI && (y <= 2 || y >= GRID - 3)) ||
                     (y >= DOOR_LO && y <= DOOR_HI && (x <= 2 || x >= GRID - 3));
      if (inDoor) continue;
      if (lrng() < wc) light[y][x] = lrng() < 0.3 ? T.TREE : T.WALL;
      if (srng() < wc) shadow[y][x] = srng() < 0.3 ? T.TREE : T.WALL;
    }
  }

  // Occasional water in far rooms
  if (dist > 4) {
    const patch = (grid, rng) => {
      const cx = 4 + Math.floor(rng() * (GRID - 8));
      const cy = 4 + Math.floor(rng() * (GRID - 8));
      for (let dy2 = -1; dy2 <= 1; dy2++)
        for (let dx2 = -1; dx2 <= 1; dx2++)
          if (rng() < 0.5 && grid[cy + dy2][cx + dx2] === T.EMPTY)
            grid[cy + dy2][cx + dx2] = T.WATER;
    };
    if (lrng() < 0.25) patch(light, lrng);
    if (srng() < 0.25) patch(shadow, srng);
  }

  // ── Connectivity guarantee ──
  // Ensure a combined-mode path exists from every doorway through the center.
  // If a tile is blocked in both worlds (or would block the combined path),
  // clear it in the world where it's least disruptive.
  // We do this by carving guaranteed horizontal + vertical corridors through the center.
  const corridor = [7]; // center row/col
  for (const y of corridor) {
    for (let x = 1; x < GRID - 1; x++) {
      // Clear center row in both worlds
      if (light[y][x] !== T.EMPTY) light[y][x] = T.EMPTY;
      if (shadow[y][x] !== T.EMPTY) shadow[y][x] = T.EMPTY;
    }
  }
  for (const x of corridor) {
    for (let y = 1; y < GRID - 1; y++) {
      if (light[y][x] !== T.EMPTY) light[y][x] = T.EMPTY;
      if (shadow[y][x] !== T.EMPTY) shadow[y][x] = T.EMPTY;
    }
  }

  return { light: { tiles: light, entities: [] }, shadow: { tiles: shadow, entities: [] } };
}

// ── Dynamic dialogue ────────────────────────────────────────────
function getDialogue(id, shardsSet, victory, chapter, voidShardsSet, echoShardsSet) {
  const sc = shardsSet.size;
  const vc = voidShardsSet ? voidShardsSet.size : 0;
  const ec = echoShardsSet ? echoShardsSet.size : 0;
  const hintFor = (locs, collected) => {
    const uncol = locs.filter(s => !collected.has(s.id));
    return "Pieces wait in the " + uncol.map(s => {
      let d = "";
      if (s.ry < 0) d += "north"; if (s.ry > 0) d += "south";
      if (s.rx < 0) d += (d ? "-" : "") + "west"; if (s.rx > 0) d += (d ? "-" : "") + "east";
      return d || "near";
    }).join(", ") + ".";
  };
  if (id === "watcher") {
    // Chapter 3 dialogue
    if (chapter >= 3) {
      if (victory) return [
        "Fifteen pieces. One figure. No name.",
        "I will not watch you from here again.",
        "Whatever you are now \u2014 walk.",
      ];
      if (ec >= TOTAL_ECHO_SHARDS) return [
        "Come stand where both of you can see.",
        "The mirror is listening.",
      ];
      const eHint = hintFor(ECHO_SHARD_LOCS, echoShardsSet);
      if (ec >= 3) return [
        "You have stopped being two things.",
        "You have not yet become one.",
        "This is where most travelers end.",
        eHint,
      ];
      if (ec >= 1) return [
        "The reflection has begun to answer.",
        "I cannot tell yours from mine anymore.",
        eHint,
      ];
      return [
        "There is a place where forward is also back.",
        "Step onto the eye. The eye will look at you.",
        "Your shadow will walk against you. Do not be afraid of it.",
        "It is only what you have always been doing, made visible.",
        eHint,
      ];
    }
    // Chapter 2 dialogue
    if (chapter >= 2) {
      if (victory) return [
        "Two rifts sealed. A mirror remains.",
        "I had hoped you would not find it.",
        "I am not sure which of us is on its other side.",
      ];
      if (vc >= TOTAL_VOID_SHARDS) return [
        "The deep answers. Return to me.",
        "Do not look at it for too long.",
      ];
      const vHint = hintFor(VOID_SHARD_LOCS, voidShardsSet);
      if (vc >= 3) return [
        "Some of the walls are not walls.",
        "Some of the locks are kind. Most are not.",
        vHint,
      ];
      if (vc >= 1) return [
        "You remember the first time you split. Hold that memory.",
        "Down here it is the only warm thing.",
        vHint,
      ];
      return [
        "Below the rift there is another rift.",
        "Below that I will not say.",
        "Five shards in the dark below. They do not want you.",
        "They have been alone a long time.",
        vHint,
      ];
    }
    // Chapter 1 dialogue
    if (victory) return [
      "The seam is stitched. The wound is not.",
      "There is a deeper cut. I did not want to tell you.",
      "I still do not.",
    ];
    if (sc >= TOTAL_SHARDS) return [
      "Five. Enough for a hand to close.",
      "Come here. Stand where I stand.",
    ];
    const uncol = SHARD_LOCS.filter(s => !shardsSet.has(s.id));
    const dirs = uncol.map(s => {
      let d = "";
      if (s.ry < 0) d += "north"; if (s.ry > 0) d += "south";
      if (s.rx < 0) d += (d ? "-" : "") + "west"; if (s.rx > 0) d += (d ? "-" : "") + "east";
      return d || "near";
    });
    const hint = "Pieces wait in the " + dirs.join(", ") + ".";
    if (sc >= 3) return [
      "You are nearly a shape again.",
      "I will not say what. Names are for the finished.",
      hint,
    ];
    if (sc >= 1) return [
      "One piece remembers. The others will not take long.",
      "Do not call them back. They were never lost.",
      hint,
    ];
    return [
      "You came as two. Most do not notice.",
      "One half walks in daylight. One half in its absence.",
      "Neither is the true one.",
      "Something was broken before the world had a name for breaking.",
      "Its pieces rest where both halves must meet.",
      "Go. Press yourself against what resists you.",
      "The thread will hold. It always has.",
      hint,
    ];
  }
  if (id === "sage") {
    if (chapter >= 2) {
      if (vc >= TOTAL_VOID_SHARDS) return [
        "The void has taken your measure.",
        "It finds you sufficient. I am sorry.",
      ];
      return [
        "The deep bends the rule you learned above.",
        "Patience was the first lesson. Precision is the second.",
        "The third I will not teach you.",
      ];
    }
    if (sc >= TOTAL_SHARDS) return [
      "I heard the last bell. So did the things that remember bells.",
      "Go to her. Do not linger in the doorway.",
    ];
    return [
      "A wall is a question the stone has been asking a long time.",
      "It does not expect to be answered.",
      "Push only what you can bring back.",
    ];
  }
  return ["..."];
}

// ── Component ───────────────────────────────────────────────────
export default function SoulSearcher() {
  const cvs = useRef(null);
  const [gs, setGs] = useState("intro");
  const [seed] = useState(42);
  const [roomX, setRoomX] = useState(0);
  const [roomY, setRoomY] = useState(0);
  const [lightWorld, setLightWorld] = useState(null);
  const [shadowWorld, setShadowWorld] = useState(null);
  const [lightPos, setLightPos] = useState({ x: 7, y: 7 });
  const [shadowPos, setShadowPos] = useState({ x: 7, y: 7 });
  const [splitMode, setSplitMode] = useState(null); // "light" or "shadow"
  const [splitSteps, setSplitSteps] = useState(0);
  const [splitAnchor, setSplitAnchor] = useState(null);
  const [hp, setHp] = useState(MAX_HEARTS);
  const [ambushed, setAmbushed] = useState(false); // survived the first catch
  const [msg, setMsg] = useState("");
  const [dialogueLines, setDialogueLines] = useState(null);
  const [dialogueLine, setDialogueLine] = useState(0);
  const [shards, setShards] = useState(new Set());
  const [shardMsg, setShardMsg] = useState(null); // pickup notification
  const [caughtMsg, setCaughtMsg] = useState(null); // ghoul catch modal
  const [victory, setVictory] = useState(false);
  const [visited, setVisited] = useState(new Set(["0,0"]));
  const [steps, setSteps] = useState(0);
  const [chapter, setChapter] = useState(1);
  const [voidShards, setVoidShards] = useState(new Set());
  const [echoShards, setEchoShards] = useState(new Set());
  const [mirrorSteps, setMirrorSteps] = useState(0); // >0 = mirror mode active
  const [mirrorAnchor, setMirrorAnchor] = useState(null);
  const [lDoorsLatched, setLDoorsLatched] = useState(false);
  const [sDoorsLatched, setSDoorsLatched] = useState(false);
  const [linkedPlates, setLinkedPlates] = useState(false);

  useEffect(() => { const r = generateDualRoom(0, 0, 42, chapter); setLightWorld(r.light); setShadowWorld(r.shadow); setLinkedPlates(!!r.linkedPlates); }, []);

  const ds = useCallback((ctx, key, x, y, sc = 1) => {
    const d = S[key]; if (!d) return;
    for (let r = 0; r < d.length; r++) for (let c = 0; c < d[r].length; c++)
      if (d[r][c] === "1") ctx.fillRect(x + c * sc, y + r * sc, sc, sc);
  }, []);

  const wrap = useCallback((ctx, text, maxW) => {
    const words = text.split(" "); const lines = []; let line = "";
    for (const w of words) { const t = line ? line + " " + w : w; if (ctx.measureText(t).width > maxW) { if (line) lines.push(line); line = w; } else line = t; }
    if (line) lines.push(line); return lines;
  }, []);

  // ── Render ──────────────────────────────────────────────────
  const render = useCallback(() => {
    const c = cvs.current; if (!c) return;
    const ctx = c.getContext("2d");
    ctx.imageSmoothingEnabled = false;
    ctx.fillStyle = WHT; ctx.fillRect(0, 0, W, H); ctx.fillStyle = BLK;

    if (gs === "intro") {
      ctx.fillStyle = BLK; ctx.fillRect(0, 0, W, H); ctx.fillStyle = WHT;
      ctx.textAlign = "center"; ctx.textBaseline = "middle";
      // Title
      ctx.font = "bold 44px 'Courier New',monospace";
      ctx.fillText("DIPTYCH", W / 2, H / 2 - 140);
      // Dictionary-style definition — larger
      ctx.font = "italic 18px 'Courier New',monospace";
      ctx.fillText("/\u02C8diptik/  noun", W / 2, H / 2 - 80);
      ctx.font = "16px 'Courier New',monospace";
      ctx.fillText("An artwork made of two related panels", W / 2, H / 2 - 40);
      ctx.fillText("that are meant to be viewed together", W / 2, H / 2 - 15);
      ctx.fillText("as one piece.", W / 2, H / 2 + 10);
      // Player sprite + prompt
      ds(ctx, "player", W / 2 - 24, H / 2 + 80, 2);
      ctx.font = "14px 'Courier New',monospace";
      ctx.fillText("[ ENTER ]", W / 2, H / 2 + 180);
      ctx.textAlign = "left"; ctx.textBaseline = "alphabetic"; return;
    }

    if (gs === "controls") {
      ctx.fillStyle = BLK; ctx.fillRect(0, 0, W, H); ctx.fillStyle = WHT;
      ctx.textAlign = "center"; ctx.textBaseline = "middle";
      ctx.font = "bold 20px 'Courier New',monospace";
      ctx.fillText("CONTROLS", W / 2, 110);
      ctx.fillRect(W / 2 - 60, 135, 120, 2);
      // Rows: key(s) column on left, label column on right
      const rows = [
        { key: "\u2191 \u2193 \u2190 \u2192", label: "walk in both worlds" },
        { key: "1", label: "light walks alone" },
        { key: "2", label: "shadow walks alone" },
        { key: "ENTER", label: "speak, confirm" },
        { key: "ESC", label: "end a walk" },
      ];
      ctx.font = "bold 17px 'Courier New',monospace";
      const startY = 200, rowH = 56;
      rows.forEach((r, i) => {
        const y = startY + i * rowH;
        ctx.textAlign = "right";
        ctx.fillText(r.key, W / 2 - 20, y);
        ctx.textAlign = "left";
        ctx.font = "17px 'Courier New',monospace";
        ctx.fillText(r.label, W / 2 + 20, y);
        ctx.font = "bold 17px 'Courier New',monospace";
      });
      ctx.textAlign = "center";
      ctx.font = "14px 'Courier New',monospace";
      ctx.fillText("[ ENTER to begin ]", W / 2, H - 60);
      ctx.textAlign = "left"; ctx.textBaseline = "alphabetic"; return;
    }

    if (gs === "victory") {
      ctx.fillStyle = BLK; ctx.fillRect(0, 0, W, H); ctx.fillStyle = WHT;
      ctx.font = "bold 20px 'Courier New',monospace";
      ctx.textAlign = "center"; ctx.textBaseline = "middle";
      if (chapter >= 3) {
        ctx.fillText("Fifteen pieces returned.", W / 2, 180);
        ctx.font = "16px 'Courier New',monospace";
        ctx.fillText("One figure stands on the hinge.", W / 2, 230);
        ctx.fillText("The panels close.", W / 2, 270);
        ds(ctx, "player", W / 2 - 36, 330);
        ctx.fillStyle = BLK; ctx.fillRect(W / 2 + 4, 330, TILE, TILE);
        ctx.fillStyle = WHT; ds(ctx, "player", W / 2 + 4, 330);
        ctx.font = "italic 16px 'Courier New',monospace";
        ctx.fillText("\u2014 walk \u2014", W / 2, 420);
        ctx.font = "12px 'Courier New',monospace";
        ctx.fillText(`${steps} steps \u00b7 ${visited.size} rooms`, W / 2, 500);
        ctx.font = "14px 'Courier New',monospace";
        ctx.fillText("[ ENTER ]", W / 2, 570);
      } else if (chapter === 2) {
        ctx.fillText("Two seams sewn.", W / 2, 180);
        ctx.font = "16px 'Courier New',monospace";
        ctx.fillText("A third is watching.", W / 2, 240);
        ds(ctx, "player", W / 2 - 36, 320);
        ctx.fillStyle = BLK; ctx.fillRect(W / 2 + 4, 320, TILE, TILE);
        ctx.fillStyle = WHT; ds(ctx, "player", W / 2 + 4, 320);
        ctx.font = "bold 14px 'Courier New',monospace";
        ctx.fillText("Chapter III", W / 2, 420);
        ctx.font = "italic 16px 'Courier New',monospace";
        ctx.fillText("THE HINGE", W / 2, 450);
        ctx.font = "12px 'Courier New',monospace";
        ctx.fillText(`${steps} steps \u00b7 ${visited.size} rooms`, W / 2, 510);
        ctx.font = "14px 'Courier New',monospace";
        ctx.fillText("[ ENTER ]", W / 2, 570);
      } else {
        ctx.fillText("The surface closes.", W / 2, 180);
        ctx.font = "16px 'Courier New',monospace";
        ctx.fillText("The deep opens its eye.", W / 2, 240);
        ds(ctx, "player", W / 2 - 36, 320);
        ctx.fillStyle = BLK; ctx.fillRect(W / 2 + 4, 320, TILE, TILE);
        ctx.fillStyle = WHT; ds(ctx, "player", W / 2 + 4, 320);
        ctx.font = "bold 14px 'Courier New',monospace";
        ctx.fillText("Chapter II", W / 2, 420);
        ctx.font = "italic 16px 'Courier New',monospace";
        ctx.fillText("THE WOUND BENEATH", W / 2, 450);
        ctx.font = "12px 'Courier New',monospace";
        ctx.fillText(`${steps} steps \u00b7 ${visited.size} rooms`, W / 2, 510);
        ctx.font = "14px 'Courier New',monospace";
        ctx.fillText("[ ENTER ]", W / 2, 570);
      }
      ctx.textAlign = "left"; ctx.textBaseline = "alphabetic"; return;
    }

    if (!lightWorld || !shadowWorld) return;

    const drawShardTileMarker = (px, py, fg) => {
      ctx.fillStyle = fg;
      const t = 5;
      ctx.fillRect(px + 1, py + 1, t, 1); ctx.fillRect(px + 1, py + 1, 1, t);
      ctx.fillRect(px + TILE - t - 1, py + 1, t, 1); ctx.fillRect(px + TILE - 2, py + 1, 1, t);
      ctx.fillRect(px + 1, py + TILE - 2, t, 1); ctx.fillRect(px + 1, py + TILE - t - 1, 1, t);
      ctx.fillRect(px + TILE - t - 1, py + TILE - 2, t, 1); ctx.fillRect(px + TILE - 2, py + TILE - t - 1, 1, t);
      drawPixelDiamond(ctx, px + 12, py + 12, 10, false);
      drawPixelDiamond(ctx, px + 12, py + 12, 8, false);
      drawPixelDiamond(ctx, px + 12, py + 12, 3, true);
    };

    const drawSplitMeter = (mode, oy, inverse) => {
      const active = splitMode === mode;
      const remaining = active ? splitSteps : 0;
      const x = VIEW_OX - 28, y0 = oy + VIEW_H / 2 - 34;
      if (inverse) {
        ctx.fillStyle = BLK;
        ctx.fillRect(x - 9, y0 - 8, 18, 84);
        ctx.strokeStyle = BLK; ctx.lineWidth = 1;
        ctx.strokeRect(x - 9, y0 - 8, 18, 84);
      }
      for (let i = 0; i < SPLIT_STEPS; i++) {
        const filled = i < remaining;
        ctx.fillStyle = inverse ? WHT : BLK;
        drawPixelDiamond(ctx, x, y0 + i * 16, 5, filled);
      }
    };

    // ─── Compute pressure plate state for each world ───
    // Plates in light world open doors in shadow world, and vice versa.
    // Latched doors stay open for the rest of the room visit.
    // In linked mode, live pressure requires BOTH plates pressed simultaneously.
    const lLiveR = lightWorld && isPlatePressed(lightWorld, lightPos);
    const sLiveR = shadowWorld && isPlatePressed(shadowWorld, shadowPos);
    const bothLiveR = lLiveR && sLiveR;
    const lightPlatePressed = sDoorsLatched || (linkedPlates ? bothLiveR : lLiveR);
    const shadowPlatePressed = lDoorsLatched || (linkedPlates ? bothLiveR : sLiveR);

    // ─── Draw one world ───
    const drawWorld = (world, pos, oy, isShadow, frozen, doorsOpen) => {
      const fg = isShadow ? WHT : BLK;
      const bg = isShadow ? BLK : WHT;
      ctx.fillStyle = bg; ctx.fillRect(VIEW_OX, oy, VIEW_W, VIEW_H);
      ctx.strokeStyle = fg; ctx.lineWidth = 1;
      ctx.strokeRect(VIEW_OX - 1, oy - 1, VIEW_W + 2, VIEW_H + 2);

      // Ground texture
      ctx.fillStyle = fg;
      const gr = mulberry32(seed + roomX * 100 + roomY + (isShadow ? 99999 : 0));
      for (let i = 0; i < (isShadow ? 15 : 30); i++) {
        ctx.fillRect(VIEW_OX + Math.floor(gr() * VIEW_W), oy + Math.floor(gr() * VIEW_H),
          1, isShadow ? 1 : (gr() < 0.5 ? 2 : 1));
      }

      // Tiles
      for (let y = 0; y < GRID; y++) for (let x = 0; x < GRID; x++) {
        const tile = world.tiles[y][x];
        const px = VIEW_OX + x * TILE, py = oy + y * TILE;
        ctx.fillStyle = fg;
        if (tile === T.WALL) ds(ctx, "wall", px, py);
        else if (tile === T.TREE) ds(ctx, "tree", px, py);
        else if (tile === T.DOOR) {
          if (doorsOpen) {
            // Open door: faint dotted outline
            for (let di = 0; di < TILE; di += 4) {
              ctx.fillRect(px + di, py, 2, 1);
              ctx.fillRect(px + di, py + TILE - 1, 2, 1);
              ctx.fillRect(px, py + di, 1, 2);
              ctx.fillRect(px + TILE - 1, py + di, 1, 2);
            }
          } else {
            ds(ctx, "door", px, py);
          }
        }
        else if (tile === T.SWITCH) ds(ctx, "plate", px, py);
        else if (tile === T.MIRROR) ds(ctx, "mirror_tile", px, py);
        else if (tile === T.WATER) {
          for (let wy = 3; wy < TILE; wy += 6)
            for (let wx = 0; wx < TILE; wx++)
              ctx.fillRect(px + wx, py + wy + ((wx >> 2) & 1), 1, 1);
        }
      }

      // Entities
      ctx.fillStyle = fg;
      world.entities.forEach(e => {
        const ex = VIEW_OX + e.x * TILE, ey2 = oy + e.y * TILE;
        ds(ctx, e.sprite || "npc", ex, ey2);
        if (e.type === "half_light" || e.type === "half_shadow") drawShardTileMarker(ex, ey2, fg);
      });

      // Player
      ctx.fillStyle = fg;
      const ppx = VIEW_OX + pos.x * TILE, ppy = oy + pos.y * TILE;
      ds(ctx, "player", ppx, ppy);

      // Freeze brackets
      if (frozen) {
        ctx.fillStyle = fg;
        const b = 5, bx2 = ppx - 3, by2 = ppy - 3, bw2 = TILE + 6, bh2 = TILE + 6;
        ctx.fillRect(bx2, by2, b, 2); ctx.fillRect(bx2, by2, 2, b);
        ctx.fillRect(bx2 + bw2 - b, by2, b, 2); ctx.fillRect(bx2 + bw2 - 2, by2, 2, b);
        ctx.fillRect(bx2, by2 + bh2 - 2, b, 2); ctx.fillRect(bx2, by2 + bh2 - b, 2, b);
        ctx.fillRect(bx2 + bw2 - b, by2 + bh2 - 2, b, 2); ctx.fillRect(bx2 + bw2 - 2, by2 + bh2 - b, 2, b);
      }
    };

    // LIGHT WORLD
    ctx.fillStyle = BLK; ctx.font = "bold 11px 'Courier New',monospace"; ctx.textAlign = "center";
    ctx.fillText("~ Light World ~", W / 2, 16);
    drawWorld(lightWorld, lightPos, LIGHT_OY, false, splitMode === "shadow", shadowPlatePressed);
    drawSplitMeter("light", LIGHT_OY, false);

    // ── MINIMAP (right margin of light world) ──
    const MN = 7, MC = 7;
    const MX = VIEW_OX + VIEW_W + 5, MY = LIGHT_OY + 5;
    ctx.fillStyle = BLK;
    ctx.font = "bold 7px 'Courier New',monospace"; ctx.textAlign = "center";
    ctx.fillText("MAP", MX + (MN * MC) / 2, MY - 2);
    // Grid border
    ctx.strokeStyle = BLK; ctx.lineWidth = 1;
    ctx.strokeRect(MX - 1, MY - 1, MN * MC + 2, MN * MC + 2);
    const half = Math.floor(MN / 2);
    for (let my = 0; my < MN; my++) for (let mx = 0; mx < MN; mx++) {
      const mrx = roomX + mx - half, mry = roomY + my - half;
      const key = `${mrx},${mry}`;
      const px = MX + mx * MC, py = MY + my * MC;
      const isCur = mx === half && my === half;
      const isVis = visited.has(key);
      const hasShard = SHARD_LOCS.some(s => s.rx === mrx && s.ry === mry && !shards.has(s.id)) ||
        (chapter >= 2 && VOID_SHARD_LOCS.some(s => s.rx === mrx && s.ry === mry && !voidShards.has(s.id))) ||
        (chapter >= 3 && ECHO_SHARD_LOCS.some(s => s.rx === mrx && s.ry === mry && !echoShards.has(s.id)));

      const isHome = mrx === 0 && mry === 0 && !isCur;
      ctx.fillStyle = BLK;
      if (isCur) {
        // Current room: filled square with white center dot
        ctx.fillRect(px + 1, py + 1, MC - 2, MC - 2);
        ctx.fillStyle = WHT;
        ctx.fillRect(px + 3, py + 3, 1, 1);
        ctx.fillStyle = BLK;
      } else if (isHome) {
        // Home/Watcher: outlined square (distinct from dot and star)
        ctx.strokeStyle = BLK; ctx.lineWidth = 1;
        ctx.strokeRect(px + 1, py + 1, MC - 2, MC - 2);
      } else if (hasShard) {
        // Shard room: filled diamond, intentionally larger than visited dots.
        const cx2 = px + 3, cy2 = py + 3;
        drawPixelDiamond(ctx, cx2, cy2, 3, true);
      } else if (isVis) {
        // Visited: small single dot
        ctx.fillRect(px + 3, py + 3, 1, 1);
      }
    }

    // DIVIDER + shard indicators
    ctx.fillStyle = BLK;
    ctx.fillRect(VIEW_OX - 8, 388, VIEW_W + 16, 1);
    // Shard diamonds — Chapter 1 shards + Chapter 2 void shards
    const drawDiamondRow = (count, prefix, collected, baseX, sy) => {
      for (let i = 0; i < count; i++) {
        const sx = baseX + i * 14;
        ctx.fillStyle = BLK;
        if (collected.has(`${prefix}${i}`)) {
          ctx.fillRect(sx + 3, sy, 1, 1); ctx.fillRect(sx + 2, sy + 1, 3, 1);
          ctx.fillRect(sx + 1, sy + 2, 5, 1); ctx.fillRect(sx, sy + 3, 7, 1);
          ctx.fillRect(sx + 1, sy + 4, 5, 1); ctx.fillRect(sx + 2, sy + 5, 3, 1);
          ctx.fillRect(sx + 3, sy + 6, 1, 1);
        } else {
          ctx.fillRect(sx + 3, sy, 1, 1);
          ctx.fillRect(sx + 2, sy + 1, 1, 1); ctx.fillRect(sx + 4, sy + 1, 1, 1);
          ctx.fillRect(sx + 1, sy + 2, 1, 1); ctx.fillRect(sx + 5, sy + 2, 1, 1);
          ctx.fillRect(sx, sy + 3, 1, 1); ctx.fillRect(sx + 6, sy + 3, 1, 1);
          ctx.fillRect(sx + 1, sy + 4, 1, 1); ctx.fillRect(sx + 5, sy + 4, 1, 1);
          ctx.fillRect(sx + 2, sy + 5, 1, 1); ctx.fillRect(sx + 4, sy + 5, 1, 1);
          ctx.fillRect(sx + 3, sy + 6, 1, 1);
        }
      }
    };
    if (chapter >= 3) {
      // Three groups side-by-side: Ch1 | Ch2 | Ch3
      // Each diamond is 7px wide, spaced 14px apart → 5 diamonds span 14*4 + 7 = 63px.
      const groupW = 14 * (TOTAL_SHARDS - 1) + 7;
      const gap = 14;
      const totalW = groupW * 3 + gap * 2;
      const baseX = Math.floor(W / 2 - totalW / 2);
      drawDiamondRow(TOTAL_SHARDS, "shard_", shards, baseX, 391);
      drawDiamondRow(TOTAL_VOID_SHARDS, "void_", voidShards, baseX + groupW + gap, 391);
      drawDiamondRow(TOTAL_ECHO_SHARDS, "echo_", echoShards, baseX + (groupW + gap) * 2, 391);
    } else if (chapter >= 2) {
      // Show both rows: Ch1 shards left, Ch2 void shards right
      drawDiamondRow(TOTAL_SHARDS, "shard_", shards, W / 2 - TOTAL_SHARDS * 14 - 4, 391);
      drawDiamondRow(TOTAL_VOID_SHARDS, "void_", voidShards, W / 2 + 4, 391);
    } else {
      drawDiamondRow(TOTAL_SHARDS, "shard_", shards, W / 2 - TOTAL_SHARDS * 7, 391);
    }
    ctx.font = "bold 10px 'Courier New',monospace"; ctx.textAlign = "center";
    ctx.fillText("~ Shadow World ~", W / 2, 405);
    ctx.fillRect(VIEW_OX - 8, 407, VIEW_W + 16, 1);

    // ── MIRROR METER (left side of divider, when active) ──
    if (mirrorSteps > 0) {
      ctx.fillStyle = BLK;
      ctx.font = "bold 9px 'Courier New',monospace"; ctx.textAlign = "left";
      ctx.fillText("MIRROR", 8, 395);
      for (let i = 0; i < MIRROR_STEPS; i++) {
        const filled = i < mirrorSteps;
        drawPixelDiamond(ctx, 14 + i * 10, 400, 3, filled);
      }
    }
    // ── MIRROR ACTIVATION BANNER (shown on first frame of mirror) ──
    if (mirrorSteps === MIRROR_STEPS) {
      const bx = 30, by = 385, bw = W - 60, bh = 26;
      ctx.fillStyle = BLK; ctx.fillRect(bx, by, bw, bh);
      ctx.fillStyle = WHT;
      ctx.strokeStyle = WHT; ctx.strokeRect(bx + 2, by + 2, bw - 4, bh - 4);
      ctx.font = "bold 11px 'Courier New',monospace"; ctx.textAlign = "center";
      ctx.fillText("\u25C6 MIRROR ACTIVATED \u2014 5 INVERTED STEPS \u25C6", W / 2, by + 17);
    }

    // SHADOW WORLD
    drawWorld(shadowWorld, shadowPos, SHADOW_OY, true, splitMode === "light", lightPlatePressed);
    drawSplitMeter("shadow", SHADOW_OY, true);

    // ── ALIGNED SHARD INDICATOR ──
    const alH = lightWorld.entities.find(e => e.type === "half_light");
    const asH = shadowWorld.entities.find(e => e.type === "half_shadow");
    if (alH && asH && alH.x === asH.x && alH.y === asH.y) {
      // Draw merged shard in BOTH world views with thick border
      const ax = alH.x, ay = alH.y;
      // Light world: draw merged sprite + border
      const lpx = VIEW_OX + ax * TILE, lpy = LIGHT_OY + ay * TILE;
      ctx.fillStyle = BLK;
      ds(ctx, "shard_merged", lpx, lpy);
      ctx.strokeStyle = BLK; ctx.lineWidth = 3;
      ctx.strokeRect(lpx - 4, lpy - 4, TILE + 8, TILE + 8);
      ctx.lineWidth = 1;
      // Shadow world: same
      const spx = VIEW_OX + ax * TILE, spy = SHADOW_OY + ay * TILE;
      ctx.fillStyle = WHT;
      ds(ctx, "shard_merged", spx, spy);
      ctx.strokeStyle = WHT; ctx.lineWidth = 3;
      ctx.strokeRect(spx - 4, spy - 4, TILE + 8, TILE + 8);
      ctx.lineWidth = 1;
    }

    // STATUS BAR — hearts
    ctx.fillStyle = BLK;
    for (let i = 0; i < MAX_HEARTS; i++) {
      const hx = 8 + i * 14, hy = STATUS_Y + 4;
      if (i < hp) {
        // Filled heart
        ctx.fillRect(hx+1,hy,2,1); ctx.fillRect(hx+5,hy,2,1);
        ctx.fillRect(hx,hy+1,8,1);
        ctx.fillRect(hx,hy+2,8,1);
        ctx.fillRect(hx+1,hy+3,6,1);
        ctx.fillRect(hx+2,hy+4,4,1);
        ctx.fillRect(hx+3,hy+5,2,1);
      } else {
        // Empty heart outline
        ctx.fillRect(hx+1,hy,2,1); ctx.fillRect(hx+5,hy,2,1);
        ctx.fillRect(hx,hy+1,1,1); ctx.fillRect(hx+3,hy+1,2,1); ctx.fillRect(hx+7,hy+1,1,1);
        ctx.fillRect(hx,hy+2,1,1); ctx.fillRect(hx+7,hy+2,1,1);
        ctx.fillRect(hx+1,hy+3,1,1); ctx.fillRect(hx+6,hy+3,1,1);
        ctx.fillRect(hx+2,hy+4,1,1); ctx.fillRect(hx+5,hy+4,1,1);
        ctx.fillRect(hx+3,hy+5,2,1);
      }
    }
    ctx.textAlign = "center"; ctx.font = "bold 11px 'Courier New',monospace";
    // Show message in status center if active, otherwise show shard count
    if (msg && !shardMsg) {
      ctx.fillText(msg.length > 40 ? msg.slice(0, 40) + "\u2026" : msg, W / 2, STATUS_Y + 14);
    } else if (splitMode) {
      ctx.fillText(`${splitMode === "light" ? "Light" : "Shadow"} split: ${splitSteps}/${SPLIT_STEPS}`, W / 2, STATUS_Y + 14);
    } else {
      ctx.fillText(chapter >= 3
        ? `Echo: ${echoShards.size}/${TOTAL_ECHO_SHARDS}`
        : chapter >= 2
        ? `Void: ${voidShards.size}/${TOTAL_VOID_SHARDS}`
        : `Shards: ${shards.size}/${TOTAL_SHARDS}`, W / 2, STATUS_Y + 14);
    }
    ctx.textAlign = "right"; ctx.font = "11px 'Courier New',monospace";
    ctx.fillText(`(${roomX},${roomY})`, W - 8, STATUS_Y + 14);

    // SHARD FOUND MODAL
    if (shardMsg) {
      const bx2 = 40, by2 = 310, bw2 = W - 80, bh2 = 150;
      ctx.fillStyle = BLK; ctx.fillRect(bx2, by2, bw2, bh2);
      ctx.fillStyle = WHT; ctx.strokeStyle = WHT;
      ctx.strokeRect(bx2 + 3, by2 + 3, bw2 - 6, bh2 - 6);
      ds(ctx, "shard_merged", W / 2 - 12, by2 + 15);
      ctx.font = "bold 16px 'Courier New',monospace"; ctx.textAlign = "center";
      ctx.fillText("A PIECE RETURNS", W / 2, by2 + 65);
      ctx.font = "14px 'Courier New',monospace";
      ctx.fillText(shardMsg, W / 2, by2 + 95);
      ctx.font = "11px 'Courier New',monospace";
      ctx.fillText("[ ENTER ]", W / 2, by2 + 125);
    }

    // GHOUL CAUGHT MODAL
    if (caughtMsg) {
      const bx2 = 40, by2 = 260, bw2 = W - 80, bh2 = 240;
      ctx.fillStyle = BLK; ctx.fillRect(bx2, by2, bw2, bh2);
      ctx.fillStyle = WHT; ctx.strokeStyle = WHT;
      ctx.strokeRect(bx2 + 3, by2 + 3, bw2 - 6, bh2 - 6);
      ctx.font = "bold 18px 'Courier New',monospace"; ctx.textAlign = "center";
      if (caughtMsg === "dead") {
        ctx.fillText("THE HINGE OPENS", W / 2, by2 + 45);
        ctx.font = "13px 'Courier New',monospace";
        ctx.fillText("You fall out of yourself.", W / 2, by2 + 80);
        ctx.fillText("The pieces return to where", W / 2, by2 + 102);
        ctx.fillText("they were kept.", W / 2, by2 + 120);
        ctx.font = "italic 12px 'Courier New',monospace";
        ctx.fillText("Try again, if you are still who you were.", W / 2, by2 + 145);
      } else {
        ds(ctx, "ghoul", W / 2 - 12, by2 + 20);
        const lightLines = [
          "The light remembers you.",
          "You were not meant to be seen this long.",
          "It does not burn. It reads you.",
          "Something in the brightness had a name for you.",
          "The light was never empty.",
          "A white silence closes.",
          "You were the shadow it needed.",
        ];
        const shadowLines = [
          "The dark leans closer.",
          "Something without a mouth says your name.",
          "The black was not a color. It was a waiting.",
          "You feel recognized. It is worse than being caught.",
          "The shadow makes room for another shadow.",
          "A cold patience touches you.",
          "What you feared was kind compared to this.",
        ];
        const lines = caughtMsg === "light" ? lightLines : shadowLines;
        ctx.font = "bold 12px 'Courier New',monospace";
        // Wrap long lines manually
        const line = lines[steps % lines.length];
        const wrapped = wrap(ctx, line, bw2 - 40);
        wrapped.forEach((l, i) => ctx.fillText(l, W / 2, by2 + 75 + i * 18));
      }
      // Draw remaining hearts
      const hx0 = W / 2 - MAX_HEARTS * 7;
      for (let i = 0; i < MAX_HEARTS; i++) {
        const hx = hx0 + i * 14, hy = by2 + 150;
        ctx.fillStyle = WHT;
        if (i < hp) {
          ctx.fillRect(hx+1,hy,2,1); ctx.fillRect(hx+5,hy,2,1);
          ctx.fillRect(hx,hy+1,8,1); ctx.fillRect(hx,hy+2,8,1);
          ctx.fillRect(hx+1,hy+3,6,1); ctx.fillRect(hx+2,hy+4,4,1); ctx.fillRect(hx+3,hy+5,2,1);
        } else {
          ctx.fillRect(hx+1,hy,2,1); ctx.fillRect(hx+5,hy,2,1);
          ctx.fillRect(hx,hy+1,1,1); ctx.fillRect(hx+3,hy+1,2,1); ctx.fillRect(hx+7,hy+1,1,1);
          ctx.fillRect(hx,hy+2,1,1); ctx.fillRect(hx+7,hy+2,1,1);
          ctx.fillRect(hx+1,hy+3,1,1); ctx.fillRect(hx+6,hy+3,1,1);
          ctx.fillRect(hx+2,hy+4,1,1); ctx.fillRect(hx+5,hy+4,1,1);
          ctx.fillRect(hx+3,hy+5,2,1);
        }
      }
      ctx.fillStyle = WHT; ctx.font = "11px 'Courier New',monospace";
      ctx.fillText("[ ENTER ]", W / 2, by2 + bh2 - 20);
    }

    // DIALOGUE
    if (gs === "dialogue" && dialogueLines) {
      const bx2 = 30, by2 = 280, bw2 = W - 60, bh2 = 220;
      ctx.fillStyle = WHT; ctx.fillRect(bx2, by2, bw2, bh2);
      ctx.fillStyle = BLK;
      ctx.strokeRect(bx2, by2, bw2, bh2); ctx.strokeRect(bx2 + 3, by2 + 3, bw2 - 6, bh2 - 6);
      ctx.font = "bold 13px 'Courier New',monospace"; ctx.textAlign = "left";
      ctx.fillText("...", bx2 + 18, by2 + 28);
      ctx.fillRect(bx2 + 10, by2 + 36, bw2 - 20, 1);
      ctx.font = "14px 'Courier New',monospace";
      wrap(ctx, dialogueLines[dialogueLine], bw2 - 40).forEach((l, i) => {
        ctx.fillText(l, bx2 + 20, by2 + 60 + i * 22);
      });
      ctx.font = "10px 'Courier New',monospace"; ctx.textAlign = "right";
      ctx.fillText(`${dialogueLine + 1}/${dialogueLines.length}  [ENTER]`, bx2 + bw2 - 15, by2 + bh2 - 15);
    }

    ctx.textAlign = "left"; ctx.textBaseline = "alphabetic";
  }, [gs, lightWorld, shadowWorld, lightPos, shadowPos, splitMode, splitSteps,
    hp, roomX, roomY, msg, dialogueLines, dialogueLine, seed, shards, shardMsg, caughtMsg,
    visited, steps, chapter, voidShards, echoShards, mirrorSteps, lDoorsLatched, sDoorsLatched, linkedPlates, ds, wrap]);

  // ── Ghoul tick + respawn ──────────────────────────────────────
  const tickGhoulsAndCheck = useCallback((lp, sp) => {
    // Move ghouls and check collision synchronously using current world state
    if (!lightWorld || !shadowWorld) return;
    const hasLGhouls = lightWorld.entities.some(e => e.type === "ghoul");
    const hasSGhouls = shadowWorld.entities.some(e => e.type === "ghoul");
    if (!hasLGhouls && !hasSGhouls) return;

    const newLEnts = hasLGhouls ? moveGhouls(lightWorld.entities, lightWorld.tiles, lp) : lightWorld.entities;
    const newSEnts = hasSGhouls ? moveGhouls(shadowWorld.entities, shadowWorld.tiles, sp) : shadowWorld.entities;

    const lHit = checkGhoulCollision(newLEnts, lp);
    const sHit = checkGhoulCollision(newSEnts, sp);

    if (lHit || sHit) {
      // Ghoul caught — show modal, defer respawn to dismissal
      const newHp = Math.max(0, hp - GHOUL_DMG);
      setHp(newHp);
      if (!ambushed) setAmbushed(true);
      if (newHp <= 0) {
        setCaughtMsg("dead");
      } else {
        setCaughtMsg(lHit ? "light" : "shadow");
      }
    } else {
      // Just update ghoul positions
      if (hasLGhouls) setLightWorld(w => ({ ...w, entities: newLEnts }));
      if (hasSGhouls) setShadowWorld(w => ({ ...w, entities: newSEnts }));
    }
  }, [lightWorld, shadowWorld, ambushed, seed, chapter]);

  // ── Input ─────────────────────────────────────────────────────
  const handleKey = useCallback((e) => {
    e.preventDefault(); const key = e.key;

    // Shard modal dismiss
    if (shardMsg) { if (key === "Enter" || key === "Escape" || key === "1" || key === "2") setShardMsg(null); return; }

    // Ghoul catch modal dismiss
    if (caughtMsg) {
      if (key === "Enter" || key === "Escape") {
        const isDead = caughtMsg === "dead";
        setCaughtMsg(null);
        // Respawn at Watcher room
        const home = generateDualRoom(0, 0, seed, chapter);
        setRoomX(0); setRoomY(0);
        setLightWorld(home.light); setShadowWorld(home.shadow);
        setLightPos({ x: 7, y: 7 }); setShadowPos({ x: 7, y: 7 });
        setSplitMode(null); setSplitSteps(0); setSplitAnchor(null);
        setMirrorSteps(0); setMirrorAnchor(null);
        setLDoorsLatched(false); setSDoorsLatched(false);
        setLinkedPlates(!!home.linkedPlates);
        if (isDead) {
          // Full reset of current chapter
          setHp(MAX_HEARTS);
          if (chapter >= 3) {
            setEchoShards(new Set());
            setMsg("The reflections forget you. Begin again.");
          } else if (chapter >= 2) {
            setVoidShards(new Set());
            setMsg("The deep takes back what it lent. Begin again.");
          } else {
            setShards(new Set());
            setMsg("The pieces scatter into the dark. Begin again.");
          }
        } else {
          setMsg("You wake where she waits. She does not look up.");
        }
      }
      return;
    }

    if (gs === "intro") {
      if (key === "Enter") { setGs("controls"); }
      // DEV: press 0 to skip to Chapter 2, press 9 to skip to Chapter 3
      if (key === "0") {
        setShards(new Set(["shard_0","shard_1","shard_2","shard_3","shard_4"]));
        setChapter(2); setGs("play"); setMsg("The deep opens its eye. Five pieces in the dark.");
      }
      if (key === "9") {
        setShards(new Set(["shard_0","shard_1","shard_2","shard_3","shard_4"]));
        setVoidShards(new Set(["void_0","void_1","void_2","void_3","void_4"]));
        setChapter(3); setGs("play"); setMsg("The hinge watches. Five reflections wait.");
      }
      return;
    }
    if (gs === "controls") {
      if (key === "Enter") { setGs("play"); setMsg("Find the one who waits. She will know you are two."); }
      return;
    }
    if (gs === "victory") {
      if (key === "Enter") {
        if (chapter === 1) {
          setChapter(2); setVictory(false); setGs("play");
          setMsg("The deep opens its eye. Five pieces in the dark.");
        } else if (chapter === 2) {
          setChapter(3); setVictory(false); setGs("play");
          setMsg("The hinge watches. Five reflections wait.");
        } else {
          setGs("play"); setMsg("The panels close. Walk where you will.");
        }
      }
      return;
    }

    if (gs === "dialogue") {
      if (key === "Enter") {
        if (dialogueLine < dialogueLines.length - 1) setDialogueLine(l => l + 1);
        else {
          setGs("play"); setDialogueLines(null); setDialogueLine(0); setMsg("");
          if (chapter === 1 && shards.size >= TOTAL_SHARDS && !victory) {
            setVictory(true); setGs("victory");
          } else if (chapter === 2 && voidShards.size >= TOTAL_VOID_SHARDS && !victory) {
            setVictory(true); setGs("victory");
          } else if (chapter >= 3 && echoShards.size >= TOTAL_ECHO_SHARDS && !victory) {
            setVictory(true); setGs("victory");
          }
        }
      } else if (key === "Escape") { setGs("play"); setDialogueLines(null); setDialogueLine(0); setMsg(""); }
      return;
    }

    if (gs !== "play" || !lightWorld || !shadowWorld) return;

    const snapSplit = (text = "The thread pulls you home.") => {
      if (!splitMode || !splitAnchor) return;
      if (splitMode === "light") setLightPos(splitAnchor);
      else setShadowPos(splitAnchor);
      setSplitMode(null);
      setSplitSteps(0);
      setSplitAnchor(null);
      setMsg(text);
    };

    let dx = 0, dy = 0;
    if (key === "ArrowUp") dy = -1; else if (key === "ArrowDown") dy = 1;
    else if (key === "ArrowLeft") dx = -1; else if (key === "ArrowRight") dx = 1;

    if (dx !== 0 || dy !== 0) {
      // Pressure plate state: plates in other world open doors in this world.
      // Doors also stay open once latched (pressed any time this room visit).
      // In linked mode, live pressure requires BOTH plates pressed simultaneously.
      const lLive = shadowWorld && isPlatePressed(shadowWorld, shadowPos);
      const sLive = lightWorld && isPlatePressed(lightWorld, lightPos);
      const bothLive = lLive && sLive;
      const lDoorsOpen = lDoorsLatched || (linkedPlates ? bothLive : lLive);
      const sDoorsOpen = sDoorsLatched || (linkedPlates ? bothLive : sLive);

      if (splitMode) {
        if (!splitAnchor || splitSteps <= 0) { snapSplit(); return; }
        const soloLight = splitMode === "light";
        const soloWorld = soloLight ? lightWorld : shadowWorld;
        const soloPos = soloLight ? lightPos : shadowPos;
        const nx = soloPos.x + dx, ny = soloPos.y + dy;

        if (nx < 0 || nx >= GRID || ny < 0 || ny >= GRID) {
          setMsg("The thread holds. It will not let you go there.");
          return;
        }

        const halfType = soloLight ? "half_light" : "half_shadow";
        const soloDoorsOpen = soloLight ? lDoorsOpen : sDoorsOpen;
        const soloHalf = soloWorld.entities.find(e => e.type === halfType);
        const soloPush = soloHalf && soloHalf.x === nx && soloHalf.y === ny;
        const soloBlock = soloWorld.entities.some(e => isBlockingEntity(e) && e.x === nx && e.y === ny);
        let ok = true;
        if (soloPush) {
          const pdx = nx + dx, pdy = ny + dy;
          if (pdx < 0 || pdx >= GRID || pdy < 0 || pdy >= GRID || !isWalkable(soloWorld.tiles[pdy][pdx], soloDoorsOpen) ||
              soloWorld.entities.some(e => e !== soloHalf && e.x === pdx && e.y === pdy)) {
            ok = false;
          }
        } else {
          ok = !soloBlock && isWalkable(soloWorld.tiles[ny][nx], soloDoorsOpen);
        }

        if (!ok) {
          setMsg(soloLight ? "The upper world refuses you." : "The lower world refuses you.");
          return;
        }

        if (soloPush) {
          const setWorld = soloLight ? setLightWorld : setShadowWorld;
          setWorld(w => ({ ...w, entities: w.entities.map(e =>
            e === soloHalf ? { ...e, x: nx + dx, y: ny + dy } : e) }));
        }
        if (soloLight) setLightPos({ x: nx, y: ny });
        else setShadowPos({ x: nx, y: ny });
        setSteps(s => s + 1);

        const remaining = splitSteps - 1;
        setSplitSteps(remaining);
        const movedHalf = soloPush ? { x: nx + dx, y: ny + dy } : (soloHalf || {});
        const otherHalf = soloLight
          ? shadowWorld.entities.find(e => e.type === "half_shadow")
          : lightWorld.entities.find(e => e.type === "half_light");
        if (movedHalf.x === otherHalf?.x && movedHalf.y === otherHalf?.y) {
          setMsg("They want to be one. Let the thread return.");
        } else if (remaining <= 0) {
          setMsg("The thread is spent. One more step, and it pulls you home.");
        } else {
          setMsg(`${soloLight ? "Light" : "Shadow"} walks alone. ${remaining} breaths remain.`);
        }
        // Ghouls advance in the active split world only
        const newLp = soloLight ? { x: nx, y: ny } : lightPos;
        const newSp = soloLight ? shadowPos : { x: nx, y: ny };
        tickGhoulsAndCheck(newLp, newSp);
        return;
      }

      // In mirror mode, shadow moves in the OPPOSITE direction
      const mdx = mirrorSteps > 0 ? -dx : dx;
      const mdy = mirrorSteps > 0 ? -dy : dy;
      const nlx = lightPos.x + dx, nly = lightPos.y + dy;
      const nsx = shadowPos.x + mdx, nsy = shadowPos.y + mdy;
      const lOff = nlx < 0 || nlx >= GRID || nly < 0 || nly >= GRID;
      const sOff = nsx < 0 || nsx >= GRID || nsy < 0 || nsy >= GRID;

      if (lOff || sOff) {
        // Don't allow room transitions during mirror mode
        if (mirrorSteps > 0) { setMsg("The eye will not let you leave."); return; }
        let nrx = roomX, nry = roomY, ex, ey;
        if (dx === 1) { nrx++; ex = 1; ey = snap(lightPos.y); }
        else if (dx === -1) { nrx--; ex = GRID - 2; ey = snap(lightPos.y); }
        else if (dy === 1) { nry++; ey = 1; ex = snap(lightPos.x); }
        else { nry--; ey = GRID - 2; ex = snap(lightPos.x); }
        // Bounded world: nothing beyond radius 4
        if (Math.abs(nrx) > 4 || Math.abs(nry) > 4) {
          setMsg("There is nothing beyond. The page ends here.");
          return;
        }
        const room = generateDualRoom(nrx, nry, seed, chapter);
        // Filter out already-collected shard halves
        const isCollected = e => (e.type === "half_light" || e.type === "half_shadow") &&
          (shards.has(e.id.replace(/_[ls]$/, "")) || voidShards.has(e.id.replace(/_[ls]$/, "")) || echoShards.has(e.id.replace(/_[ls]$/, "")));
        room.light.entities = room.light.entities.filter(e => !isCollected(e));
        room.shadow.entities = room.shadow.entities.filter(e => !isCollected(e));
        if (isWalkable(room.light.tiles[ey][ex]) && isWalkable(room.shadow.tiles[ey][ex]) &&
            !room.light.entities.some(e => isBlockingEntity(e) && e.x === ex && e.y === ey) &&
            !room.shadow.entities.some(e => isBlockingEntity(e) && e.x === ex && e.y === ey)) {
          setRoomX(nrx); setRoomY(nry);
          setLightWorld(room.light); setShadowWorld(room.shadow);
          setLightPos({ x: ex, y: ey }); setShadowPos({ x: ex, y: ey });
          setVisited(v => { const s = new Set(v); s.add(`${nrx},${nry}`); return s; });
          setSteps(s => s + 1); setMsg("");
          // Reset door latches when entering a new room
          setLDoorsLatched(false); setSDoorsLatched(false);
          setLinkedPlates(!!room.linkedPlates);
        } else { setMsg("The way is not yet."); }
        return;
      }

      // ── Check for aligned shard collection ──
      const lHalf = lightWorld.entities.find(e => e.type === "half_light");
      const sHalf = shadowWorld.entities.find(e => e.type === "half_shadow");
      const aligned = lHalf && sHalf && lHalf.x === sHalf.x && lHalf.y === sHalf.y;
      if (aligned && nlx === lHalf.x && nly === lHalf.y && nsx === sHalf.x && nsy === sHalf.y) {
        // Collect the aligned shard!
        const shardId = (lHalf.id || "").replace("_l", "");
        setLightPos({ x: nlx, y: nly });
        setShadowPos({ x: nsx, y: nsy });
        setLightWorld(w => ({ ...w, entities: w.entities.filter(e => e.type !== "half_light") }));
        setShadowWorld(w => ({ ...w, entities: w.entities.filter(e => e.type !== "half_shadow") }));
        if (shardId.startsWith("echo_")) {
          setEchoShards(prev => { const ns = new Set(prev); ns.add(shardId); return ns; });
          const ct = echoShards.size + 1;
          setShardMsg(ECHO_PICKUP_LINES[ct - 1] || `${ct}/${TOTAL_ECHO_SHARDS} - The echo sings.`);
        } else if (shardId.startsWith("void_")) {
          setVoidShards(prev => { const ns = new Set(prev); ns.add(shardId); return ns; });
          const ct = voidShards.size + 1;
          setShardMsg(VOID_PICKUP_LINES[ct - 1] || `${ct}/${TOTAL_VOID_SHARDS} - The void sings.`);
        } else {
          setShards(prev => { const ns = new Set(prev); ns.add(shardId); return ns; });
          const ct = shards.size + 1;
          setShardMsg(SHARD_PICKUP_LINES[ct - 1] || `${ct}/${TOTAL_SHARDS} - The shard sings.`);
        }
        setSteps(s => s + 1);
        return;
      }

      // ── Check for half-shard pushing ──
      const lPush = lightWorld.entities.find(e => (e.type === "half_light") && e.x === nlx && e.y === nly);
      const sPush = shadowWorld.entities.find(e => (e.type === "half_shadow") && e.x === nsx && e.y === nsy);

      // Validate pushes (shadow uses mirrored direction in mirror mode)
      let lOk = true, sOk = true;
      if (lPush) {
        const pdx = nlx + dx, pdy = nly + dy;
        if (pdx < 0 || pdx >= GRID || pdy < 0 || pdy >= GRID || !isWalkable(lightWorld.tiles[pdy][pdx], lDoorsOpen) ||
            lightWorld.entities.some(e => e !== lPush && e.x === pdx && e.y === pdy)) {
          lOk = false;
        }
      } else {
        lOk = isWalkable(lightWorld.tiles[nly][nlx], lDoorsOpen) &&
          !lightWorld.entities.some(e => isBlockingEntity(e) && e.x === nlx && e.y === nly);
      }
      if (sPush) {
        const pdx = nsx + mdx, pdy = nsy + mdy;
        if (pdx < 0 || pdx >= GRID || pdy < 0 || pdy >= GRID || !isWalkable(shadowWorld.tiles[pdy][pdx], sDoorsOpen) ||
            shadowWorld.entities.some(e => e !== sPush && e.x === pdx && e.y === pdy)) {
          sOk = false;
        }
      } else {
        sOk = isWalkable(shadowWorld.tiles[nsy][nsx], sDoorsOpen) &&
          !shadowWorld.entities.some(e => isBlockingEntity(e) && e.x === nsx && e.y === nsy);
      }

      if (lOk && sOk) {
        // Execute pushes
        if (lPush) {
          setLightWorld(w => ({ ...w, entities: w.entities.map(e =>
            e === lPush ? { ...e, x: nlx + dx, y: nly + dy } : e) }));
        }
        if (sPush) {
          setShadowWorld(w => ({ ...w, entities: w.entities.map(e =>
            e === sPush ? { ...e, x: nsx + mdx, y: nsy + mdy } : e) }));
        }
        // Move player
        setLightPos({ x: nlx, y: nly });
        setShadowPos({ x: nsx, y: nsy });
        setSteps(s => s + 1);
        // Check alignment after push
        const nlH = lPush ? { x: nlx + dx, y: nly + dy } : (lHalf || {});
        const nsH = sPush ? { x: nsx + mdx, y: nsy + mdy } : (sHalf || {});
        if (nlH.x === nsH.x && nlH.y === nsH.y) {
          setMsg("They want to be one. Step onto them.");
        } else {
          setMsg(lPush || sPush ? "" : "");
        }
        // Latch detection: only linked-plate rooms latch.
        // Non-linked rooms use live pressure only — doors close if weight is removed.
        if (linkedPlates) {
          const postLightEnts = lPush
            ? lightWorld.entities.map(e => e === lPush ? { ...e, x: nlx + dx, y: nly + dy } : e)
            : lightWorld.entities;
          const postShadowEnts = shadowWorld.entities.map(e => {
            if (e === sPush) return { ...e, x: nsx + mdx, y: nsy + mdy };
            return e;
          });
          const lightCheck = { tiles: lightWorld.tiles, entities: postLightEnts };
          const shadowCheck = { tiles: shadowWorld.tiles, entities: postShadowEnts };
          const lPressedNow = isPlatePressed(lightCheck, { x: nlx, y: nly });
          const sPressedNow = isPlatePressed(shadowCheck, { x: nsx, y: nsy });
          // Linked: BOTH plates must be pressed on the same move to latch (requires mirror mode)
          if (lPressedNow && sPressedNow) {
            setSDoorsLatched(true);
            setLDoorsLatched(true);
          }
        }

        // Mirror mode: activate on mirror tile, tick down, snap back when spent
        if (mirrorSteps > 0) {
          const rem = mirrorSteps - 1;
          setMirrorSteps(rem);
          if (rem <= 0) {
            // Snap both players back to the mirror anchor (like split mode)
            if (mirrorAnchor) {
              setLightPos(mirrorAnchor);
              setShadowPos(mirrorAnchor);
            }
            setMirrorAnchor(null);
            setMsg("The eye closes. You return to yourself.");
          } else {
            setMsg(`The eye is watching. ${rem} breaths remain.`);
          }
        } else if (lightWorld.tiles[nly]?.[nlx] === T.MIRROR || shadowWorld.tiles[nsy]?.[nsx] === T.MIRROR) {
          setMirrorSteps(MIRROR_STEPS);
          setMirrorAnchor({ x: nlx, y: nly });
          setMsg(`\u25C6 THE EYE OPENS \u2014 ${MIRROR_STEPS} INVERTED BREATHS \u25C6`);
        }
        // Ghouls advance in both worlds
        tickGhoulsAndCheck({ x: nlx, y: nly }, { x: nsx, y: nsy });
      } else {
        if (!lOk && !sOk) setMsg("Both worlds refuse you.");
        else if (!lOk) setMsg("The upper world will not yield.");
        else setMsg("The lower world will not yield.");
      }
      return;
    }

    if (key === "Escape" && splitMode) { snapSplit(); return; }
    if (key === "Escape" && mirrorSteps > 0) {
      if (mirrorAnchor) { setLightPos(mirrorAnchor); setShadowPos(mirrorAnchor); }
      setMirrorAnchor(null); setMirrorSteps(0);
      setMsg("The eye closes. You return to yourself.");
      return;
    }

    if (key === "1") {
      if (splitMode === "light") { snapSplit(); return; }
      if (splitMode) { setMsg("Finish the first walk. Then begin another."); return; }
      setSplitMode("light");
      setSplitSteps(SPLIT_STEPS);
      setSplitAnchor({ ...lightPos });
      setMsg(`Light walks alone. ${SPLIT_STEPS} breaths remain.`);
      return;
    }
    if (key === "2" || key === "j" || key === "J") {
      if (splitMode === "shadow") { snapSplit(); return; }
      if (splitMode) { setMsg("Finish the first walk. Then begin another."); return; }
      setSplitMode("shadow");
      setSplitSteps(SPLIT_STEPS);
      setSplitAnchor({ ...shadowPos });
      setMsg(`Shadow walks alone. ${SPLIT_STEPS} breaths remain.`);
      return;
    }

    if (key === "Enter") {
      if (splitMode) { setMsg("You are two. Finish first, then speak."); return; }
      const dirs = [{ dx: 0, dy: -1 }, { dx: 0, dy: 1 }, { dx: -1, dy: 0 }, { dx: 1, dy: 0 }];
      for (const d of dirs) {
        const ax = lightPos.x + d.dx, ay = lightPos.y + d.dy;
        if (ax < 0 || ax >= GRID || ay < 0 || ay >= GRID) continue;
        const ent = lightWorld.entities.find(en => en.x === ax && en.y === ay);
        if (ent) {
          if (ent.type === "npc" && ent.dialogue) { setDialogueLines(getDialogue(ent.dialogue, shards, victory, chapter, voidShards, echoShards)); setDialogueLine(0); setGs("dialogue"); return; }
          if (ent.type === "sign") {
            if (ent.lines) { setDialogueLines(ent.lines); setDialogueLine(0); setGs("dialogue"); return; }
            setMsg(ent.text || "..."); return;
          }
        }
      }
      for (const d of dirs) {
        const ax = shadowPos.x + d.dx, ay = shadowPos.y + d.dy;
        if (ax < 0 || ax >= GRID || ay < 0 || ay >= GRID) continue;
        const ent = shadowWorld.entities.find(en => en.x === ax && en.y === ay);
        if (ent) {
          if (ent.type === "npc" && ent.dialogue) { setDialogueLines(getDialogue(ent.dialogue, shards, victory, chapter, voidShards, echoShards)); setDialogueLine(0); setGs("dialogue"); return; }
          if (ent.type === "sign") {
            if (ent.lines) { setDialogueLines(ent.lines); setDialogueLine(0); setGs("dialogue"); return; }
            setMsg(ent.text || "..."); return;
          }
        }
      }
      setMsg("Nothing answers you here.");
    }
  }, [gs, lightWorld, shadowWorld, lightPos, shadowPos, splitMode, splitSteps, splitAnchor,
    dialogueLines, dialogueLine, roomX, roomY, seed, shards, shardMsg, caughtMsg, victory, chapter, voidShards, echoShards, mirrorSteps, mirrorAnchor, lDoorsLatched, sDoorsLatched, linkedPlates,
    tickGhoulsAndCheck]);

  useEffect(() => { window.addEventListener("keydown", handleKey); return () => window.removeEventListener("keydown", handleKey); }, [handleKey]);
  useEffect(() => { render(); }, [render]);

  return (
    <div tabIndex={0} autoFocus style={{ display:"flex",flexDirection:"column",alignItems:"center",justifyContent:"center",minHeight:"100vh",background:"#121212",fontFamily:"'Courier New',monospace",outline:"none" }}>
      <div style={{ background:"#222",padding:"20px",borderRadius:"16px",boxShadow:"0 16px 48px rgba(0,0,0,0.8)",border:"1px solid #333" }}>
        <canvas ref={cvs} width={W} height={H} style={{ width:W,height:H,borderRadius:"4px",imageRendering:"pixelated",border:"1px solid #444",background:WHT }} />
        <div style={{ display:"flex",justifyContent:"center",gap:"6px",marginTop:"14px",flexWrap:"wrap" }}>
          {["Split Light (1)","\u2191","\u2193","\u2190","\u2192","Split Shadow (2)","\u23CE Act"].map(l=>(
            <div key={l} style={{ background:"#333",color:"#777",fontSize:"10px",padding:"6px 10px",borderRadius:"6px",border:"1px solid #444" }}>{l}</div>
          ))}
        </div>
        <div style={{ fontSize:"9px",color:"#444",textAlign:"center",marginTop:"8px" }}>1 = Split Light &middot; 2 = Split Shadow &middot; ENTER = Interact</div>
      </div>
    </div>
  );
}
