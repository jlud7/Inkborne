#pragma once

#include <deque>

#include "../Activity.h"

class SnakeActivity final : public Activity {
  struct Point {
    int x;
    int y;
  };

  static constexpr int COLS = 20;
  static constexpr int ROWS = 24;
  static constexpr unsigned long STEP_MS = 170;

  std::deque<Point> snake;
  Point food{0, 0};
  int dirX = 1;
  int dirY = 0;
  int nextDirX = 1;
  int nextDirY = 0;
  bool running = true;
  bool gameOver = false;
  int score = 0;
  unsigned long lastStep = 0;

  void resetGame();
  void spawnFood();
  void step();
  bool isOccupied(int x, int y) const;

 public:
  explicit SnakeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Snake", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool skipLoopDelay() override { return true; }
};
