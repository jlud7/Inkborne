#include "DiptychActivity.h"

#include <Arduino.h>
#include <HalDisplay.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "components/UITheme.h"
#include "fontIds.h"

using Button = MappedInputManager::Button;

namespace {

// ── Layout constants (480×800 panel) ──
constexpr int GRID = 15;
constexpr int TILE = 24;
constexpr int SHARDS_PER_CHAPTER = 5;
constexpr int TOTAL_CHAPTERS = 3;
constexpr int SPLIT_STEPS = 5;
constexpr int MIRROR_STEPS = 5;
constexpr int MAX_HEARTS = 5;
constexpr int WORLD_RADIUS = 4;

constexpr int VIEW = GRID * TILE;  // 360
constexpr int VIEW_X = (480 - VIEW) / 2;  // 60
constexpr int LIGHT_Y = 24;
constexpr int SHADOW_Y = 408;
constexpr int STATUS_Y = 772;
constexpr int DIVIDER_Y = 388;
constexpr int DOOR_LO = 5;
constexpr int DOOR_HI = 9;
constexpr unsigned long EXIT_HOLD_MS = 1300;

// ── Shard positions per chapter ──
struct ShardDef {
  int8_t rx;
  int8_t ry;
  int8_t lx;
  int8_t ly;
  int8_t sx;
  int8_t sy;
};

constexpr ShardDef SHARDS_CH1[SHARDS_PER_CHAPTER] = {
    {0, -1, 11, 4, 4, 10},
    {-1, 0, 11, 3, 3, 11},
    {1, 1, 11, 11, 3, 3},
    {2, -1, 12, 2, 3, 12},
    {0, 2, 12, 3, 2, 11},
};

constexpr ShardDef SHARDS_CH2[SHARDS_PER_CHAPTER] = {
    {0, -3, 11, 2, 3, 12},
    {-3, 0, 3, 3, 11, 11},
    {3, 3, 7, 11, 7, 3},   // Gatekeeper: combined-mode collection, positions set in buildChapter2Room
    {-2, -2, 11, 11, 3, 3},
    {2, 3, 10, 3, 4, 11},  // Double Gate
};

constexpr ShardDef SHARDS_CH3[SHARDS_PER_CHAPTER] = {
    {0, -2, 10, 3, 4, 11},  // Reflection: door cages + mirror
    {-2, 0, 7, 5, 7, 9},    // Divide: vertical mirror
    {2, 2, 5, 10, 9, 10},   // Crossroads
    {-2, 2, 7, 3, 7, 11},   // Paradox: two mirrors
    {2, -2, 10, 3, 4, 11},  // Final Mirror: hardest dual-plate
};

// ── Shard pickup flavor per chapter ──
constexpr const char* SHARD_PICKUP_CH1[SHARDS_PER_CHAPTER] = {
    "I. A star wakes beneath the ice.",
    "II. The dark learns a name it cannot say.",
    "III. Two roads remember one sky.",
    "IV. Something below begins to sing.",
    "V. Bring what is whole to the one who waits.",
};
constexpr const char* SHARD_PICKUP_CH2[SHARDS_PER_CHAPTER] = {
    "I. The deep answers in its own voice.",
    "II. Roots find water that does not reflect.",
    "III. The glass cracks but will not part.",
    "IV. Silence takes a shape. You recognize it.",
    "V. The last seam is near. So is something else.",
};
constexpr const char* SHARD_PICKUP_CH3[SHARDS_PER_CHAPTER] = {
    "I. A voice speaks your line before you.",
    "II. What you did, undoes. What you undid, does.",
    "III. Two paths. One step. No witness.",
    "IV. The mirror says the name you had forgotten.",
    "V. The reflection lowers its hand. You lower yours.",
};

// ── Ghoul catch flavor text (cycled by step count) ──
constexpr const char* GHOUL_LIGHT_LINES[] = {
    "The light remembers you.",
    "You were not meant to be seen this long.",
    "It does not burn. It reads you.",
    "Something in the brightness had a name for you.",
    "The light was never empty.",
    "A white silence closes.",
    "You were the shadow it needed.",
};
constexpr size_t GHOUL_LIGHT_COUNT = sizeof(GHOUL_LIGHT_LINES) / sizeof(GHOUL_LIGHT_LINES[0]);

constexpr const char* GHOUL_SHADOW_LINES[] = {
    "The dark leans closer.",
    "Something without a mouth says your name.",
    "The black was not a color. It was a waiting.",
    "You feel recognized. It is worse than being caught.",
    "The shadow makes room for another shadow.",
    "A cold patience touches you.",
    "What you feared was kind compared to this.",
};
constexpr size_t GHOUL_SHADOW_COUNT = sizeof(GHOUL_SHADOW_LINES) / sizeof(GHOUL_SHADOW_LINES[0]);

// ── Sign IDs (local to this translation unit) ──
enum SignId : uint8_t {
  SIGN_NONE = 0,
  SIGN_SPLIT_LIGHT = 1,
  SIGN_SPLIT_SHADOW = 2,
  // Graveyard light headstones 10..15
  SIGN_GRAVE_L1 = 10,
  SIGN_GRAVE_L2 = 11,
  SIGN_GRAVE_L3 = 12,
  SIGN_GRAVE_L4 = 13,
  SIGN_GRAVE_L5 = 14,
  SIGN_GRAVE_L6 = 15,
  // Graveyard shadow headstones 20..25
  SIGN_GRAVE_S1 = 20,
  SIGN_GRAVE_S2 = 21,
  SIGN_GRAVE_S3 = 22,
  SIGN_GRAVE_S4 = 23,
  SIGN_GRAVE_S5 = 24,
  SIGN_GRAVE_S6 = 25,
  // First Diptych 30..34
  SIGN_FD_L1 = 30,
  SIGN_FD_L2 = 31,
  SIGN_FD_PLAQUE = 32,
  SIGN_FD_S1 = 33,
  SIGN_FD_S2 = 34,
  SIGN_EMPTY_WATCHER = 40,
  SIGN_BELL_L = 50,
  SIGN_BELL_S = 51,
  SIGN_SHRINE_L = 60,
  SIGN_SHRINE_S = 61,
  SIGN_WELLS_L = 70,
  SIGN_WELLS_S = 71,
  SIGN_POOL_L = 80,
  SIGN_POOL_S = 81,
  SIGN_REFL_L = 90,
  SIGN_REFL_S = 91,
};

// NPC IDs
constexpr uint8_t NPC_WATCHER = 0;
constexpr uint8_t NPC_SAGE = 1;

// ── Simple deterministic PRNG for procedural rooms ──
inline uint32_t xorshift32(uint32_t& state) {
  uint32_t x = state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  state = x ? x : 0xdeadbeefu;
  return x;
}

inline float randFloat(uint32_t& state) {
  return static_cast<float>(xorshift32(state)) / 4294967296.0f;
}

inline uint32_t seedFor(int rx, int ry, uint32_t salt) {
  uint32_t h = 2166136261u;
  h ^= static_cast<uint32_t>(rx + 1000);
  h *= 16777619u;
  h ^= static_cast<uint32_t>(ry + 1000);
  h *= 16777619u;
  h ^= salt;
  h *= 16777619u;
  if (h == 0) h = 1;
  return h;
}

inline const char* directionWord(int rx, int ry) {
  // Returns a short compass word for a shard position.
  const bool n = ry < 0, s = ry > 0;
  const bool w = rx < 0, e = rx > 0;
  if (n && w) return "northwest";
  if (n && e) return "northeast";
  if (s && w) return "southwest";
  if (s && e) return "southeast";
  if (n) return "north";
  if (s) return "south";
  if (w) return "west";
  if (e) return "east";
  return "near";
}

}  // namespace

// ── Activity lifecycle ──
void DiptychActivity::onEnter() {
  Activity::onEnter();
  resetGame();
  requestUpdate();
}

bool DiptychActivity::preventAutoSleep() { return screen_ == Screen::Play; }

// ── State management ──
void DiptychActivity::resetGame() {
  screen_ = Screen::Intro;
  chapter_ = 1;
  roomX_ = 0;
  roomY_ = 0;
  lightX_ = shadowX_ = 7;
  lightY_ = shadowY_ = 7;
  splitMode_ = SplitMode::None;
  splitSteps_ = 0;
  splitAnchorX_ = splitAnchorY_ = 7;
  mirrorSteps_ = 0;
  mirrorAnchorX_ = mirrorAnchorY_ = 7;
  shardsMask_ = 0;
  voidShardsMask_ = 0;
  echoShardsMask_ = 0;
  hp_ = MAX_HEARTS;
  ambushed_ = false;
  lDoorsLatched_ = false;
  sDoorsLatched_ = false;
  linkedPlates_ = false;
  caughtKind_ = CaughtKind::None;
  pendingVictory_ = false;
  victory_ = false;
  steps_ = 0;
  backConsumed_ = false;
  needsFullRefresh_ = true;
  message_.clear();
  pickupMessage_.clear();
  dialogue_.clear();
  dialogueLine_ = 0;
  for (int y = 0; y < WORLD_SIZE; ++y) {
    for (int x = 0; x < WORLD_SIZE; ++x) {
      visited_[y][x] = 0;
    }
  }
  markVisited(0, 0);
  buildRoom(0, 0, light_, shadow_);
}

void DiptychActivity::resetChapter() {
  if (chapter_ >= 3) {
    echoShardsMask_ = 0;
    message_ = "The reflections forget you. Begin again.";
  } else if (chapter_ >= 2) {
    voidShardsMask_ = 0;
    message_ = "The deep takes back what it lent. Begin again.";
  } else {
    shardsMask_ = 0;
    message_ = "The pieces scatter into the dark. Begin again.";
  }
  hp_ = MAX_HEARTS;
}

void DiptychActivity::respawnAtWatcher() {
  roomX_ = 0;
  roomY_ = 0;
  lightX_ = shadowX_ = 7;
  lightY_ = shadowY_ = 7;
  splitMode_ = SplitMode::None;
  splitSteps_ = 0;
  mirrorSteps_ = 0;
  lDoorsLatched_ = false;
  sDoorsLatched_ = false;
  linkedPlates_ = false;
  buildRoom(0, 0, light_, shadow_);
  needsFullRefresh_ = true;
}

void DiptychActivity::markVisited(int rx, int ry) {
  const int x = rx + WORLD_RADIUS;
  const int y = ry + WORLD_RADIUS;
  if (x < 0 || y < 0 || x >= WORLD_SIZE || y >= WORLD_SIZE) return;
  visited_[y][x] = 1;
}

bool DiptychActivity::wasVisited(int rx, int ry) const {
  const int x = rx + WORLD_RADIUS;
  const int y = ry + WORLD_RADIUS;
  if (x < 0 || y < 0 || x >= WORLD_SIZE || y >= WORLD_SIZE) return false;
  return visited_[y][x] != 0;
}

bool DiptychActivity::anyShardRemainingAt(int rx, int ry) const {
  for (uint8_t i = 0; i < SHARDS_PER_CHAPTER; ++i) {
    if (SHARDS_CH1[i].rx == rx && SHARDS_CH1[i].ry == ry && !isShardCollected(1, i)) return true;
  }
  if (chapter_ >= 2) {
    for (uint8_t i = 0; i < SHARDS_PER_CHAPTER; ++i) {
      if (SHARDS_CH2[i].rx == rx && SHARDS_CH2[i].ry == ry && !isShardCollected(2, i)) return true;
    }
  }
  if (chapter_ >= 3) {
    for (uint8_t i = 0; i < SHARDS_PER_CHAPTER; ++i) {
      if (SHARDS_CH3[i].rx == rx && SHARDS_CH3[i].ry == ry && !isShardCollected(3, i)) return true;
    }
  }
  return false;
}

// ── Room building ──
void DiptychActivity::clearWorld(World& world) const {
  for (int y = 0; y < GRID; ++y) {
    for (int x = 0; x < GRID; ++x) {
      world.tiles[y][x] = Tile::Empty;
    }
  }
  world.count = 0;
  for (auto& entity : world.entities) {
    entity = Entity{};
  }
}

void DiptychActivity::addEntity(World& world, EntityType type, int x, int y, uint8_t id, bool blocking,
                                SignSprite sprite) const {
  if (world.count >= world.entities.size()) return;
  Entity& e = world.entities[world.count++];
  e.type = type;
  e.x = static_cast<int8_t>(x);
  e.y = static_cast<int8_t>(y);
  e.id = id;
  e.blocking = blocking;
  e.signSprite = sprite;
}

void DiptychActivity::addPerimeter(World& world) const {
  for (int i = 0; i < GRID; ++i) {
    if (i >= DOOR_LO && i <= DOOR_HI) continue;
    world.tiles[0][i] = Tile::Wall;
    world.tiles[GRID - 1][i] = Tile::Wall;
    world.tiles[i][0] = Tile::Wall;
    world.tiles[i][GRID - 1] = Tile::Wall;
  }
}

void DiptychActivity::addWalls(World& world, std::initializer_list<std::array<int, 2>> points) const {
  for (const auto& p : points) {
    const int x = p[0];
    const int y = p[1];
    if (inside(x, y)) world.tiles[y][x] = Tile::Wall;
  }
}

void DiptychActivity::addDoors(World& world, std::initializer_list<std::array<int, 2>> points) const {
  for (const auto& p : points) {
    const int x = p[0];
    const int y = p[1];
    if (inside(x, y)) world.tiles[y][x] = Tile::Door;
  }
}

void DiptychActivity::addWater(World& world, std::initializer_list<std::array<int, 2>> points) const {
  for (const auto& p : points) {
    const int x = p[0];
    const int y = p[1];
    if (inside(x, y)) world.tiles[y][x] = Tile::Water;
  }
}

void DiptychActivity::sealBoundaryEdges(int rx, int ry, World& light, World& shadow) const {
  auto seal = [&](World& w) {
    if (rx == -WORLD_RADIUS) {
      for (int y = 0; y < GRID; ++y) w.tiles[y][0] = Tile::Wall;
    }
    if (rx == WORLD_RADIUS) {
      for (int y = 0; y < GRID; ++y) w.tiles[y][GRID - 1] = Tile::Wall;
    }
    if (ry == -WORLD_RADIUS) {
      for (int x = 0; x < GRID; ++x) w.tiles[0][x] = Tile::Wall;
    }
    if (ry == WORLD_RADIUS) {
      for (int x = 0; x < GRID; ++x) w.tiles[GRID - 1][x] = Tile::Wall;
    }
  };
  seal(light);
  seal(shadow);
}

bool DiptychActivity::buildTutorialRoom(int rx, int ry, World& light, World& shadow) const {
  // Watcher room
  if (rx == 0 && ry == 0) {
    light.tiles[2][2] = Tile::Tree;
    light.tiles[2][12] = Tile::Tree;
    light.tiles[12][2] = Tile::Tree;
    light.tiles[12][12] = Tile::Tree;
    shadow.tiles[3][3] = Tile::Tree;
    shadow.tiles[3][11] = Tile::Tree;
    shadow.tiles[11][3] = Tile::Tree;
    shadow.tiles[11][11] = Tile::Tree;
    addEntity(light, EntityType::Npc, 5, 7, NPC_WATCHER);
    return true;
  }
  // Split Light tutorial
  if (rx == 1 && ry == 0) {
    for (int y = 0; y < GRID; ++y) light.tiles[y][9] = Tile::Wall;
    addEntity(light, EntityType::Sign, 3, 7, SIGN_SPLIT_LIGHT, true, SignSprite::Sign);
    return true;
  }
  // Split Shadow tutorial
  if (rx == 2 && ry == 0) {
    for (int y = 0; y < GRID; ++y) shadow.tiles[y][9] = Tile::Wall;
    addEntity(light, EntityType::Sign, 3, 7, SIGN_SPLIT_SHADOW, true, SignSprite::Sign);
    return true;
  }
  // Wall room
  if (rx == 3 && ry == 0) {
    for (int y = 0; y < GRID; ++y) light.tiles[y][10] = Tile::Wall;
    for (int y = 0; y < GRID; ++y) shadow.tiles[y][5] = Tile::Wall;
    return true;
  }
  // Sage room
  if (rx == 4 && ry == 0) {
    light.tiles[2][2] = Tile::Tree;
    light.tiles[2][6] = Tile::Tree;
    light.tiles[2][10] = Tile::Tree;
    shadow.tiles[3][3] = Tile::Tree;
    shadow.tiles[3][7] = Tile::Tree;
    shadow.tiles[3][11] = Tile::Tree;
    addEntity(light, EntityType::Npc, 7, 6, NPC_SAGE);
    return true;
  }
  // The Ambush (1,-1): unwinnable ghoul encounter
  if (rx == 1 && ry == -1) {
    addWalls(light, {{3, 3}, {4, 3}, {10, 3}, {11, 3}, {3, 11}, {4, 11}, {10, 11}, {11, 11}});
    addWalls(shadow, {{3, 3}, {4, 3}, {10, 3}, {11, 3}, {3, 11}, {4, 11}, {10, 11}, {11, 11}});
    addEntity(light, EntityType::Ghoul, 1, 1);
    addEntity(light, EntityType::Ghoul, 13, 1);
    addEntity(light, EntityType::Ghoul, 1, 13);
    addEntity(light, EntityType::Ghoul, 13, 13);
    addEntity(shadow, EntityType::Ghoul, 7, 1);
    addEntity(shadow, EntityType::Ghoul, 7, 13);
    return true;
  }
  // Ghoul patrol (-1,-1)
  if (rx == -1 && ry == -1) {
    addWalls(light, {{5, 4}, {6, 4}, {8, 4}, {9, 4}, {5, 10}, {6, 10}, {8, 10}, {9, 10}});
    addWalls(shadow, {{4, 5}, {4, 6}, {4, 8}, {4, 9}, {10, 5}, {10, 6}, {10, 8}, {10, 9}});
    addEntity(light, EntityType::Ghoul, 3, 7);
    addEntity(shadow, EntityType::Ghoul, 11, 7);
    return true;
  }
  // Ghoul patrol (1,2)
  if (rx == 1 && ry == 2) {
    addWalls(light, {{4, 4}, {5, 4}, {9, 4}, {10, 4}, {7, 7}});
    addWalls(shadow, {{4, 10}, {5, 10}, {9, 10}, {10, 10}, {7, 7}});
    addEntity(light, EntityType::Ghoul, 2, 2);
    addEntity(light, EntityType::Ghoul, 12, 12);
    addEntity(shadow, EntityType::Ghoul, 7, 2);
    return true;
  }

  // Chapter 1 shard puzzles
  if (rx == 0 && ry == -1) {
    addWalls(light, {{10, 4}, {10, 5}, {10, 6}, {11, 8}, {6, 7}});
    addWalls(shadow, {{5, 10}, {5, 9}, {3, 10}, {3, 9}, {4, 6}, {8, 7}});
    if (!isShardCollected(1, 0)) {
      addEntity(light, EntityType::HalfLight, 11, 4, 0);
      addEntity(shadow, EntityType::HalfShadow, 4, 10, 0);
    }
    return true;
  }
  if (rx == -1 && ry == 0) {
    addWalls(light, {{6, 3}, {7, 8}, {3, 12}, {2, 11}});
    addWalls(shadow, {{8, 11}, {7, 6}, {11, 2}, {12, 3}});
    if (!isShardCollected(1, 1)) {
      addEntity(light, EntityType::HalfLight, 11, 3, 1);
      addEntity(shadow, EntityType::HalfShadow, 3, 11, 1);
    }
    return true;
  }
  if (rx == 1 && ry == 1) {
    addWalls(light, {{11, 6}, {6, 7}, {3, 2}, {2, 3}});
    addWalls(shadow, {{3, 8}, {8, 7}, {11, 12}, {12, 11}});
    if (!isShardCollected(1, 2)) {
      addEntity(light, EntityType::HalfLight, 11, 11, 2);
      addEntity(shadow, EntityType::HalfShadow, 3, 3, 2);
    }
    return true;
  }
  if (rx == 2 && ry == -1) {
    addWalls(light, {{12, 4}, {6, 3}, {7, 6}, {10, 2}});
    addWalls(shadow, {{3, 8}, {8, 9}, {7, 4}, {11, 1}, {11, 2}, {4, 12}, {4, 11}});
    if (!isShardCollected(1, 3)) {
      addEntity(light, EntityType::HalfLight, 12, 2, 3);
      addEntity(shadow, EntityType::HalfShadow, 3, 12, 3);
    }
    return true;
  }
  if (rx == 0 && ry == 2) {
    addWalls(light, {{9, 3}, {10, 8}, {6, 7}, {12, 4}, {3, 10}, {3, 9}});
    addWalls(shadow, {{5, 11}, {4, 6}, {8, 7}, {2, 10}, {11, 4}, {11, 3}});
    if (!isShardCollected(1, 4)) {
      addEntity(light, EntityType::HalfLight, 12, 3, 4);
      addEntity(shadow, EntityType::HalfShadow, 2, 11, 4);
    }
    return true;
  }

  return false;
}

bool DiptychActivity::buildChapter2Room(int rx, int ry, World& light, World& shadow) const {
  // Void 0 (0,-3): The Fortress
  if (rx == 0 && ry == -3) {
    addWalls(light, {{11, 6}, {6, 5}, {7, 8}, {10, 1}});
    addWalls(shadow, {{3, 8}, {8, 9}, {7, 6}, {4, 13}});
    if (!isShardCollected(2, 0)) {
      addEntity(light, EntityType::HalfLight, 11, 2, 0);
      addEntity(shadow, EntityType::HalfShadow, 3, 12, 0);
    }
    return true;
  }
  // Void 1 (-3,0): The Maze
  if (rx == -3 && ry == 0) {
    addWalls(light, {{3, 6}, {8, 3}, {7, 8}, {4, 1}});
    addWalls(shadow, {{11, 8}, {6, 11}, {7, 6}, {10, 13}});
    if (!isShardCollected(2, 1)) {
      addEntity(light, EntityType::HalfLight, 3, 3, 1);
      addEntity(shadow, EntityType::HalfShadow, 11, 11, 1);
    }
    return true;
  }
  // Void 2 (3,3): The Gatekeeper — pressure plate opens shadow door cage
  if (rx == 3 && ry == 3) {
    light.tiles[7][7] = Tile::Switch;
    addWalls(light, {{7, 6}, {6, 7}, {8, 3}});
    addDoors(shadow, {{6, 2}, {7, 2}, {8, 2}, {6, 3}, {8, 3}, {6, 4}, {7, 4}, {8, 4}});
    addWalls(shadow, {{8, 7}, {7, 10}});
    if (!isShardCollected(2, 2)) {
      addEntity(light, EntityType::HalfLight, 7, 11, 2);
      addEntity(shadow, EntityType::HalfShadow, 7, 3, 2);
    }
    return true;
  }
  // Void 3 (-2,-2): The Echo
  if (rx == -2 && ry == -2) {
    addWalls(light, {{11, 8}, {8, 11}, {6, 7}, {10, 10}});
    addWalls(shadow, {{3, 6}, {6, 3}, {8, 7}, {4, 4}});
    if (!isShardCollected(2, 3)) {
      addEntity(light, EntityType::HalfLight, 11, 11, 3);
      addEntity(shadow, EntityType::HalfShadow, 3, 3, 3);
    }
    return true;
  }
  // Void 4 (2,3): The Double Gate
  if (rx == 2 && ry == 3) {
    light.tiles[7][4] = Tile::Switch;
    addDoors(light, {{9, 2}, {10, 2}, {11, 2}, {9, 3}, {11, 3}, {9, 4}, {10, 4}, {11, 4}});
    addWalls(light, {{6, 7}, {7, 10}});
    shadow.tiles[7][10] = Tile::Switch;
    addDoors(shadow, {{3, 10}, {4, 10}, {5, 10}, {3, 11}, {5, 11}, {3, 12}, {4, 12}, {5, 12}});
    addWalls(shadow, {{8, 7}, {7, 4}});
    if (!isShardCollected(2, 4)) {
      addEntity(light, EntityType::HalfLight, 10, 3, 4);
      addEntity(shadow, EntityType::HalfShadow, 4, 11, 4);
    }
    return true;
  }
  return false;
}

bool DiptychActivity::buildChapter3Room(int rx, int ry, World& light, World& shadow, bool& linked) const {
  // Echo 0 (0,-2): The Reflection — linked-plate room
  if (rx == 0 && ry == -2) {
    light.tiles[7][3] = Tile::Switch;
    light.tiles[7][7] = Tile::Mirror;
    addDoors(light, {{9, 2}, {10, 2}, {11, 2}, {9, 3}, {11, 3}, {9, 4}, {10, 4}, {11, 4}});
    addWalls(light, {{11, 6}, {11, 8}});
    shadow.tiles[7][11] = Tile::Switch;
    shadow.tiles[7][7] = Tile::Mirror;
    addDoors(shadow, {{3, 10}, {4, 10}, {5, 10}, {3, 11}, {5, 11}, {3, 12}, {4, 12}, {5, 12}});
    addWalls(shadow, {{3, 6}, {3, 8}});
    linked = true;
    if (!isShardCollected(3, 0)) {
      addEntity(light, EntityType::HalfLight, 10, 3, 0);
      addEntity(shadow, EntityType::HalfShadow, 4, 11, 0);
    }
    return true;
  }
  // Echo 1 (-2,0): The Divide
  if (rx == -2 && ry == 0) {
    light.tiles[7][7] = Tile::Mirror;
    addWalls(light, {{7, 3}, {6, 7}, {8, 7}});
    shadow.tiles[7][7] = Tile::Mirror;
    addWalls(shadow, {{7, 11}, {6, 7}, {8, 7}});
    if (!isShardCollected(3, 1)) {
      addEntity(light, EntityType::HalfLight, 7, 5, 1);
      addEntity(shadow, EntityType::HalfShadow, 7, 9, 1);
    }
    return true;
  }
  // Echo 2 (2,2): The Crossroads
  if (rx == 2 && ry == 2) {
    light.tiles[10][7] = Tile::Mirror;
    addWalls(light, {{3, 7}, {7, 3}, {6, 10}, {8, 10}});
    shadow.tiles[10][7] = Tile::Mirror;
    addWalls(shadow, {{11, 7}, {7, 11}, {6, 10}, {8, 10}});
    if (!isShardCollected(3, 2)) {
      addEntity(light, EntityType::HalfLight, 5, 10, 2);
      addEntity(shadow, EntityType::HalfShadow, 9, 10, 2);
    }
    return true;
  }
  // Echo 3 (-2,2): The Paradox — two mirrors
  if (rx == -2 && ry == 2) {
    light.tiles[4][7] = Tile::Mirror;
    light.tiles[10][7] = Tile::Mirror;
    addWalls(light, {{3, 4}, {11, 4}, {7, 2}, {5, 10}, {9, 10}});
    shadow.tiles[4][7] = Tile::Mirror;
    shadow.tiles[10][7] = Tile::Mirror;
    addWalls(shadow, {{3, 10}, {11, 10}, {7, 12}, {5, 4}, {9, 4}});
    if (!isShardCollected(3, 3)) {
      addEntity(light, EntityType::HalfLight, 7, 3, 3);
      addEntity(shadow, EntityType::HalfShadow, 7, 11, 3);
    }
    return true;
  }
  // Echo 4 (2,-2): The Final Mirror — linked-plate room
  if (rx == 2 && ry == -2) {
    light.tiles[7][4] = Tile::Switch;
    light.tiles[7][7] = Tile::Mirror;
    addDoors(light, {{9, 2}, {10, 2}, {11, 2}, {11, 3}, {11, 4}, {9, 4}, {10, 4}});
    addWalls(light, {{10, 6}, {10, 8}, {9, 7}});
    shadow.tiles[7][10] = Tile::Switch;
    shadow.tiles[7][7] = Tile::Mirror;
    addDoors(shadow, {{3, 10}, {4, 10}, {5, 10}, {3, 11}, {3, 12}, {5, 12}, {5, 11}});
    addWalls(shadow, {{4, 6}, {4, 8}, {5, 7}});
    linked = true;
    if (!isShardCollected(3, 4)) {
      addEntity(light, EntityType::HalfLight, 10, 3, 4);
      addEntity(shadow, EntityType::HalfShadow, 4, 11, 4);
    }
    return true;
  }
  return false;
}

bool DiptychActivity::buildEasterEggRoom(int rx, int ry, World& light, World& shadow) const {
  // (4, -4) The Graveyard
  if (rx == 4 && ry == -4) {
    addEntity(light, EntityType::Sign, 3, 4, SIGN_GRAVE_L1, true, SignSprite::Headstone);
    addEntity(light, EntityType::Sign, 7, 4, SIGN_GRAVE_L2, true, SignSprite::Headstone);
    addEntity(light, EntityType::Sign, 11, 4, SIGN_GRAVE_L3, true, SignSprite::Headstone);
    addEntity(light, EntityType::Sign, 3, 10, SIGN_GRAVE_L4, true, SignSprite::Headstone);
    addEntity(light, EntityType::Sign, 7, 10, SIGN_GRAVE_L5, true, SignSprite::Headstone);
    addEntity(light, EntityType::Sign, 11, 10, SIGN_GRAVE_L6, true, SignSprite::Headstone);
    addEntity(shadow, EntityType::Sign, 3, 4, SIGN_GRAVE_S1, true, SignSprite::Headstone);
    addEntity(shadow, EntityType::Sign, 7, 4, SIGN_GRAVE_S2, true, SignSprite::Headstone);
    addEntity(shadow, EntityType::Sign, 11, 4, SIGN_GRAVE_S3, true, SignSprite::Headstone);
    addEntity(shadow, EntityType::Sign, 3, 10, SIGN_GRAVE_S4, true, SignSprite::Headstone);
    addEntity(shadow, EntityType::Sign, 7, 10, SIGN_GRAVE_S5, true, SignSprite::Headstone);
    addEntity(shadow, EntityType::Sign, 11, 10, SIGN_GRAVE_S6, true, SignSprite::Headstone);
    return true;
  }
  // (-4, 4) The First Diptych
  if (rx == -4 && ry == 4) {
    addEntity(light, EntityType::Sign, 6, 7, SIGN_FD_L1, true, SignSprite::PlayerLight);
    addEntity(light, EntityType::Sign, 8, 7, SIGN_FD_L2, true, SignSprite::PlayerLight);
    addEntity(light, EntityType::Sign, 7, 10, SIGN_FD_PLAQUE, true, SignSprite::Sign);
    addEntity(shadow, EntityType::Sign, 6, 7, SIGN_FD_S1, true, SignSprite::PlayerShadow);
    addEntity(shadow, EntityType::Sign, 8, 7, SIGN_FD_S2, true, SignSprite::PlayerShadow);
    return true;
  }
  // (-4, 0) The Empty Watcher
  if (rx == -4 && ry == 0) {
    light.tiles[2][2] = Tile::Tree;
    light.tiles[2][12] = Tile::Tree;
    light.tiles[12][2] = Tile::Tree;
    light.tiles[12][12] = Tile::Tree;
    shadow.tiles[3][3] = Tile::Tree;
    shadow.tiles[3][11] = Tile::Tree;
    shadow.tiles[11][3] = Tile::Tree;
    shadow.tiles[11][11] = Tile::Tree;
    addEntity(light, EntityType::Sign, 5, 7, SIGN_EMPTY_WATCHER, true, SignSprite::Sign);
    return true;
  }
  // (0, 4) The Bell
  if (rx == 0 && ry == 4) {
    addEntity(light, EntityType::Sign, 7, 7, SIGN_BELL_L, true, SignSprite::Bell);
    addEntity(shadow, EntityType::Sign, 7, 7, SIGN_BELL_S, true, SignSprite::Bell);
    return true;
  }
  // (0, -4) The Sunken Shrine
  if (rx == 0 && ry == -4) {
    addWater(light, {{3, 3}, {4, 3}, {5, 3}, {3, 4}, {3, 5},
                     {9, 3}, {10, 3}, {11, 3}, {11, 4}, {11, 5},
                     {3, 9}, {3, 10}, {3, 11}, {4, 11}, {5, 11},
                     {9, 11}, {10, 11}, {11, 11}, {11, 10}, {11, 9}});
    addWater(shadow, {{3, 3}, {4, 3}, {5, 3}, {3, 4}, {3, 5},
                      {9, 3}, {10, 3}, {11, 3}, {11, 4}, {11, 5},
                      {3, 9}, {3, 10}, {3, 11}, {4, 11}, {5, 11},
                      {9, 11}, {10, 11}, {11, 11}, {11, 10}, {11, 9}});
    addEntity(light, EntityType::Sign, 7, 7, SIGN_SHRINE_L, true, SignSprite::Headstone);
    addEntity(shadow, EntityType::Sign, 7, 7, SIGN_SHRINE_S, true, SignSprite::Headstone);
    return true;
  }
  // (3, 4) The Two Wells
  if (rx == 3 && ry == 4) {
    addWater(light, {{3, 5}, {4, 5}, {3, 6}, {4, 6}, {3, 7}, {4, 7}, {3, 8}, {4, 8}, {3, 9}, {4, 9},
                     {10, 5}, {11, 5}, {10, 6}, {11, 6}, {10, 7}, {11, 7}, {10, 8}, {11, 8}, {10, 9}, {11, 9}});
    addWater(shadow, {{3, 5}, {4, 5}, {3, 6}, {4, 6}, {3, 7}, {4, 7}, {3, 8}, {4, 8}, {3, 9}, {4, 9},
                      {10, 5}, {11, 5}, {10, 6}, {11, 6}, {10, 7}, {11, 7}, {10, 8}, {11, 8}, {10, 9}, {11, 9}});
    addEntity(light, EntityType::Sign, 7, 7, SIGN_WELLS_L, true, SignSprite::Sign);
    addEntity(shadow, EntityType::Sign, 7, 7, SIGN_WELLS_S, true, SignSprite::Sign);
    return true;
  }
  // (3, -3) The Still Pool
  if (rx == 3 && ry == -3) {
    addEntity(light, EntityType::Sign, 7, 7, SIGN_POOL_L, true, SignSprite::MirrorEye);
    addEntity(shadow, EntityType::Sign, 7, 7, SIGN_POOL_S, true, SignSprite::MirrorEye);
    return true;
  }
  // (-3, -3) The Reflected Grave
  if (rx == -3 && ry == -3) {
    addEntity(light, EntityType::Sign, 7, 7, SIGN_REFL_L, true, SignSprite::Headstone);
    addEntity(shadow, EntityType::Sign, 7, 8, SIGN_REFL_S, true, SignSprite::PlayerShadow);
    return true;
  }
  return false;
}

void DiptychActivity::buildProceduralRoom(int rx, int ry, World& light, World& shadow) const {
  uint32_t ls = seedFor(rx, ry, 0x13371337u);
  uint32_t ss = seedFor(rx, ry, 0xbeefcafeu);

  addPerimeter(light);
  addPerimeter(shadow);

  const int dist = std::abs(rx) + std::abs(ry);
  const float wc = std::min(0.18f, 0.06f + static_cast<float>(dist) * 0.015f);
  for (int y = 1; y < GRID - 1; ++y) {
    for (int x = 1; x < GRID - 1; ++x) {
      // Central safe zone (5x5 around center)
      if (std::abs(x - 7) <= 2 && std::abs(y - 7) <= 2) continue;
      // Keep doorway corridors clear (3 tiles deep)
      const bool inDoor =
          (x >= DOOR_LO && x <= DOOR_HI && (y <= 2 || y >= GRID - 3)) ||
          (y >= DOOR_LO && y <= DOOR_HI && (x <= 2 || x >= GRID - 3));
      if (inDoor) continue;
      if (randFloat(ls) < wc) {
        light.tiles[y][x] = randFloat(ls) < 0.3f ? Tile::Tree : Tile::Wall;
      }
      if (randFloat(ss) < wc) {
        shadow.tiles[y][x] = randFloat(ss) < 0.3f ? Tile::Tree : Tile::Wall;
      }
    }
  }

  // Water patch in distant rooms
  if (dist > 4) {
    auto patch = [&](World& w, uint32_t& st) {
      const int cx = 4 + static_cast<int>(randFloat(st) * (GRID - 8));
      const int cy = 4 + static_cast<int>(randFloat(st) * (GRID - 8));
      for (int dy2 = -1; dy2 <= 1; ++dy2) {
        for (int dx2 = -1; dx2 <= 1; ++dx2) {
          if (randFloat(st) < 0.5f) {
            const int wx = cx + dx2;
            const int wy = cy + dy2;
            if (inside(wx, wy) && w.tiles[wy][wx] == Tile::Empty) {
              w.tiles[wy][wx] = Tile::Water;
            }
          }
        }
      }
    };
    if (randFloat(ls) < 0.25f) patch(light, ls);
    if (randFloat(ss) < 0.25f) patch(shadow, ss);
  }

  // Connectivity guarantee: clear center row + column in both worlds
  for (int x = 1; x < GRID - 1; ++x) {
    if (light.tiles[7][x] != Tile::Empty) light.tiles[7][x] = Tile::Empty;
    if (shadow.tiles[7][x] != Tile::Empty) shadow.tiles[7][x] = Tile::Empty;
  }
  for (int y = 1; y < GRID - 1; ++y) {
    if (light.tiles[y][7] != Tile::Empty) light.tiles[y][7] = Tile::Empty;
    if (shadow.tiles[y][7] != Tile::Empty) shadow.tiles[y][7] = Tile::Empty;
  }
}

void DiptychActivity::buildRoom(int rx, int ry, World& light, World& shadow) const {
  clearWorld(light);
  clearWorld(shadow);

  bool linked = false;
  bool handled = buildTutorialRoom(rx, ry, light, shadow);
  if (!handled && chapter_ >= 2) {
    handled = buildChapter2Room(rx, ry, light, shadow);
  }
  if (!handled && chapter_ >= 3) {
    handled = buildChapter3Room(rx, ry, light, shadow, linked);
  }
  if (!handled) {
    handled = buildEasterEggRoom(rx, ry, light, shadow);
  }
  if (!handled) {
    buildProceduralRoom(rx, ry, light, shadow);
  }

  sealBoundaryEdges(rx, ry, light, shadow);

  // Remember whether this room uses linked plates; since buildRoom is const we
  // store via a const_cast trick through a mutable member. Instead, surface
  // the flag through loadRoom which sets linkedPlates_ after calling buildRoom.
  // The linked flag from build is captured via a side channel: re-check.
  const_cast<DiptychActivity*>(this)->linkedPlates_ = linked;
}

void DiptychActivity::loadRoom(int rx, int ry, int entryX, int entryY) {
  roomX_ = rx;
  roomY_ = ry;
  lightX_ = shadowX_ = entryX;
  lightY_ = shadowY_ = entryY;
  lDoorsLatched_ = false;
  sDoorsLatched_ = false;
  buildRoom(rx, ry, light_, shadow_);
  markVisited(rx, ry);
  needsFullRefresh_ = true;
}

// ── Shard helpers ──
bool DiptychActivity::isShardCollected(uint8_t chapter, uint8_t index) const {
  const uint8_t bit = 1u << index;
  switch (chapter) {
    case 1: return (shardsMask_ & bit) != 0;
    case 2: return (voidShardsMask_ & bit) != 0;
    case 3: return (echoShardsMask_ & bit) != 0;
  }
  return false;
}

int DiptychActivity::shardCountForChapter(uint8_t chapter) const {
  uint8_t mask = 0;
  switch (chapter) {
    case 1: mask = shardsMask_; break;
    case 2: mask = voidShardsMask_; break;
    case 3: mask = echoShardsMask_; break;
    default: return 0;
  }
  int n = 0;
  for (int i = 0; i < SHARDS_PER_CHAPTER; ++i) {
    if (mask & (1u << i)) ++n;
  }
  return n;
}

int DiptychActivity::totalShardsThisChapter() const { return SHARDS_PER_CHAPTER; }

const char* DiptychActivity::shardPickupLine(uint8_t chapter, int countAfterPickup) const {
  const int idx = std::max(0, std::min(SHARDS_PER_CHAPTER - 1, countAfterPickup - 1));
  switch (chapter) {
    case 1: return SHARD_PICKUP_CH1[idx];
    case 2: return SHARD_PICKUP_CH2[idx];
    case 3: return SHARD_PICKUP_CH3[idx];
  }
  return "";
}

// ── Tile rules ──
bool DiptychActivity::isWalkable(Tile tile, bool doorsOpen) const {
  return tile == Tile::Empty || tile == Tile::Switch || tile == Tile::Mirror ||
         (tile == Tile::Door && doorsOpen);
}

bool DiptychActivity::inside(int x, int y) const { return x >= 0 && y >= 0 && x < GRID && y < GRID; }

int DiptychActivity::snapDoor(int v) const { return std::max(DOOR_LO, std::min(DOOR_HI, v)); }

int DiptychActivity::findEntityAt(const World& world, int x, int y, EntityType type) const {
  for (int i = 0; i < world.count; ++i) {
    const Entity& e = world.entities[i];
    if (e.x == x && e.y == y && (type == EntityType::None || e.type == type)) return i;
  }
  return -1;
}

int DiptychActivity::findHalfAt(const World& world, int x, int y, EntityType halfType) const {
  for (int i = 0; i < world.count; ++i) {
    const Entity& e = world.entities[i];
    if (e.type == halfType && e.x == x && e.y == y) return i;
  }
  return -1;
}

bool DiptychActivity::hasBlockingEntity(const World& world, int x, int y) const {
  for (int i = 0; i < world.count; ++i) {
    const Entity& e = world.entities[i];
    if (e.x != x || e.y != y) continue;
    if (e.type == EntityType::Npc) return true;
    if (e.type == EntityType::Sign && e.blocking) return true;
  }
  return false;
}

void DiptychActivity::removeEntity(World& world, int index) {
  if (index < 0 || index >= world.count) return;
  for (int i = index; i + 1 < world.count; ++i) {
    world.entities[i] = world.entities[i + 1];
  }
  world.entities[--world.count] = Entity{};
}

void DiptychActivity::filterCollectedShardsFromWorld(World& world) const {
  // Remove half entities whose shard is already collected (current chapter).
  for (int i = world.count - 1; i >= 0; --i) {
    const Entity& e = world.entities[i];
    if (e.type != EntityType::HalfLight && e.type != EntityType::HalfShadow) continue;
    if (isShardCollected(chapter_, e.id)) {
      const_cast<DiptychActivity*>(this)->removeEntity(const_cast<World&>(world), i);
    }
  }
}

// ── Pressure plate ──
bool DiptychActivity::isPlatePressed(const World& world, int playerX, int playerY) const {
  if (inside(playerX, playerY) && world.tiles[playerY][playerX] == Tile::Switch) return true;
  for (int i = 0; i < world.count; ++i) {
    const Entity& e = world.entities[i];
    if (e.type != EntityType::HalfLight && e.type != EntityType::HalfShadow) continue;
    if (inside(e.x, e.y) && world.tiles[e.y][e.x] == Tile::Switch) return true;
  }
  return false;
}

bool DiptychActivity::computeLightDoorsOpen() const {
  // Plates in shadow world control light doors.
  const bool sLive = isPlatePressed(shadow_, shadowX_, shadowY_);
  const bool lLive = isPlatePressed(light_, lightX_, lightY_);
  if (linkedPlates_) {
    return lDoorsLatched_ || (sLive && lLive);
  }
  return lDoorsLatched_ || sLive;
}

bool DiptychActivity::computeShadowDoorsOpen() const {
  // Plates in light world control shadow doors.
  const bool lLive = isPlatePressed(light_, lightX_, lightY_);
  const bool sLive = isPlatePressed(shadow_, shadowX_, shadowY_);
  if (linkedPlates_) {
    return sDoorsLatched_ || (lLive && sLive);
  }
  return sDoorsLatched_ || lLive;
}

// ── Ghouls ──
bool DiptychActivity::hasGhouls(const World& world) const {
  for (int i = 0; i < world.count; ++i) {
    if (world.entities[i].type == EntityType::Ghoul) return true;
  }
  return false;
}

void DiptychActivity::moveGhoulsIn(World& world, int targetX, int targetY) {
  for (int i = 0; i < world.count; ++i) {
    Entity& g = world.entities[i];
    if (g.type != EntityType::Ghoul) continue;
    const int dxs = targetX - g.x;
    const int dys = targetY - g.y;
    const int dx = dxs == 0 ? 0 : (dxs > 0 ? 1 : -1);
    const int dy = dys == 0 ? 0 : (dys > 0 ? 1 : -1);
    const int adx = std::abs(dxs);
    const int ady = std::abs(dys);

    struct Move { int dx; int dy; };
    Move moves[2];
    if (adx >= ady) {
      moves[0] = {dx, 0};
      moves[1] = {0, dy};
    } else {
      moves[0] = {0, dy};
      moves[1] = {dx, 0};
    }
    for (int m = 0; m < 2; ++m) {
      const int mdx = moves[m].dx;
      const int mdy = moves[m].dy;
      if (mdx == 0 && mdy == 0) continue;
      const int nx = g.x + mdx;
      const int ny = g.y + mdy;
      if (!inside(nx, ny)) continue;
      const Tile tile = world.tiles[ny][nx];
      if (tile == Tile::Wall || tile == Tile::Tree || tile == Tile::Water) continue;
      // Don't stack on other ghouls
      bool blocked = false;
      for (int j = 0; j < world.count; ++j) {
        if (j == i) continue;
        const Entity& o = world.entities[j];
        if (o.type == EntityType::Ghoul && o.x == nx && o.y == ny) {
          blocked = true;
          break;
        }
      }
      if (blocked) continue;
      g.x = static_cast<int8_t>(nx);
      g.y = static_cast<int8_t>(ny);
      break;
    }
  }
}

bool DiptychActivity::checkGhoulCollision(const World& world, int x, int y) const {
  for (int i = 0; i < world.count; ++i) {
    const Entity& e = world.entities[i];
    if (e.type == EntityType::Ghoul && e.x == x && e.y == y) return true;
  }
  return false;
}

void DiptychActivity::onCaught(CaughtKind kind) {
  const uint8_t newHp = hp_ > 0 ? hp_ - 1 : 0;
  hp_ = newHp;
  if (!ambushed_) ambushed_ = true;
  caughtKind_ = (newHp == 0) ? CaughtKind::Dead : kind;
  screen_ = Screen::Caught;
}

void DiptychActivity::tickGhoulsAndCheck(int lx, int ly, int sx, int sy) {
  const bool hasL = hasGhouls(light_);
  const bool hasS = hasGhouls(shadow_);
  if (!hasL && !hasS) return;
  if (hasL) moveGhoulsIn(light_, lx, ly);
  if (hasS) moveGhoulsIn(shadow_, sx, sy);
  const bool lHit = hasL && checkGhoulCollision(light_, lx, ly);
  const bool sHit = hasS && checkGhoulCollision(shadow_, sx, sy);
  if (lHit || sHit) {
    onCaught(lHit ? CaughtKind::Light : CaughtKind::Shadow);
  }
}

// ── Movement core ──
bool DiptychActivity::tryWalkOrPush(World& world, EntityType halfType, int nx, int ny, int dx, int dy,
                                    bool execute, bool doorsOpen) {
  if (!inside(nx, ny)) return false;

  const int halfIndex = findHalfAt(world, nx, ny, halfType);
  if (halfIndex >= 0) {
    const int px = nx + dx;
    const int py = ny + dy;
    if (!inside(px, py) || !isWalkable(world.tiles[py][px], doorsOpen)) return false;
    // Blocked by any other entity at push destination?
    for (int i = 0; i < world.count; ++i) {
      if (i == halfIndex) continue;
      const Entity& o = world.entities[i];
      if (o.x == px && o.y == py) return false;
    }
    if (execute) {
      world.entities[halfIndex].x = static_cast<int8_t>(px);
      world.entities[halfIndex].y = static_cast<int8_t>(py);
    }
    return true;
  }

  return isWalkable(world.tiles[ny][nx], doorsOpen) && !hasBlockingEntity(world, nx, ny);
}

void DiptychActivity::tryTransition(int dx, int dy) {
  if (mirrorSteps_ > 0) {
    message_ = "The eye will not let you leave.";
    return;
  }
  int nrx = roomX_;
  int nry = roomY_;
  int entryX = lightX_;
  int entryY = lightY_;

  if (dx > 0) {
    ++nrx;
    entryX = 1;
    entryY = snapDoor(lightY_);
  } else if (dx < 0) {
    --nrx;
    entryX = GRID - 2;
    entryY = snapDoor(lightY_);
  } else if (dy > 0) {
    ++nry;
    entryY = 1;
    entryX = snapDoor(lightX_);
  } else if (dy < 0) {
    --nry;
    entryY = GRID - 2;
    entryX = snapDoor(lightX_);
  }

  // Bounded world
  if (std::abs(nrx) > WORLD_RADIUS || std::abs(nry) > WORLD_RADIUS) {
    message_ = "There is nothing beyond. The page ends here.";
    return;
  }

  World nextLight;
  World nextShadow;
  const bool savedLinked = linkedPlates_;
  // buildRoom mutates linkedPlates_ via const_cast; we restore if we can't enter.
  buildRoom(nrx, nry, nextLight, nextShadow);
  filterCollectedShardsFromWorld(nextLight);
  filterCollectedShardsFromWorld(nextShadow);

  const bool passable =
      isWalkable(nextLight.tiles[entryY][entryX], false) &&
      isWalkable(nextShadow.tiles[entryY][entryX], false) &&
      !hasBlockingEntity(nextLight, entryX, entryY) &&
      !hasBlockingEntity(nextShadow, entryX, entryY);

  if (passable) {
    light_ = nextLight;
    shadow_ = nextShadow;
    roomX_ = nrx;
    roomY_ = nry;
    lightX_ = shadowX_ = entryX;
    lightY_ = shadowY_ = entryY;
    lDoorsLatched_ = false;
    sDoorsLatched_ = false;
    needsFullRefresh_ = true;
    markVisited(nrx, nry);
    ++steps_;
    message_.clear();
  } else {
    linkedPlates_ = savedLinked;
    message_ = "The way is not yet.";
  }
}

void DiptychActivity::collectAlignedShard(uint8_t chapter, uint8_t index, int x, int y) {
  const uint8_t bit = 1u << index;
  switch (chapter) {
    case 1: shardsMask_ |= bit; break;
    case 2: voidShardsMask_ |= bit; break;
    case 3: echoShardsMask_ |= bit; break;
  }
  lightX_ = shadowX_ = x;
  lightY_ = shadowY_ = y;

  const int li = findHalfAt(light_, x, y, EntityType::HalfLight);
  if (li >= 0) removeEntity(light_, li);
  const int si = findHalfAt(shadow_, x, y, EntityType::HalfShadow);
  if (si >= 0) removeEntity(shadow_, si);

  const int count = shardCountForChapter(chapter);
  pickupMessage_ = shardPickupLine(chapter, count);
  screen_ = Screen::Pickup;
  ++steps_;
}

void DiptychActivity::updateAlignmentMessage() {
  int lx = -1, ly = -1, sx = -2, sy = -2;
  for (int i = 0; i < light_.count; ++i) {
    if (light_.entities[i].type == EntityType::HalfLight) {
      lx = light_.entities[i].x;
      ly = light_.entities[i].y;
      break;
    }
  }
  for (int i = 0; i < shadow_.count; ++i) {
    if (shadow_.entities[i].type == EntityType::HalfShadow) {
      sx = shadow_.entities[i].x;
      sy = shadow_.entities[i].y;
      break;
    }
  }
  if (lx == sx && ly == sy) {
    message_ = "They want to be one. Step onto them.";
  } else {
    message_.clear();
  }
}

void DiptychActivity::moveCoupled(int dx, int dy) {
  // Mirror mode inverts shadow movement
  const int mdx = mirrorSteps_ > 0 ? -dx : dx;
  const int mdy = mirrorSteps_ > 0 ? -dy : dy;

  const int nlx = lightX_ + dx;
  const int nly = lightY_ + dy;
  const int nsx = shadowX_ + mdx;
  const int nsy = shadowY_ + mdy;

  const bool lOff = !inside(nlx, nly);
  const bool sOff = !inside(nsx, nsy);
  if (lOff || sOff) {
    tryTransition(dx, dy);
    return;
  }

  const bool lDoorsOpen = computeLightDoorsOpen();
  const bool sDoorsOpen = computeShadowDoorsOpen();

  // Aligned shard collection
  const int lHalfIdx = findHalfAt(light_, nlx, nly, EntityType::HalfLight);
  const int sHalfIdx = findHalfAt(shadow_, nsx, nsy, EntityType::HalfShadow);
  if (lHalfIdx >= 0 && sHalfIdx >= 0) {
    const Entity& lh = light_.entities[lHalfIdx];
    const Entity& sh = shadow_.entities[sHalfIdx];
    if (lh.x == sh.x && lh.y == sh.y && lh.id == sh.id) {
      collectAlignedShard(chapter_, lh.id, nlx, nly);
      return;
    }
  }

  // Walkability for push or direct walk
  const bool lightOk = tryWalkOrPush(light_, EntityType::HalfLight, nlx, nly, dx, dy, false, lDoorsOpen);
  const bool shadowOk = tryWalkOrPush(shadow_, EntityType::HalfShadow, nsx, nsy, mdx, mdy, false, sDoorsOpen);
  if (!lightOk || !shadowOk) {
    if (!lightOk && !shadowOk) {
      message_ = "Both worlds refuse you.";
    } else if (!lightOk) {
      message_ = "The upper world will not yield.";
    } else {
      message_ = "The lower world will not yield.";
    }
    return;
  }

  tryWalkOrPush(light_, EntityType::HalfLight, nlx, nly, dx, dy, true, lDoorsOpen);
  tryWalkOrPush(shadow_, EntityType::HalfShadow, nsx, nsy, mdx, mdy, true, sDoorsOpen);
  lightX_ = nlx;
  lightY_ = nly;
  shadowX_ = nsx;
  shadowY_ = nsy;
  ++steps_;
  updateAlignmentMessage();

  // Linked-plate latching: if both plates pressed simultaneously, latch doors
  if (linkedPlates_) {
    const bool lPressedNow = isPlatePressed(light_, lightX_, lightY_);
    const bool sPressedNow = isPlatePressed(shadow_, shadowX_, shadowY_);
    if (lPressedNow && sPressedNow) {
      lDoorsLatched_ = true;
      sDoorsLatched_ = true;
    }
  }

  // Mirror mode tick
  if (mirrorSteps_ > 0) {
    --mirrorSteps_;
    if (mirrorSteps_ <= 0) {
      lightX_ = shadowX_ = mirrorAnchorX_;
      lightY_ = shadowY_ = mirrorAnchorY_;
      message_ = "The eye closes. You return to yourself.";
    } else {
      char buf[48];
      snprintf(buf, sizeof(buf), "The eye is watching. %d breaths remain.", mirrorSteps_);
      message_ = buf;
    }
  } else if (inside(lightX_, lightY_) && light_.tiles[lightY_][lightX_] == Tile::Mirror) {
    mirrorSteps_ = MIRROR_STEPS;
    mirrorAnchorX_ = lightX_;
    mirrorAnchorY_ = lightY_;
    char buf[64];
    snprintf(buf, sizeof(buf), "The eye opens. %d inverted breaths.", MIRROR_STEPS);
    message_ = buf;
  } else if (inside(shadowX_, shadowY_) && shadow_.tiles[shadowY_][shadowX_] == Tile::Mirror) {
    mirrorSteps_ = MIRROR_STEPS;
    mirrorAnchorX_ = shadowX_;
    mirrorAnchorY_ = shadowY_;
    char buf[64];
    snprintf(buf, sizeof(buf), "The eye opens. %d inverted breaths.", MIRROR_STEPS);
    message_ = buf;
  }

  tickGhoulsAndCheck(lightX_, lightY_, shadowX_, shadowY_);
}

void DiptychActivity::moveSplit(int dx, int dy) {
  if (splitSteps_ <= 0) {
    snapSplit();
    return;
  }

  const bool soloLight = splitMode_ == SplitMode::Light;
  World& world = soloLight ? light_ : shadow_;
  int& px = soloLight ? lightX_ : shadowX_;
  int& py = soloLight ? lightY_ : shadowY_;
  const EntityType halfType = soloLight ? EntityType::HalfLight : EntityType::HalfShadow;
  const int nx = px + dx;
  const int ny = py + dy;

  if (!inside(nx, ny)) {
    message_ = "The thread holds. It will not let you go there.";
    return;
  }

  const bool doorsOpen = soloLight ? computeLightDoorsOpen() : computeShadowDoorsOpen();
  if (!tryWalkOrPush(world, halfType, nx, ny, dx, dy, false, doorsOpen)) {
    message_ = soloLight ? "The upper world refuses you." : "The lower world refuses you.";
    return;
  }
  tryWalkOrPush(world, halfType, nx, ny, dx, dy, true, doorsOpen);
  px = nx;
  py = ny;
  --splitSteps_;
  ++steps_;

  updateAlignmentMessage();
  if (message_.empty()) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s walks alone. %d breaths remain.", soloLight ? "Light" : "Shadow", splitSteps_);
    message_ = buf;
  }
  if (splitSteps_ <= 0) {
    message_ = "The thread is spent. One more step, and it pulls you home.";
  }

  tickGhoulsAndCheck(lightX_, lightY_, shadowX_, shadowY_);
}

void DiptychActivity::snapSplit(const char* text) {
  if (splitMode_ == SplitMode::Light) {
    lightX_ = splitAnchorX_;
    lightY_ = splitAnchorY_;
  } else if (splitMode_ == SplitMode::Shadow) {
    shadowX_ = splitAnchorX_;
    shadowY_ = splitAnchorY_;
  }
  splitMode_ = SplitMode::None;
  splitSteps_ = 0;
  message_ = text;
}

void DiptychActivity::snapMirror(const char* text) {
  lightX_ = shadowX_ = mirrorAnchorX_;
  lightY_ = shadowY_ = mirrorAnchorY_;
  mirrorSteps_ = 0;
  message_ = text;
}

void DiptychActivity::toggleLightSplit() {
  if (splitMode_ == SplitMode::Light) {
    snapSplit();
    return;
  }
  if (splitMode_ != SplitMode::None) {
    message_ = "Finish the first walk. Then begin another.";
    return;
  }
  if (mirrorSteps_ > 0) {
    snapMirror();
    return;
  }
  splitMode_ = SplitMode::Light;
  splitSteps_ = SPLIT_STEPS;
  splitAnchorX_ = lightX_;
  splitAnchorY_ = lightY_;
  char buf[48];
  snprintf(buf, sizeof(buf), "Light walks alone. %d breaths remain.", SPLIT_STEPS);
  message_ = buf;
}

void DiptychActivity::toggleShadowSplit() {
  if (splitMode_ == SplitMode::Shadow) {
    snapSplit();
    return;
  }
  if (splitMode_ != SplitMode::None) {
    message_ = "Finish the first walk. Then begin another.";
    return;
  }
  if (mirrorSteps_ > 0) {
    snapMirror();
    return;
  }
  splitMode_ = SplitMode::Shadow;
  splitSteps_ = SPLIT_STEPS;
  splitAnchorX_ = shadowX_;
  splitAnchorY_ = shadowY_;
  char buf[48];
  snprintf(buf, sizeof(buf), "Shadow walks alone. %d breaths remain.", SPLIT_STEPS);
  message_ = buf;
}

// ── Dialogue ──
bool DiptychActivity::startAdjacentDialogueOrSign() {
  static constexpr int dirs[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
  // First check the LIGHT world (same as web version)
  for (const auto& dir : dirs) {
    const int ax = lightX_ + dir[0];
    const int ay = lightY_ + dir[1];
    if (!inside(ax, ay)) continue;
    const int idx = findEntityAt(light_, ax, ay);
    if (idx < 0) continue;
    const Entity& e = light_.entities[idx];
    if (e.type == EntityType::Npc) {
      if (e.id == NPC_SAGE) openSageDialogue();
      else openWatcherDialogue();
      return true;
    }
    if (e.type == EntityType::Sign) {
      openSignDialogue(e.id);
      return true;
    }
  }
  // Then shadow world
  for (const auto& dir : dirs) {
    const int ax = shadowX_ + dir[0];
    const int ay = shadowY_ + dir[1];
    if (!inside(ax, ay)) continue;
    const int idx = findEntityAt(shadow_, ax, ay);
    if (idx < 0) continue;
    const Entity& e = shadow_.entities[idx];
    if (e.type == EntityType::Npc) {
      if (e.id == NPC_SAGE) openSageDialogue();
      else openWatcherDialogue();
      return true;
    }
    if (e.type == EntityType::Sign) {
      openSignDialogue(e.id);
      return true;
    }
  }
  return false;
}

static std::string hintFrom(const ShardDef* defs, int count, uint8_t mask) {
  std::string s = "Pieces wait in the ";
  bool first = true;
  for (int i = 0; i < count; ++i) {
    if (mask & (1u << i)) continue;
    if (!first) s += ", ";
    first = false;
    s += directionWord(defs[i].rx, defs[i].ry);
  }
  s += ".";
  return s;
}

void DiptychActivity::openWatcherDialogue() {
  dialogue_.clear();
  pendingVictory_ = false;

  if (chapter_ >= 3) {
    const int ec = shardCountForChapter(3);
    if (ec >= SHARDS_PER_CHAPTER && !victory_) {
      dialogue_.push_back("Come stand where both of you can see.");
      dialogue_.push_back("The mirror is listening.");
      pendingVictory_ = true;
    } else if (victory_) {
      dialogue_.push_back("Fifteen pieces. One figure. No name.");
      dialogue_.push_back("I will not watch you from here again.");
      dialogue_.push_back("Whatever you are now — walk.");
    } else if (ec >= 3) {
      dialogue_.push_back("You have stopped being two things.");
      dialogue_.push_back("You have not yet become one.");
      dialogue_.push_back("This is where most travelers end.");
      dialogue_.push_back(hintFrom(SHARDS_CH3, SHARDS_PER_CHAPTER, echoShardsMask_));
    } else if (ec >= 1) {
      dialogue_.push_back("The reflection has begun to answer.");
      dialogue_.push_back("I cannot tell yours from mine anymore.");
      dialogue_.push_back(hintFrom(SHARDS_CH3, SHARDS_PER_CHAPTER, echoShardsMask_));
    } else {
      dialogue_.push_back("There is a place where forward is also back.");
      dialogue_.push_back("Step onto the eye. The eye will look at you.");
      dialogue_.push_back("Your shadow will walk against you. Do not be afraid of it.");
      dialogue_.push_back("It is only what you have always been doing, made visible.");
      dialogue_.push_back(hintFrom(SHARDS_CH3, SHARDS_PER_CHAPTER, echoShardsMask_));
    }
  } else if (chapter_ >= 2) {
    const int vc = shardCountForChapter(2);
    if (vc >= SHARDS_PER_CHAPTER && !victory_) {
      dialogue_.push_back("The deep answers. Return to me.");
      dialogue_.push_back("Do not look at it for too long.");
      pendingVictory_ = true;
    } else if (victory_) {
      dialogue_.push_back("Two rifts sealed. A mirror remains.");
      dialogue_.push_back("I had hoped you would not find it.");
      dialogue_.push_back("I am not sure which of us is on its other side.");
    } else if (vc >= 3) {
      dialogue_.push_back("Some of the walls are not walls.");
      dialogue_.push_back("Some of the locks are kind. Most are not.");
      dialogue_.push_back(hintFrom(SHARDS_CH2, SHARDS_PER_CHAPTER, voidShardsMask_));
    } else if (vc >= 1) {
      dialogue_.push_back("You remember the first time you split. Hold that memory.");
      dialogue_.push_back("Down here it is the only warm thing.");
      dialogue_.push_back(hintFrom(SHARDS_CH2, SHARDS_PER_CHAPTER, voidShardsMask_));
    } else {
      dialogue_.push_back("Below the rift there is another rift.");
      dialogue_.push_back("Below that I will not say.");
      dialogue_.push_back("Five shards in the dark below. They do not want you.");
      dialogue_.push_back("They have been alone a long time.");
      dialogue_.push_back(hintFrom(SHARDS_CH2, SHARDS_PER_CHAPTER, voidShardsMask_));
    }
  } else {
    const int sc = shardCountForChapter(1);
    if (sc >= SHARDS_PER_CHAPTER && !victory_) {
      dialogue_.push_back("Five. Enough for a hand to close.");
      dialogue_.push_back("Come here. Stand where I stand.");
      pendingVictory_ = true;
    } else if (victory_) {
      dialogue_.push_back("The seam is stitched. The wound is not.");
      dialogue_.push_back("There is a deeper cut. I did not want to tell you.");
      dialogue_.push_back("I still do not.");
    } else if (sc >= 3) {
      dialogue_.push_back("You are nearly a shape again.");
      dialogue_.push_back("I will not say what. Names are for the finished.");
      dialogue_.push_back(hintFrom(SHARDS_CH1, SHARDS_PER_CHAPTER, shardsMask_));
    } else if (sc >= 1) {
      dialogue_.push_back("One piece remembers. The others will not take long.");
      dialogue_.push_back("Do not call them back. They were never lost.");
      dialogue_.push_back(hintFrom(SHARDS_CH1, SHARDS_PER_CHAPTER, shardsMask_));
    } else {
      dialogue_.push_back("You came as two. Most do not notice.");
      dialogue_.push_back("One half walks in daylight. One half in its absence.");
      dialogue_.push_back("Neither is the true one.");
      dialogue_.push_back("Something was broken before the world had a name for breaking.");
      dialogue_.push_back("Its pieces rest where both halves must meet.");
      dialogue_.push_back("Go. Press yourself against what resists you.");
      dialogue_.push_back(hintFrom(SHARDS_CH1, SHARDS_PER_CHAPTER, shardsMask_));
    }
  }

  dialogueLine_ = 0;
  screen_ = Screen::Dialogue;
}

void DiptychActivity::openSageDialogue() {
  dialogue_.clear();
  pendingVictory_ = false;
  if (chapter_ >= 2) {
    if (shardCountForChapter(2) >= SHARDS_PER_CHAPTER) {
      dialogue_.push_back("The void has taken your measure.");
      dialogue_.push_back("It finds you sufficient. I am sorry.");
    } else {
      dialogue_.push_back("The deep bends the rule you learned above.");
      dialogue_.push_back("Patience was the first lesson. Precision is the second.");
      dialogue_.push_back("The third I will not teach you.");
    }
  } else if (shardCountForChapter(1) >= SHARDS_PER_CHAPTER) {
    dialogue_.push_back("I heard the last bell. So did the things that remember bells.");
    dialogue_.push_back("Go to her. Do not linger in the doorway.");
  } else {
    dialogue_.push_back("A wall is a question the stone has been asking a long time.");
    dialogue_.push_back("It does not expect to be answered.");
    dialogue_.push_back("Push only what you can bring back.");
  }
  dialogueLine_ = 0;
  screen_ = Screen::Dialogue;
}

void DiptychActivity::openSignDialogue(uint8_t id) {
  dialogue_.clear();
  pendingVictory_ = false;
  switch (id) {
    case SIGN_SPLIT_LIGHT:
      dialogue_.push_back("Split the light. Five breaths before the thread pulls taut.");
      break;
    case SIGN_SPLIT_SHADOW:
      dialogue_.push_back("Split the dark. Five breaths. No more.");
      break;
    case SIGN_GRAVE_L1:
      dialogue_.push_back("Here rests one who was two.");
      dialogue_.push_back("They forgot which half was first.");
      break;
    case SIGN_GRAVE_L2:
      dialogue_.push_back("Here begins the one who carved the hinge.");
      dialogue_.push_back("—  J A M E S  —");
      dialogue_.push_back("@JLUDDY7");
      dialogue_.push_back("x.com/JLUDDY7");
      break;
    case SIGN_GRAVE_L3:
      dialogue_.push_back("Almost a shape again.");
      dialogue_.push_back("Almost.");
      break;
    case SIGN_GRAVE_L4:
      dialogue_.push_back("Name forgotten.");
      dialogue_.push_back("Halves never met.");
      break;
    case SIGN_GRAVE_L5:
      dialogue_.push_back("They lost the thread.");
      break;
    case SIGN_GRAVE_L6:
      dialogue_.push_back("They looked too long at the light.");
      break;
    case SIGN_GRAVE_S1:
      dialogue_.push_back("The dark was kind.");
      dialogue_.push_back("Or so she said.");
      break;
    case SIGN_GRAVE_S2:
      dialogue_.push_back("The stone does not know who lies beneath.");
      dialogue_.push_back("Neither do you.");
      break;
    case SIGN_GRAVE_S3:
      dialogue_.push_back("She walked through the mirror.");
      dialogue_.push_back("She did not walk back.");
      break;
    case SIGN_GRAVE_S4:
      dialogue_.push_back("The hinge forgot them.");
      break;
    case SIGN_GRAVE_S5:
      dialogue_.push_back("They thought they were one.");
      dialogue_.push_back("They were not yet two.");
      break;
    case SIGN_GRAVE_S6:
      dialogue_.push_back("He came twice.");
      dialogue_.push_back("Once as light. Once as dark.");
      dialogue_.push_back("Neither returned.");
      break;
    case SIGN_FD_L1:
      dialogue_.push_back("A figure of light.");
      dialogue_.push_back("Standing as it was before the panels parted.");
      break;
    case SIGN_FD_L2:
      dialogue_.push_back("A figure of shadow.");
      dialogue_.push_back("Standing as it was before the panels parted.");
      break;
    case SIGN_FD_PLAQUE:
      dialogue_.push_back("Before the panels parted.");
      dialogue_.push_back("Before the hinge.");
      dialogue_.push_back("There was only a figure.");
      break;
    case SIGN_FD_S1:
      dialogue_.push_back("A figure of light.");
      dialogue_.push_back("It is watching you. So are you.");
      break;
    case SIGN_FD_S2:
      dialogue_.push_back("A figure of shadow.");
      dialogue_.push_back("It is watching you. So are you.");
      break;
    case SIGN_EMPTY_WATCHER:
      dialogue_.push_back("The one who waits is not here.");
      dialogue_.push_back("She has gone to look for what she lost.");
      dialogue_.push_back("Or perhaps she was never here at all.");
      break;
    case SIGN_BELL_L:
      dialogue_.push_back("The bell has already rung.");
      dialogue_.push_back("You could not have heard it.");
      dialogue_.push_back("You were not yet two.");
      break;
    case SIGN_BELL_S:
      dialogue_.push_back("The sound is still in the stone.");
      dialogue_.push_back("The stone is still listening.");
      break;
    case SIGN_SHRINE_L:
      dialogue_.push_back("Here the depth becomes deeper.");
      dialogue_.push_back("The bottom is still above us.");
      dialogue_.push_back("Do not look down.");
      break;
    case SIGN_SHRINE_S:
      dialogue_.push_back("The water does not ripple.");
      dialogue_.push_back("It has been listening for a long time.");
      break;
    case SIGN_WELLS_L:
      dialogue_.push_back("Two wells. One water.");
      dialogue_.push_back("Drop something in.");
      dialogue_.push_back("Another you will find it.");
      break;
    case SIGN_WELLS_S:
      dialogue_.push_back("The wells are older than the rift.");
      dialogue_.push_back("They were already listening when the panels parted.");
      break;
    case SIGN_POOL_L:
      dialogue_.push_back("The eye is resting.");
      dialogue_.push_back("Do not wake it.");
      dialogue_.push_back("It dreams of you walking backward.");
      break;
    case SIGN_POOL_S:
      dialogue_.push_back("Some mirrors are eyes that have closed.");
      dialogue_.push_back("Some have only blinked.");
      break;
    case SIGN_REFL_L:
      dialogue_.push_back("Here lies you.");
      dialogue_.push_back("You have not yet died.");
      dialogue_.push_back("The grave is patient.");
      break;
    case SIGN_REFL_S:
      dialogue_.push_back("Something is lying face-down.");
      dialogue_.push_back("It has your outline.");
      dialogue_.push_back("It is breathing. Almost.");
      break;
    default:
      dialogue_.push_back("...");
      break;
  }
  dialogueLine_ = 0;
  screen_ = Screen::Dialogue;
}

void DiptychActivity::advanceDialogue() {
  if (dialogueLine_ + 1 < dialogue_.size()) {
    ++dialogueLine_;
    return;
  }
  if (pendingVictory_) {
    victory_ = true;
    pendingVictory_ = false;
    screen_ = Screen::Victory;
  } else {
    screen_ = Screen::Play;
  }
}

// ── Loop / input ──
void DiptychActivity::loop() {
  if (mappedInput.wasPressed(Button::Back)) {
    backConsumed_ = false;
  }
  if (mappedInput.isPressed(Button::Back) && !backConsumed_ && mappedInput.getHeldTime() >= EXIT_HOLD_MS) {
    backConsumed_ = true;
    finish();
    return;
  }

  if (screen_ == Screen::Intro) {
    if (mappedInput.wasReleased(Button::Confirm)) {
      screen_ = Screen::Controls;
      needsFullRefresh_ = true;
      requestUpdate();
    }
    return;
  }

  if (screen_ == Screen::Controls) {
    if (mappedInput.wasReleased(Button::Confirm)) {
      screen_ = Screen::Play;
      message_ = "Find the one who waits. She will know you are two.";
      needsFullRefresh_ = true;
      requestUpdate();
    }
    return;
  }

  if (screen_ == Screen::Pickup) {
    if (mappedInput.wasReleased(Button::Confirm) || (mappedInput.wasReleased(Button::Back) && !backConsumed_)) {
      screen_ = Screen::Play;
      requestUpdate();
    }
    return;
  }

  if (screen_ == Screen::Caught) {
    if (mappedInput.wasReleased(Button::Confirm) || (mappedInput.wasReleased(Button::Back) && !backConsumed_)) {
      const bool dead = caughtKind_ == CaughtKind::Dead;
      caughtKind_ = CaughtKind::None;
      if (dead) {
        resetChapter();
      } else {
        message_ = "You wake where she waits. She does not look up.";
      }
      respawnAtWatcher();
      screen_ = Screen::Play;
      requestUpdate();
    }
    return;
  }

  if (screen_ == Screen::Dialogue) {
    if (mappedInput.wasReleased(Button::Confirm)) {
      advanceDialogue();
      needsFullRefresh_ = true;
      requestUpdate();
    } else if (mappedInput.wasReleased(Button::Back) && !backConsumed_) {
      screen_ = Screen::Play;
      pendingVictory_ = false;
      requestUpdate();
    }
    return;
  }

  if (screen_ == Screen::Victory) {
    if (mappedInput.wasReleased(Button::Confirm)) {
      if (chapter_ == 1) {
        chapter_ = 2;
        victory_ = false;
        message_ = "The deep opens its eye. Five pieces in the dark.";
        respawnAtWatcher();
        screen_ = Screen::Play;
      } else if (chapter_ == 2) {
        chapter_ = 3;
        victory_ = false;
        message_ = "The hinge watches. Five reflections wait.";
        respawnAtWatcher();
        screen_ = Screen::Play;
      } else {
        screen_ = Screen::Play;
        message_ = "The panels close. Walk where you will.";
      }
      needsFullRefresh_ = true;
      requestUpdate();
    } else if (mappedInput.wasReleased(Button::Back) && !backConsumed_) {
      finish();
    }
    return;
  }

  // Play screen
  bool updated = false;
  if (mappedInput.wasReleased(Button::Back) && !backConsumed_) {
    toggleLightSplit();
    updated = true;
  } else if (mappedInput.wasReleased(Button::Confirm)) {
    if (splitMode_ == SplitMode::None && startAdjacentDialogueOrSign()) {
      updated = true;
    } else {
      toggleShadowSplit();
      updated = true;
    }
  } else {
    int dx = 0;
    int dy = 0;
    if (mappedInput.wasPressed(Button::Up)) {
      dy = -1;
    } else if (mappedInput.wasPressed(Button::Down)) {
      dy = 1;
    } else if (mappedInput.wasPressed(Button::Left)) {
      dx = -1;
    } else if (mappedInput.wasPressed(Button::Right)) {
      dx = 1;
    }
    if (dx != 0 || dy != 0) {
      if (splitMode_ == SplitMode::None) {
        moveCoupled(dx, dy);
      } else {
        moveSplit(dx, dy);
      }
      updated = true;
    }
  }

  if (updated) {
    requestUpdate();
  }
}

// ── Sprite primitives ──
void DiptychActivity::drawDiamond(int cx, int cy, int radius, bool filled, bool black) const {
  for (int dy = -radius; dy <= radius; ++dy) {
    const int w = radius - std::abs(dy);
    if (filled) {
      renderer.fillRect(cx - w, cy + dy, w * 2 + 1, 1, black);
    } else {
      renderer.drawPixel(cx - w, cy + dy, black);
      renderer.drawPixel(cx + w, cy + dy, black);
    }
  }
}

void DiptychActivity::drawShardHalf(int tileX, int tileY, EntityType type, bool black) const {
  const int cx = tileX + TILE / 2;
  const int cy = tileY + TILE / 2;
  drawDiamond(cx, cy, 10, false, black);
  drawDiamond(cx, cy, 8, false, black);
  const int yStart = type == EntityType::HalfLight ? -8 : 0;
  const int yEnd = type == EntityType::HalfLight ? 0 : 8;
  for (int dy = yStart; dy <= yEnd; ++dy) {
    const int w = 8 - std::abs(dy);
    if (w > 0) {
      renderer.fillRect(cx - w, cy + dy, w * 2 + 1, 1, black);
    }
  }
  drawDiamond(cx, cy, 3, true, black);
}

void DiptychActivity::drawShardMerged(int tileX, int tileY, bool black) const {
  const int cx = tileX + TILE / 2;
  const int cy = tileY + TILE / 2;
  drawDiamond(cx, cy, 10, true, black);
}

void DiptychActivity::drawPlayerSprite(int x, int y, bool black) const {
  renderer.fillRect(x + 9, y + 4, 7, 7, black);
  renderer.fillRect(x + 7, y + 11, 11, 8, black);
  renderer.fillRect(x + 6, y + 18, 5, 3, black);
  renderer.fillRect(x + 14, y + 18, 5, 3, black);
  renderer.drawLine(x + 4, y + 14, x + 2, y + 22, black);
}

void DiptychActivity::drawNpcSprite(int x, int y, bool black) const {
  renderer.fillRect(x + 8, y + 3, 8, 7, black);
  renderer.drawPixel(x + 10, y + 6, !black);
  renderer.drawPixel(x + 13, y + 6, !black);
  renderer.fillRect(x + 6, y + 10, 13, 12, black);
  renderer.drawLine(x + 20, y + 9, x + 20, y + 23, black);
}

void DiptychActivity::drawGhoulSprite(int x, int y, bool black) const {
  // Head with fangs
  renderer.fillRect(x + 8, y + 2, 8, 6, black);
  renderer.drawPixel(x + 10, y + 4, !black);
  renderer.drawPixel(x + 13, y + 4, !black);
  // Body (ragged)
  renderer.fillRect(x + 6, y + 8, 13, 10, black);
  // Arms splayed
  renderer.drawLine(x + 4, y + 10, x + 2, y + 18, black);
  renderer.drawLine(x + 19, y + 10, x + 21, y + 18, black);
  // Feet
  renderer.fillRect(x + 8, y + 18, 3, 4, black);
  renderer.fillRect(x + 13, y + 18, 3, 4, black);
}

void DiptychActivity::drawWallSprite(int x, int y, bool black) const {
  renderer.fillRect(x, y, TILE, TILE, black);
  renderer.drawLine(x, y + 7, x + TILE - 1, y + 7, !black);
  renderer.drawLine(x, y + 15, x + TILE - 1, y + 15, !black);
  renderer.drawLine(x + 7, y, x + 7, y + 7, !black);
  renderer.drawLine(x + 15, y + 8, x + 15, y + 15, !black);
  renderer.drawLine(x + 11, y + 16, x + 11, y + TILE - 1, !black);
}

void DiptychActivity::drawTreeSprite(int x, int y, bool black) const {
  drawDiamond(x + 12, y + 9, 9, true, black);
  renderer.fillRect(x + 10, y + 15, 4, 8, black);
}

void DiptychActivity::drawWaterSprite(int x, int y, bool black) const {
  // Horizontal wavy stripes
  for (int wy = 3; wy < TILE; wy += 6) {
    for (int wx = 0; wx < TILE; wx++) {
      renderer.drawPixel(x + wx, y + wy + ((wx >> 2) & 1), black);
    }
  }
}

void DiptychActivity::drawDoorSprite(int x, int y, bool open, bool black) const {
  if (open) {
    // Faint dotted outline
    for (int di = 0; di < TILE; di += 4) {
      renderer.fillRect(x + di, y, 2, 1, black);
      renderer.fillRect(x + di, y + TILE - 1, 2, 1, black);
      renderer.fillRect(x, y + di, 1, 2, black);
      renderer.fillRect(x + TILE - 1, y + di, 1, 2, black);
    }
  } else {
    // Solid door with cross bar
    renderer.drawRect(x + 3, y + 1, TILE - 6, TILE - 3, black);
    renderer.drawRect(x + 4, y + 2, TILE - 8, TILE - 5, black);
    renderer.fillRect(x + TILE / 2 - 2, y + TILE / 2 - 1, 4, 2, black);
  }
}

void DiptychActivity::drawPlateSprite(int x, int y, bool black) const {
  // Low pressure plate near bottom of the tile
  renderer.drawRect(x + 4, y + TILE - 8, TILE - 8, 5, black);
  renderer.drawPixel(x + 6, y + TILE - 6, black);
  renderer.drawPixel(x + TILE - 7, y + TILE - 6, black);
}

void DiptychActivity::drawMirrorSprite(int x, int y, bool black) const {
  // Eye-shaped mirror
  drawDiamond(x + 12, y + 8, 8, false, black);
  drawDiamond(x + 12, y + 8, 4, true, black);
  drawDiamond(x + 12, y + 16, 8, false, black);
  drawDiamond(x + 12, y + 16, 4, false, black);
}

void DiptychActivity::drawHeadstoneSprite(int x, int y, bool black) const {
  // Rounded-top stone
  drawDiamond(x + 12, y + 6, 4, true, black);
  renderer.fillRect(x + 8, y + 6, 9, TILE - 8, black);
  renderer.drawRect(x + 9, y + 9, 7, TILE - 12, !black);
  // Inscription lines
  renderer.drawLine(x + 10, y + 11, x + 14, y + 11, !black);
  renderer.drawLine(x + 10, y + 14, x + 14, y + 14, !black);
}

void DiptychActivity::drawBellSprite(int x, int y, bool black) const {
  drawDiamond(x + 12, y + 10, 8, true, black);
  renderer.fillRect(x + 7, y + 17, 11, 2, black);
  renderer.fillRect(x + 11, y + 19, 3, 3, black);
}

void DiptychActivity::drawSignSprite(int x, int y, bool black) const {
  renderer.drawRect(x + 4, y + 4, 16, 10, black);
  renderer.fillRect(x + 11, y + 14, 2, 8, black);
}

void DiptychActivity::drawSignEntity(const Entity& e, int px, int py, bool black) const {
  switch (e.signSprite) {
    case SignSprite::Headstone: drawHeadstoneSprite(px, py, black); break;
    case SignSprite::Bell: drawBellSprite(px, py, black); break;
    case SignSprite::MirrorEye: drawMirrorSprite(px, py, black); break;
    case SignSprite::PlayerLight:
    case SignSprite::PlayerShadow:
      drawPlayerSprite(px, py, black);
      break;
    case SignSprite::Sign:
    default:
      drawSignSprite(px, py, black);
      break;
  }
}

// ── Main scene drawing ──
void DiptychActivity::drawWorld(const World& world, int playerX, int playerY, int originY, bool shadowWorld,
                                bool frozen, bool doorsOpen) const {
  const bool ink = !shadowWorld;
  renderer.fillRect(VIEW_X, originY, VIEW, VIEW, shadowWorld);
  renderer.drawRect(VIEW_X - 1, originY - 1, VIEW + 2, VIEW + 2, ink);

  for (int y = 0; y < GRID; ++y) {
    for (int x = 0; x < GRID; ++x) {
      const int px = VIEW_X + x * TILE;
      const int py = originY + y * TILE;
      const Tile t = world.tiles[y][x];
      if (t == Tile::Wall) {
        drawWallSprite(px, py, ink);
      } else if (t == Tile::Tree) {
        drawTreeSprite(px, py, ink);
      } else if (t == Tile::Water) {
        drawWaterSprite(px, py, ink);
      } else if (t == Tile::Door) {
        drawDoorSprite(px, py, doorsOpen, ink);
      } else if (t == Tile::Switch) {
        drawPlateSprite(px, py, ink);
      } else if (t == Tile::Mirror) {
        drawMirrorSprite(px, py, ink);
      }
    }
  }

  for (int i = 0; i < world.count; ++i) {
    const Entity& e = world.entities[i];
    const int px = VIEW_X + e.x * TILE;
    const int py = originY + e.y * TILE;
    switch (e.type) {
      case EntityType::Npc: drawNpcSprite(px, py, ink); break;
      case EntityType::Sign: drawSignEntity(e, px, py, ink); break;
      case EntityType::Ghoul: drawGhoulSprite(px, py, ink); break;
      case EntityType::HalfLight:
      case EntityType::HalfShadow:
        drawShardHalf(px, py, e.type, ink);
        break;
      default: break;
    }
  }

  const int ppx = VIEW_X + playerX * TILE;
  const int ppy = originY + playerY * TILE;
  drawPlayerSprite(ppx, ppy, ink);
  if (frozen) {
    renderer.drawRect(ppx - 3, ppy - 3, TILE + 6, TILE + 6, ink);
  }
}

void DiptychActivity::drawSplitMeter(SplitMode mode, int originY, bool inverse) const {
  const bool active = splitMode_ == mode;
  const int remaining = active ? splitSteps_ : 0;
  const int x = VIEW_X - 27;
  const int y0 = originY + VIEW / 2 - 34;
  if (inverse) {
    renderer.fillRect(x - 9, y0 - 8, 18, 84, true);
    renderer.drawRect(x - 9, y0 - 8, 18, 84, true);
  }
  for (int i = 0; i < SPLIT_STEPS; ++i) {
    drawDiamond(x, y0 + i * 16, 5, i < remaining, !inverse);
  }
}

void DiptychActivity::drawMirrorMeter() const {
  if (mirrorSteps_ <= 0) return;
  renderer.drawText(SMALL_FONT_ID, 6, DIVIDER_Y - 2, "MIRROR", true, EpdFontFamily::BOLD);
  for (int i = 0; i < MIRROR_STEPS; ++i) {
    drawDiamond(14 + i * 10, DIVIDER_Y + 6, 3, i < mirrorSteps_, true);
  }
}

void DiptychActivity::drawShardRows() const {
  // Tiny diamond glyph (7px) drawing helper inline.
  auto drawDia = [&](int x, int y, bool filled) {
    renderer.drawPixel(x + 3, y, true);
    if (filled) {
      renderer.fillRect(x + 2, y + 1, 3, 1, true);
      renderer.fillRect(x + 1, y + 2, 5, 1, true);
      renderer.fillRect(x + 0, y + 3, 7, 1, true);
      renderer.fillRect(x + 1, y + 4, 5, 1, true);
      renderer.fillRect(x + 2, y + 5, 3, 1, true);
      renderer.drawPixel(x + 3, y + 6, true);
    } else {
      renderer.drawPixel(x + 2, y + 1, true);
      renderer.drawPixel(x + 4, y + 1, true);
      renderer.drawPixel(x + 1, y + 2, true);
      renderer.drawPixel(x + 5, y + 2, true);
      renderer.drawPixel(x + 0, y + 3, true);
      renderer.drawPixel(x + 6, y + 3, true);
      renderer.drawPixel(x + 1, y + 4, true);
      renderer.drawPixel(x + 5, y + 4, true);
      renderer.drawPixel(x + 2, y + 5, true);
      renderer.drawPixel(x + 4, y + 5, true);
      renderer.drawPixel(x + 3, y + 6, true);
    }
  };

  auto drawRow = [&](int count, uint8_t mask, int baseX, int y) {
    for (int i = 0; i < count; ++i) {
      const int sx = baseX + i * 14;
      drawDia(sx, y, (mask & (1u << i)) != 0);
    }
  };

  const int W = 480;
  const int y = 391;
  if (chapter_ >= 3) {
    const int groupW = 14 * (SHARDS_PER_CHAPTER - 1) + 7;
    const int gap = 14;
    const int totalW = groupW * 3 + gap * 2;
    const int baseX = W / 2 - totalW / 2;
    drawRow(SHARDS_PER_CHAPTER, shardsMask_, baseX, y);
    drawRow(SHARDS_PER_CHAPTER, voidShardsMask_, baseX + groupW + gap, y);
    drawRow(SHARDS_PER_CHAPTER, echoShardsMask_, baseX + (groupW + gap) * 2, y);
  } else if (chapter_ >= 2) {
    drawRow(SHARDS_PER_CHAPTER, shardsMask_, W / 2 - SHARDS_PER_CHAPTER * 14 - 4, y);
    drawRow(SHARDS_PER_CHAPTER, voidShardsMask_, W / 2 + 4, y);
  } else {
    drawRow(SHARDS_PER_CHAPTER, shardsMask_, W / 2 - SHARDS_PER_CHAPTER * 7, y);
  }
}

void DiptychActivity::drawMinimap() const {
  constexpr int CELL = 7;
  constexpr int SIZE = 7;
  const int x0 = VIEW_X + VIEW + 5;
  const int y0 = LIGHT_Y + 5;
  renderer.drawText(SMALL_FONT_ID, x0 + 4, y0 - 8, "MAP", true, EpdFontFamily::BOLD);
  renderer.drawRect(x0 - 1, y0 - 1, SIZE * CELL + 2, SIZE * CELL + 2, true);
  const int half = SIZE / 2;
  for (int my = 0; my < SIZE; ++my) {
    for (int mx = 0; mx < SIZE; ++mx) {
      const int rx = roomX_ + mx - half;
      const int ry = roomY_ + my - half;
      const int px = x0 + mx * CELL;
      const int py = y0 + my * CELL;
      const bool isCur = mx == half && my == half;
      const bool isHome = (rx == 0 && ry == 0) && !isCur;
      const bool hasShard = anyShardRemainingAt(rx, ry);
      const bool isVis = wasVisited(rx, ry);
      if (isCur) {
        renderer.fillRect(px + 1, py + 1, CELL - 2, CELL - 2, true);
      } else if (isHome) {
        renderer.drawRect(px + 1, py + 1, CELL - 2, CELL - 2, true);
      } else if (hasShard) {
        drawDiamond(px + 3, py + 3, 3, true, true);
      } else if (isVis) {
        renderer.drawPixel(px + 3, py + 3, true);
      }
    }
  }
}

void DiptychActivity::drawHearts(int x, int y, bool inverse) const {
  const bool fg = !inverse;
  for (int i = 0; i < MAX_HEARTS; ++i) {
    const int hx = x + i * 14;
    const bool filled = i < hp_;
    if (filled) {
      renderer.fillRect(hx + 1, y, 2, 1, fg);
      renderer.fillRect(hx + 5, y, 2, 1, fg);
      renderer.fillRect(hx, y + 1, 8, 1, fg);
      renderer.fillRect(hx, y + 2, 8, 1, fg);
      renderer.fillRect(hx + 1, y + 3, 6, 1, fg);
      renderer.fillRect(hx + 2, y + 4, 4, 1, fg);
      renderer.fillRect(hx + 3, y + 5, 2, 1, fg);
    } else {
      renderer.fillRect(hx + 1, y, 2, 1, fg);
      renderer.fillRect(hx + 5, y, 2, 1, fg);
      renderer.drawPixel(hx, y + 1, fg);
      renderer.fillRect(hx + 3, y + 1, 2, 1, fg);
      renderer.drawPixel(hx + 7, y + 1, fg);
      renderer.drawPixel(hx, y + 2, fg);
      renderer.drawPixel(hx + 7, y + 2, fg);
      renderer.drawPixel(hx + 1, y + 3, fg);
      renderer.drawPixel(hx + 6, y + 3, fg);
      renderer.drawPixel(hx + 2, y + 4, fg);
      renderer.drawPixel(hx + 5, y + 4, fg);
      renderer.fillRect(hx + 3, y + 5, 2, 1, fg);
    }
  }
}

void DiptychActivity::drawStatus() const {
  renderer.fillRect(0, STATUS_Y, renderer.getScreenWidth(), renderer.getScreenHeight() - STATUS_Y, false);
  renderer.drawLine(VIEW_X - 8, STATUS_Y - 4, VIEW_X + VIEW + 8, STATUS_Y - 4, true);

  drawHearts(8, STATUS_Y + 6, false);

  std::string center;
  if (splitMode_ != SplitMode::None) {
    char buf[48];
    snprintf(buf, sizeof(buf), "%s split: %d/%d", splitMode_ == SplitMode::Light ? "Light" : "Shadow", splitSteps_,
             SPLIT_STEPS);
    center = buf;
  } else if (!message_.empty()) {
    center = message_;
  } else {
    char buf[48];
    if (chapter_ >= 3) {
      snprintf(buf, sizeof(buf), "Echo %d/%d", shardCountForChapter(3), SHARDS_PER_CHAPTER);
    } else if (chapter_ >= 2) {
      snprintf(buf, sizeof(buf), "Void %d/%d", shardCountForChapter(2), SHARDS_PER_CHAPTER);
    } else {
      snprintf(buf, sizeof(buf), "Shards %d/%d", shardCountForChapter(1), SHARDS_PER_CHAPTER);
    }
    center = buf;
  }
  center = renderer.truncatedText(SMALL_FONT_ID, center.c_str(), 260);
  renderer.drawCenteredText(SMALL_FONT_ID, STATUS_Y + 8, center.c_str(), true);

  char right[32];
  snprintf(right, sizeof(right), "(%d,%d)", roomX_, roomY_);
  const int rw = renderer.getTextWidth(SMALL_FONT_ID, right);
  renderer.drawText(SMALL_FONT_ID, 472 - rw, STATUS_Y + 8, right, true);
}

void DiptychActivity::drawDialogue() const {
  const int x = 32;
  const int y = 280;
  const int w = 416;
  const int h = 220;
  renderer.fillRect(x, y, w, h, false);
  renderer.drawRect(x, y, w, h, true);
  renderer.drawRect(x + 3, y + 3, w - 6, h - 6, true);
  renderer.drawText(UI_12_FONT_ID, x + 18, y + 20, "...", true, EpdFontFamily::BOLD);
  renderer.drawLine(x + 12, y + 42, x + w - 12, y + 42, true);

  if (!dialogue_.empty()) {
    const auto lines = renderer.wrappedText(UI_12_FONT_ID, dialogue_[dialogueLine_].c_str(), w - 40, 5);
    for (size_t i = 0; i < lines.size(); ++i) {
      renderer.drawText(UI_12_FONT_ID, x + 20, y + 62 + static_cast<int>(i) * 26, lines[i].c_str(), true);
    }
  }

  char idx[24];
  snprintf(idx, sizeof(idx), "%u/%u", static_cast<unsigned>(dialogueLine_ + 1),
           static_cast<unsigned>(dialogue_.size()));
  renderer.drawText(SMALL_FONT_ID, x + w - 76, y + h - 22, idx, true);
  renderer.drawText(SMALL_FONT_ID, x + w - 48, y + h - 22, "OK", true, EpdFontFamily::BOLD);
}

void DiptychActivity::drawPickup() const {
  const int x = 40;
  const int y = 310;
  const int w = 400;
  const int h = 150;
  renderer.fillRect(x, y, w, h, true);
  renderer.drawRect(x + 3, y + 3, w - 6, h - 6, false);
  drawShardMerged(480 / 2 - 12, y + 14, false);
  renderer.drawCenteredText(UI_12_FONT_ID, y + 62, "A PIECE RETURNS", false, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_10_FONT_ID, y + 92, pickupMessage_.c_str(), false);
  renderer.drawCenteredText(SMALL_FONT_ID, y + 122, "Confirm", false, EpdFontFamily::BOLD);
}

void DiptychActivity::drawCaughtModal() const {
  const int x = 40;
  const int y = 260;
  const int w = 400;
  const int h = 240;
  renderer.fillRect(x, y, w, h, true);
  renderer.drawRect(x + 3, y + 3, w - 6, h - 6, false);

  if (caughtKind_ == CaughtKind::Dead) {
    renderer.drawCenteredText(UI_12_FONT_ID, y + 30, "THE HINGE OPENS", false, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, y + 70, "You fall out of yourself.", false);
    renderer.drawCenteredText(UI_10_FONT_ID, y + 90, "The pieces return to where", false);
    renderer.drawCenteredText(UI_10_FONT_ID, y + 108, "they were kept.", false);
    renderer.drawCenteredText(SMALL_FONT_ID, y + 140, "Try again, if you are still who you were.", false);
  } else {
    drawGhoulSprite(480 / 2 - 12, y + 20, false);
    const char* line = "...";
    if (caughtKind_ == CaughtKind::Light) {
      line = GHOUL_LIGHT_LINES[steps_ % GHOUL_LIGHT_COUNT];
    } else if (caughtKind_ == CaughtKind::Shadow) {
      line = GHOUL_SHADOW_LINES[steps_ % GHOUL_SHADOW_COUNT];
    }
    const auto wrapped = renderer.wrappedText(UI_10_FONT_ID, line, w - 40, 3);
    for (size_t i = 0; i < wrapped.size(); ++i) {
      renderer.drawCenteredText(UI_10_FONT_ID, y + 70 + static_cast<int>(i) * 20, wrapped[i].c_str(), false);
    }
  }

  drawHearts(480 / 2 - MAX_HEARTS * 7, y + h - 60, true);
  renderer.drawCenteredText(SMALL_FONT_ID, y + h - 20, "Confirm", false, EpdFontFamily::BOLD);
}

void DiptychActivity::drawIntro() const {
  renderer.clearScreen();
  renderer.fillRect(0, 0, renderer.getScreenWidth(), renderer.getScreenHeight(), true);
  renderer.drawCenteredText(UI_12_FONT_ID, 260, "DIPTYCH", false, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_10_FONT_ID, 300, "/'diptik/  noun", false);
  renderer.drawCenteredText(UI_10_FONT_ID, 328, "An artwork made of two", false);
  renderer.drawCenteredText(UI_10_FONT_ID, 348, "related panels meant to be", false);
  renderer.drawCenteredText(UI_10_FONT_ID, 368, "viewed together as one piece.", false);
  drawPlayerSprite(480 / 2 - 12, 420, false);
  renderer.drawCenteredText(UI_10_FONT_ID, 500, "Confirm to continue", false, EpdFontFamily::BOLD);
}

void DiptychActivity::drawControls() const {
  renderer.clearScreen();
  renderer.fillRect(0, 0, renderer.getScreenWidth(), renderer.getScreenHeight(), true);
  renderer.drawCenteredText(UI_12_FONT_ID, 140, "CONTROLS", false, EpdFontFamily::BOLD);
  renderer.fillRect(480 / 2 - 60, 160, 120, 2, false);

  struct Row { const char* key; const char* label; };
  static const Row rows[] = {
      {"D-Pad", "walk in both worlds"},
      {"Back", "light walks alone"},
      {"OK", "speak / shadow alone"},
      {"Hold Back", "exit"},
  };
  const int startY = 220;
  const int rowH = 52;
  for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i) {
    const int y = startY + static_cast<int>(i) * rowH;
    const int kw = renderer.getTextWidth(UI_10_FONT_ID, rows[i].key);
    renderer.drawText(UI_10_FONT_ID, 480 / 2 - 20 - kw, y, rows[i].key, false, EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, 480 / 2 + 20, y, rows[i].label, false);
  }

  renderer.drawCenteredText(UI_10_FONT_ID, 700, "Confirm to begin", false, EpdFontFamily::BOLD);
}

void DiptychActivity::drawVictory() const {
  renderer.clearScreen();
  renderer.fillRect(0, 0, renderer.getScreenWidth(), renderer.getScreenHeight(), true);
  char stepsBuf[48];
  snprintf(stepsBuf, sizeof(stepsBuf), "%lu steps", static_cast<unsigned long>(steps_));
  if (chapter_ >= 3) {
    renderer.drawCenteredText(UI_12_FONT_ID, 200, "Fifteen pieces returned.", false, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, 250, "One figure stands on the hinge.", false);
    renderer.drawCenteredText(UI_10_FONT_ID, 280, "The panels close.", false);
    drawPlayerSprite(480 / 2 - 12, 340, false);
    renderer.drawCenteredText(UI_10_FONT_ID, 420, "— walk —", false);
    renderer.drawCenteredText(SMALL_FONT_ID, 480, stepsBuf, false);
    renderer.drawCenteredText(UI_10_FONT_ID, 560, "Confirm to explore", false, EpdFontFamily::BOLD);
  } else if (chapter_ == 2) {
    renderer.drawCenteredText(UI_12_FONT_ID, 200, "Two seams sewn.", false, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, 250, "A third is watching.", false);
    drawPlayerSprite(480 / 2 - 28, 330, false);
    drawPlayerSprite(480 / 2 + 4, 330, false);
    renderer.drawCenteredText(UI_12_FONT_ID, 420, "Chapter III", false, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, 450, "THE HINGE", false);
    renderer.drawCenteredText(SMALL_FONT_ID, 500, stepsBuf, false);
    renderer.drawCenteredText(UI_10_FONT_ID, 560, "Confirm to descend", false, EpdFontFamily::BOLD);
  } else {
    renderer.drawCenteredText(UI_12_FONT_ID, 200, "The surface closes.", false, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, 250, "The deep opens its eye.", false);
    drawPlayerSprite(480 / 2 - 28, 330, false);
    drawPlayerSprite(480 / 2 + 4, 330, false);
    renderer.drawCenteredText(UI_12_FONT_ID, 420, "Chapter II", false, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, 450, "THE WOUND BENEATH", false);
    renderer.drawCenteredText(SMALL_FONT_ID, 500, stepsBuf, false);
    renderer.drawCenteredText(UI_10_FONT_ID, 560, "Confirm to descend", false, EpdFontFamily::BOLD);
  }
}

void DiptychActivity::drawPlay() const {
  renderer.clearScreen();
  renderer.drawCenteredText(SMALL_FONT_ID, 6, "DIPTYCH", true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(SMALL_FONT_ID, 18, "~ Light World ~", true, EpdFontFamily::BOLD);
  drawWorld(light_, lightX_, lightY_, LIGHT_Y, false, splitMode_ == SplitMode::Shadow, computeLightDoorsOpen());
  drawSplitMeter(SplitMode::Light, LIGHT_Y, false);
  drawMinimap();

  renderer.drawLine(VIEW_X - 8, DIVIDER_Y, VIEW_X + VIEW + 8, DIVIDER_Y, true);
  drawShardRows();
  drawMirrorMeter();
  renderer.drawCenteredText(SMALL_FONT_ID, 397, "~ Shadow World ~", true, EpdFontFamily::BOLD);
  renderer.drawLine(VIEW_X - 8, 407, VIEW_X + VIEW + 8, 407, true);

  drawWorld(shadow_, shadowX_, shadowY_, SHADOW_Y, true, splitMode_ == SplitMode::Light, computeShadowDoorsOpen());
  drawSplitMeter(SplitMode::Shadow, SHADOW_Y, true);

  drawStatus();
}

void DiptychActivity::render(RenderLock&&) {
  const bool full = needsFullRefresh_;

  if (screen_ == Screen::Intro) {
    drawIntro();
  } else if (screen_ == Screen::Controls) {
    drawControls();
  } else if (screen_ == Screen::Victory) {
    drawVictory();
  } else {
    drawPlay();
    if (screen_ == Screen::Dialogue) {
      drawDialogue();
    } else if (screen_ == Screen::Pickup) {
      drawPickup();
    } else if (screen_ == Screen::Caught) {
      drawCaughtModal();
    }
  }

  renderer.displayBuffer(full ? HalDisplay::FULL_REFRESH : HalDisplay::FAST_REFRESH);
  needsFullRefresh_ = false;
}
