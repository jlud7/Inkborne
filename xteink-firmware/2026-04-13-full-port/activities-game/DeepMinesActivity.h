#pragma once

#include "../Activity.h"
#include "game/DungeonGenerator.h"
#include "game/GameRenderer.h"
#include "game/GameState.h"
#include "game/GameTypes.h"

class DeepMinesActivity final : public Activity {
  // Level data (~5.5KB total)
  game::Tile tiles[game::MAP_SIZE];
  uint8_t fogOfWar[game::FOG_SIZE];
  game::Monster monsters[game::MAX_MONSTERS];
  game::Item levelItems[game::MAX_ITEMS_PER_LEVEL];
  uint8_t monsterCount = 0;
  uint8_t itemCount = 0;

  // Visibility cache (computed per turn)
  bool visible[game::MAP_SIZE];

  // Rendering
  GameRenderer gameRenderer;

  // Title screen state
  bool showingTitle = true;
  bool titleRendered = false;

  // Internal methods
  void loadOrGenerateLevel();
  void saveCurrentLevel();
  void computeVisibility();
  void handleMove(int dx, int dy);
  void handleAction();
  void processMonsterTurns();
  void monsterAttackPlayer(game::Monster& monster);
  void checkLevelUp();
  void handlePlayerDeath();
  void handleVictory();
  void openGameMenu();
  void renderTitle();

 public:
  explicit DeepMinesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("DeepMines", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }
};
