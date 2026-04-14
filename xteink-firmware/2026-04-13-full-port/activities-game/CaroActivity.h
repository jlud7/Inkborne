#pragma once

#include <array>

#include "../Activity.h"

class CaroActivity final : public Activity {
  static constexpr int SIZE = 13;
  std::array<std::array<uint8_t, SIZE>, SIZE> grid{};
  int cursorR = SIZE / 2;
  int cursorC = SIZE / 2;
  uint8_t winner = 0;
  bool draw = false;

  bool hasFive(int r, int c, uint8_t v) const;
  int scorePos(int r, int c, uint8_t me, uint8_t opp) const;
  void aiMove();

 public:
  explicit CaroActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Gomoku", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
