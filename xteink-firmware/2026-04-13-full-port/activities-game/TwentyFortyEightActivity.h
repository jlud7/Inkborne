#pragma once

#include <array>

#include "../Activity.h"

class TwentyFortyEightActivity final : public Activity {
  std::array<std::array<int, 4>, 4> board{};
  int score = 0;
  bool won = false;
  bool gameOver = false;

  bool slideLeft();
  void rotateClockwise();
  bool applyMove(int turnsToLeft);
  void addRandomTile();
  bool hasMoves() const;

 public:
  explicit TwentyFortyEightActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("2048", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
