#pragma once

#include "../Activity.h"

class TetrisActivity final : public Activity {
 public:
  explicit TetrisActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Tetris", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override;

 private:
  static constexpr int COLS = 10;
  static constexpr int ROWS = 20;
  static constexpr int CELL = 32;

  // Layout (computed in onEnter)
  int boardX_ = 0;
  int boardY_ = 0;
  int screenW_ = 0;
  int screenH_ = 0;

  // Board: 0 = empty, 1-7 = locked piece type
  int board_[ROWS][COLS] = {};

  // Scoring
  int score_ = 0;
  int level_ = 1;
  int lines_ = 0;

  // State
  bool gameOver_ = false;
  bool paused_ = false;
  unsigned long lastTick_ = 0;
  int dropMs_ = 1500;
  bool needsFullRefresh_ = true;  // full refresh on pause/unpause/gameover, not during play

  // Current piece (shape stored as NxN grid in top-left of 4x4 array)
  int curShape_[4][4] = {};
  int curSize_ = 0;  // bounding box: 2 (O), 3 (T/S/Z/J/L), or 4 (I)
  int curType_ = 0;  // piece type 1-7
  int curX_ = 0;     // board column of shape[0][0]
  int curY_ = 0;     // board row of shape[0][0]

  // Next and held pieces
  int nextType_ = 0;
  int heldType_ = 0;
  bool canHold_ = true;

  // 7-bag randomizer
  int bag_[7] = {};
  int bagIdx_ = 7;  // starts past end to trigger first shuffle

  // Long-press tracking for hold/exit button
  bool upConsumed_ = false;

  // Game logic
  bool fits(const int shape[4][4], int size, int px, int py) const;
  void lockPiece();
  int clearFullLines();
  int nextFromBag();
  void spawnPiece(int type);
  int ghostY() const;
  bool tryRotate(bool clockwise);
  void hardDrop();
  void doHoldPiece();
  void initGame();

  // Rendering helpers
  void drawBoard() const;
  void drawMiniPiece(int type, int ox, int oy, int cellSize) const;
};
