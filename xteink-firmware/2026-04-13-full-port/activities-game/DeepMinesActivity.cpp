#include "DeepMinesActivity.h"

#include <HalDisplay.h>

#include <algorithm>
#include <cstring>

#include "DeepMinesMenuActivity.h"
#include "MappedInputManager.h"
#include "fontIds.h"
#include "game/GameSave.h"

// --- FOV ---

namespace {
constexpr int FOV_RADIUS = 8;

bool hasLineOfSight(const game::Tile* tiles, int x0, int y0, int x1, int y1) {
  int dx = abs(x1 - x0);
  int dy = -abs(y1 - y0);
  int sx = x0 < x1 ? 1 : -1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;

  int cx = x0, cy = y0;
  while (true) {
    if ((cx != x0 || cy != y0) && (cx != x1 || cy != y1)) {
      if (tiles[cy * game::MAP_WIDTH + cx] == game::Tile::Wall) {
        return false;
      }
    }
    if (cx == x1 && cy == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      cx += sx;
    }
    if (e2 <= dx) {
      err += dx;
      cy += sy;
    }
  }
  return true;
}

int equippedAttackBonus() {
  int bonus = 0;
  for (uint8_t i = 0; i < GAME_STATE.inventoryCount; i++) {
    const auto& item = GAME_STATE.inventory[i];
    if (item.flags & static_cast<uint8_t>(game::ItemFlag::Equipped)) {
      for (int d = 0; d < game::ITEM_DEF_COUNT; d++) {
        if (game::ITEM_DEFS[d].type == item.type && game::ITEM_DEFS[d].subtype == item.subtype) {
          bonus += game::ITEM_DEFS[d].attack + item.enchantment;
          break;
        }
      }
    }
  }
  return bonus;
}

int equippedDefenseBonus() {
  int bonus = 0;
  for (uint8_t i = 0; i < GAME_STATE.inventoryCount; i++) {
    const auto& item = GAME_STATE.inventory[i];
    if (item.flags & static_cast<uint8_t>(game::ItemFlag::Equipped)) {
      for (int d = 0; d < game::ITEM_DEF_COUNT; d++) {
        if (game::ITEM_DEFS[d].type == item.type && game::ITEM_DEFS[d].subtype == item.subtype) {
          bonus += game::ITEM_DEFS[d].defense + item.enchantment;
          break;
        }
      }
    }
  }
  return bonus;
}

bool isWalkable(game::Tile tile) {
  return tile == game::Tile::Floor || tile == game::Tile::DoorOpen || tile == game::Tile::StairsUp ||
         tile == game::Tile::StairsDown || tile == game::Tile::Rubble;
}

// --- Title screen block letters ---
constexpr uint8_t GLYPH_D[] = {0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110};
constexpr uint8_t GLYPH_E[] = {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111};
constexpr uint8_t GLYPH_P[] = {0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000};
constexpr uint8_t GLYPH_M[] = {0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001};
constexpr uint8_t GLYPH_I[] = {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b11111};
constexpr uint8_t GLYPH_N[] = {0b10001, 0b11001, 0b10101, 0b10101, 0b10011, 0b10001, 0b10001};
constexpr uint8_t GLYPH_S[] = {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110};

constexpr int GLYPH_W = 5;
constexpr int GLYPH_H = 7;

void drawBlockLetter(GfxRenderer& r, const uint8_t* glyph, int ox, int oy, int scale) {
  for (int row = 0; row < GLYPH_H; row++) {
    for (int col = 0; col < GLYPH_W; col++) {
      if (glyph[row] & (1 << (GLYPH_W - 1 - col))) {
        r.fillRect(ox + col * scale, oy + row * scale, scale - 1, scale - 1);
      }
    }
  }
}

struct LetterEntry { const uint8_t* glyph; };

void drawWord(GfxRenderer& r, const LetterEntry* letters, int count, int centerX, int originY, int scale) {
  int letterW = GLYPH_W * scale + scale;
  int totalW = count * letterW - scale;
  int startX = centerX - totalW / 2;
  for (int i = 0; i < count; i++) {
    drawBlockLetter(r, letters[i].glyph, startX + i * letterW, originY, scale);
  }
}

}  // namespace

// --- Lifecycle ---

void DeepMinesActivity::onEnter() {
  Activity::onEnter();

  showingTitle = true;
  titleRendered = false;
  requestUpdate();
}

void DeepMinesActivity::renderTitle() {
  const auto pageWidth = renderer.getScreenWidth();
  const int centerX = pageWidth / 2;

  renderer.clearScreen();
  renderer.drawRect(10, 10, pageWidth - 20, 480);
  renderer.drawRect(13, 13, pageWidth - 26, 474);

  renderer.drawCenteredText(UI_10_FONT_ID, 40, "A roguelike for the CrossPoint Reader");
  renderer.drawLine(40, 70, pageWidth - 40, 70);

  constexpr int SCALE = 8;
  const LetterEntry deep[] = {{GLYPH_D}, {GLYPH_E}, {GLYPH_E}, {GLYPH_P}};
  drawWord(renderer, deep, 4, centerX, 90, SCALE);

  const LetterEntry mines[] = {{GLYPH_M}, {GLYPH_I}, {GLYPH_N}, {GLYPH_E}, {GLYPH_S}};
  drawWord(renderer, mines, 5, centerX, 160, SCALE);

  renderer.drawLine(40, 230, pageWidth - 40, 230);
  renderer.drawCenteredText(UI_10_FONT_ID, 250, "D E E P    M I N E S", true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_10_FONT_ID, 290, "--- * ---");
  renderer.drawCenteredText(SMALL_FONT_ID, 400, "Inspired by Moria & Angband");
  renderer.drawLine(40, 460, pageWidth - 40, 460);
  renderer.drawCenteredText(SMALL_FONT_ID, 500, "Descend 26 levels. Defeat the Necromancer.");
  renderer.drawCenteredText(SMALL_FONT_ID, 525, "Claim the Ring of Power.");
  renderer.drawCenteredText(UI_10_FONT_ID, 740, "[ press any key to continue ]");

  renderer.displayBuffer();
  titleRendered = true;
}

// --- Input ---

void DeepMinesActivity::loop() {
  // Title screen
  if (showingTitle) {
    if (titleRendered && mappedInput.wasAnyReleased()) {
      showingTitle = false;

      // Initialize game
      gameRenderer.init(renderer);
      if (GAME_STATE.hasSaveFile()) {
        GAME_STATE.loadFromFile();
      } else {
        GAME_STATE.newGame(esp_random());
      }
      loadOrGenerateLevel();
      computeVisibility();
      requestUpdate();
    }
    return;
  }

  using Button = MappedInputManager::Button;

  if (mappedInput.wasReleased(Button::Up)) {
    handleMove(0, -1);
  } else if (mappedInput.wasReleased(Button::Down)) {
    handleMove(0, 1);
  } else if (mappedInput.wasReleased(Button::Left)) {
    handleMove(-1, 0);
  } else if (mappedInput.wasReleased(Button::Right)) {
    handleMove(1, 0);
  } else if (mappedInput.wasReleased(Button::Confirm)) {
    handleAction();
  } else if (mappedInput.wasReleased(Button::Back)) {
    openGameMenu();
  }
}

// --- Render ---

void DeepMinesActivity::render(RenderLock&&) {
  if (showingTitle) {
    renderTitle();
    return;
  }
  gameRenderer.draw(renderer, tiles, fogOfWar, monsters, monsterCount, levelItems, itemCount, visible);
}

// --- Game Menu ---

void DeepMinesActivity::openGameMenu() {
  auto menuActivity = std::make_unique<DeepMinesMenuActivity>(renderer, mappedInput);

  startActivityForResult(std::move(menuActivity), [this](const ActivityResult& result) {
    int action = 0;  // 0=resume, 1=save, 2=save+quit, 3=abandon
    if (auto* menu = std::get_if<MenuResult>(&result.data)) {
      action = menu->action;
    }

    if (action == 1) {
      // Save and resume
      saveCurrentLevel();
      GAME_STATE.saveToFile();
      GAME_STATE.addMessage("Game saved.");
      requestUpdate();
    } else if (action == 2) {
      // Save and quit
      saveCurrentLevel();
      GAME_STATE.saveToFile();
      finish();
    } else if (action == 3) {
      // Abandon
      GameSave::deleteAll();
      GAME_STATE.deleteSaveFile();
      finish();
    } else {
      // Resume
      requestUpdate();
    }
  });
}

// --- Movement ---

void DeepMinesActivity::handleMove(int dx, int dy) {
  auto& p = GAME_STATE.player;
  if (p.hp == 0) return;

  int newX = p.x + dx;
  int newY = p.y + dy;

  if (newX < 0 || newX >= game::MAP_WIDTH || newY < 0 || newY >= game::MAP_HEIGHT) return;

  game::Tile target = tiles[newY * game::MAP_WIDTH + newX];

  if (target == game::Tile::Wall) return;
  if (target == game::Tile::DoorClosed) {
    tiles[newY * game::MAP_WIDTH + newX] = game::Tile::DoorOpen;
    GAME_STATE.addMessage("You open the door.");
    p.turnCount++;
    processMonsterTurns();
    computeVisibility();
    requestUpdate();
    return;
  }

  // Check for monster at target
  for (uint8_t i = 0; i < monsterCount; i++) {
    if (monsters[i].x == newX && monsters[i].y == newY && monsters[i].hp > 0) {
      const auto& def = game::MONSTER_DEFS[monsters[i].type];
      int atkPower = static_cast<int>(p.strength) + equippedAttackBonus();
      int damage = std::max(1, atkPower - static_cast<int>(def.defense));
      game::Rng rng(p.turnCount ^ (p.x * 31 + p.y * 37));
      damage = std::max(1, damage + rng.nextRangeInclusive(-damage / 4, damage / 4));

      monsters[i].hp = (damage >= monsters[i].hp) ? 0 : monsters[i].hp - damage;

      char msgBuf[64];
      if (monsters[i].hp == 0) {
        snprintf(msgBuf, sizeof(msgBuf), "You slay the %s! (+%uXP)", def.name, def.expValue);
        GAME_STATE.addMessage(msgBuf);
        p.experience += def.expValue;
        checkLevelUp();

        if (monsters[i].type == game::BOSS_MONSTER_TYPE && itemCount < game::MAX_ITEMS_PER_LEVEL) {
          auto& ring = levelItems[itemCount];
          ring.x = monsters[i].x;
          ring.y = monsters[i].y;
          ring.type = game::ITEM_DEFS[game::RING_OF_POWER_DEF].type;
          ring.subtype = game::ITEM_DEFS[game::RING_OF_POWER_DEF].subtype;
          ring.count = 1;
          ring.enchantment = 0;
          ring.flags = 0;
          itemCount++;
          GAME_STATE.addMessage("Something glints on the ground...");
        }
      } else {
        snprintf(msgBuf, sizeof(msgBuf), "You hit the %s for %d.", def.name, damage);
        GAME_STATE.addMessage(msgBuf);
      }

      monsters[i].state = static_cast<uint8_t>(game::MonsterState::Hostile);

      p.turnCount++;
      processMonsterTurns();
      computeVisibility();
      requestUpdate();
      return;
    }
  }

  // Move player
  p.x = static_cast<int16_t>(newX);
  p.y = static_cast<int16_t>(newY);
  p.turnCount++;

  processMonsterTurns();
  computeVisibility();
  requestUpdate();
}

// --- Action (Confirm button) ---

void DeepMinesActivity::handleAction() {
  auto& p = GAME_STATE.player;

  if (p.hp == 0) {
    handlePlayerDeath();
    return;
  }

  int mapIdx = p.y * game::MAP_WIDTH + p.x;
  game::Tile here = tiles[mapIdx];

  if (here == game::Tile::StairsDown) {
    if (p.dungeonDepth >= game::MAX_DEPTH) {
      GAME_STATE.addMessage("The stairs are blocked by rubble.");
      requestUpdate();
      return;
    }
    saveCurrentLevel();
    p.dungeonDepth++;
    GAME_STATE.addMessage("You descend deeper...");
    loadOrGenerateLevel();
    for (int i = 0; i < game::MAP_SIZE; i++) {
      if (tiles[i] == game::Tile::StairsUp) {
        p.x = static_cast<int16_t>(i % game::MAP_WIDTH);
        p.y = static_cast<int16_t>(i / game::MAP_WIDTH);
        break;
      }
    }
    computeVisibility();
    requestUpdate();
    return;
  }

  if (here == game::Tile::StairsUp) {
    if (p.dungeonDepth <= 1) {
      GAME_STATE.addMessage("You see daylight above... but the mines call.");
      requestUpdate();
      return;
    }
    saveCurrentLevel();
    p.dungeonDepth--;
    GAME_STATE.addMessage("You ascend...");
    loadOrGenerateLevel();
    for (int i = 0; i < game::MAP_SIZE; i++) {
      if (tiles[i] == game::Tile::StairsDown) {
        p.x = static_cast<int16_t>(i % game::MAP_WIDTH);
        p.y = static_cast<int16_t>(i / game::MAP_WIDTH);
        break;
      }
    }
    computeVisibility();
    requestUpdate();
    return;
  }

  // Pick up items
  for (uint8_t i = 0; i < itemCount; i++) {
    if (levelItems[i].x == p.x && levelItems[i].y == p.y) {
      const auto& ringDef = game::ITEM_DEFS[game::RING_OF_POWER_DEF];
      if (levelItems[i].type == ringDef.type && levelItems[i].subtype == ringDef.subtype) {
        GAME_STATE.addMessage("You claim the Ring of Power!");
        GAME_STATE.addMessage("The mines tremble... You have won!");
        handleVictory();
        return;
      }

      if (static_cast<game::ItemType>(levelItems[i].type) == game::ItemType::Gold) {
        uint16_t amount = levelItems[i].count * 10;
        p.gold += amount;
        char msgBuf[48];
        snprintf(msgBuf, sizeof(msgBuf), "You pick up %u gold.", amount);
        GAME_STATE.addMessage(msgBuf);
      } else {
        if (GAME_STATE.inventoryCount >= game::MAX_INVENTORY) {
          GAME_STATE.addMessage("Your pack is full!");
          requestUpdate();
          return;
        }
        GAME_STATE.inventory[GAME_STATE.inventoryCount] = levelItems[i];
        GAME_STATE.inventory[GAME_STATE.inventoryCount].x = -1;
        GAME_STATE.inventory[GAME_STATE.inventoryCount].y = -1;
        GAME_STATE.inventoryCount++;

        char msgBuf[64];
        for (int d = 0; d < game::ITEM_DEF_COUNT; d++) {
          if (game::ITEM_DEFS[d].type == levelItems[i].type && game::ITEM_DEFS[d].subtype == levelItems[i].subtype) {
            snprintf(msgBuf, sizeof(msgBuf), "You pick up %s.", game::ITEM_DEFS[d].name);
            GAME_STATE.addMessage(msgBuf);
            break;
          }
        }
      }

      levelItems[i] = levelItems[itemCount - 1];
      itemCount--;

      p.turnCount++;
      processMonsterTurns();
      requestUpdate();
      return;
    }
  }

  GAME_STATE.addMessage("Nothing to do here.");
  requestUpdate();
}

// --- Monster AI ---

void DeepMinesActivity::processMonsterTurns() {
  auto& p = GAME_STATE.player;
  if (p.hp == 0) return;

  for (uint8_t i = 0; i < monsterCount; i++) {
    auto& m = monsters[i];
    if (m.hp == 0) continue;

    int dx = static_cast<int>(p.x) - m.x;
    int dy = static_cast<int>(p.y) - m.y;
    int dist2 = dx * dx + dy * dy;

    auto state = static_cast<game::MonsterState>(m.state);

    if (state == game::MonsterState::Asleep) {
      if (dist2 <= FOV_RADIUS * FOV_RADIUS && visible[m.y * game::MAP_WIDTH + m.x]) {
        game::Rng rng(p.turnCount ^ (m.x * 17 + m.y * 13 + i));
        int wakeChance = 80 - dist2;
        if (static_cast<int>(rng.nextRange(100)) < wakeChance) {
          m.state = static_cast<uint8_t>(game::MonsterState::Hostile);
          state = game::MonsterState::Hostile;
        }
      }
      continue;
    }

    if (state == game::MonsterState::Wandering) {
      if (dist2 <= FOV_RADIUS * FOV_RADIUS && visible[m.y * game::MAP_WIDTH + m.x]) {
        m.state = static_cast<uint8_t>(game::MonsterState::Hostile);
        state = game::MonsterState::Hostile;
      } else {
        game::Rng rng(p.turnCount ^ (m.x * 23 + m.y * 29 + i * 7));
        int dir = rng.nextRange(4);
        int wmx = m.x + ((dir == 0) ? 1 : (dir == 1) ? -1 : 0);
        int wmy = m.y + ((dir == 2) ? 1 : (dir == 3) ? -1 : 0);

        if (wmx >= 0 && wmx < game::MAP_WIDTH && wmy >= 0 && wmy < game::MAP_HEIGHT) {
          if (isWalkable(tiles[wmy * game::MAP_WIDTH + wmx])) {
            bool blocked = (wmx == p.x && wmy == p.y);
            if (!blocked) {
              for (uint8_t j = 0; j < monsterCount && !blocked; j++) {
                if (j != i && monsters[j].hp > 0 && monsters[j].x == wmx && monsters[j].y == wmy) {
                  blocked = true;
                }
              }
            }
            if (!blocked) {
              m.x = static_cast<int16_t>(wmx);
              m.y = static_cast<int16_t>(wmy);
            }
          }
        }
        continue;
      }
    }

    if (state == game::MonsterState::Hostile) {
      if (abs(dx) <= 1 && abs(dy) <= 1 && dist2 <= 2) {
        monsterAttackPlayer(m);
        if (p.hp == 0) return;
        continue;
      }

      int bestX = m.x;
      int bestY = m.y;
      int bestDist = dist2;

      static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
      for (const auto& d : dirs) {
        int nx = m.x + d[0];
        int ny = m.y + d[1];

        if (nx < 0 || nx >= game::MAP_WIDTH || ny < 0 || ny >= game::MAP_HEIGHT) continue;
        if (!isWalkable(tiles[ny * game::MAP_WIDTH + nx])) continue;
        if (nx == p.x && ny == p.y) continue;

        bool occupied = false;
        for (uint8_t j = 0; j < monsterCount; j++) {
          if (j != i && monsters[j].hp > 0 && monsters[j].x == nx && monsters[j].y == ny) {
            occupied = true;
            break;
          }
        }
        if (occupied) continue;

        int ndx = static_cast<int>(p.x) - nx;
        int ndy = static_cast<int>(p.y) - ny;
        int nd = ndx * ndx + ndy * ndy;
        if (nd < bestDist) {
          bestDist = nd;
          bestX = nx;
          bestY = ny;
        }
      }

      m.x = static_cast<int16_t>(bestX);
      m.y = static_cast<int16_t>(bestY);
    }
  }

  int regenRate = std::max(5, 20 - static_cast<int>(p.constitution) / 2);
  if (p.hp > 0 && p.hp < p.maxHp && p.turnCount % regenRate == 0) {
    p.hp++;
  }
  if (p.mp < p.maxMp && p.turnCount % (regenRate + 5) == 0) {
    p.mp++;
  }
}

void DeepMinesActivity::monsterAttackPlayer(game::Monster& m) {
  auto& p = GAME_STATE.player;
  const auto& def = game::MONSTER_DEFS[m.type];

  int playerDef = static_cast<int>(p.dexterity / 3) + equippedDefenseBonus();
  int damage = std::max(1, static_cast<int>(def.attack) - playerDef);
  game::Rng rng(p.turnCount ^ (m.x * 41 + m.y * 43));
  damage = std::max(1, damage + rng.nextRangeInclusive(-damage / 4, damage / 4));

  p.hp = (static_cast<uint16_t>(damage) >= p.hp) ? 0 : p.hp - static_cast<uint16_t>(damage);

  char msgBuf[64];
  if (p.hp == 0) {
    snprintf(msgBuf, sizeof(msgBuf), "The %s kills you!", def.name);
  } else {
    snprintf(msgBuf, sizeof(msgBuf), "The %s hits you for %d.", def.name, damage);
  }
  GAME_STATE.addMessage(msgBuf);
}

// --- Level Up ---

void DeepMinesActivity::checkLevelUp() {
  auto& p = GAME_STATE.player;

  while (p.charLevel < 50 && p.experience >= game::xpForLevel(p.charLevel + 1)) {
    p.charLevel++;

    uint16_t hpGain = 4 + p.constitution / 4;
    uint16_t mpGain = 2 + p.intelligence / 5;
    p.maxHp += hpGain;
    p.maxMp += mpGain;
    p.hp = p.maxHp;
    p.mp = p.maxMp;
    p.strength += 1;
    p.dexterity += 1;

    char msgBuf[48];
    snprintf(msgBuf, sizeof(msgBuf), "Welcome to level %u!", p.charLevel);
    GAME_STATE.addMessage(msgBuf);
  }
}

// --- Player Death ---

void DeepMinesActivity::handlePlayerDeath() {
  GAME_STATE.addMessage("You have perished...");
  requestUpdate();

  GameSave::deleteAll();
  GAME_STATE.deleteSaveFile();

  finish();
}

// --- Victory ---

void DeepMinesActivity::handleVictory() {
  auto& p = GAME_STATE.player;

  char msgBuf[64];
  snprintf(msgBuf, sizeof(msgBuf), "Victory! Depth %u, Level %u, %u turns.", p.dungeonDepth, p.charLevel, p.turnCount);
  GAME_STATE.addMessage(msgBuf);
  requestUpdate();

  GameSave::deleteAll();
  GAME_STATE.deleteSaveFile();

  finish();
}

// --- Level Management ---

void DeepMinesActivity::loadOrGenerateLevel() {
  auto& p = GAME_STATE.player;

  auto result = DungeonGenerator::generate(p.gameSeed, p.dungeonDepth, tiles, monsters, levelItems);
  monsterCount = result.monsterCount;
  itemCount = result.itemCount;

  memset(fogOfWar, 0, sizeof(fogOfWar));
  memset(visible, 0, sizeof(visible));

  if (GameSave::hasLevel(p.dungeonDepth)) {
    GameSave::loadLevel(p.dungeonDepth, fogOfWar, monsters, monsterCount, levelItems, itemCount);
  } else {
    p.x = result.stairsUpX;
    p.y = result.stairsUpY;
  }
}

void DeepMinesActivity::saveCurrentLevel() {
  GameSave::saveLevel(GAME_STATE.player.dungeonDepth, fogOfWar, monsters, monsterCount, levelItems, itemCount);
}

// --- Visibility ---

void DeepMinesActivity::computeVisibility() {
  auto& p = GAME_STATE.player;

  memset(visible, 0, sizeof(visible));

  int startX = std::max(0, static_cast<int>(p.x) - FOV_RADIUS);
  int endX = std::min(game::MAP_WIDTH - 1, static_cast<int>(p.x) + FOV_RADIUS);
  int startY = std::max(0, static_cast<int>(p.y) - FOV_RADIUS);
  int endY = std::min(game::MAP_HEIGHT - 1, static_cast<int>(p.y) + FOV_RADIUS);

  for (int y = startY; y <= endY; y++) {
    for (int x = startX; x <= endX; x++) {
      int dx = x - p.x;
      int dy = y - p.y;
      if (dx * dx + dy * dy > FOV_RADIUS * FOV_RADIUS) continue;

      if (hasLineOfSight(tiles, p.x, p.y, x, y)) {
        visible[y * game::MAP_WIDTH + x] = true;
        game::fogSetExplored(fogOfWar, x, y);
      }
    }
  }
}
