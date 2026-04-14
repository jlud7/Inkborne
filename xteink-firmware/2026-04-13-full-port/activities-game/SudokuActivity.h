#pragma once

#include <array>

#include "../Activity.h"

class SudokuActivity final : public Activity {
  std::array<std::array<uint8_t, 9>, 9> puzzle{};
  std::array<std::array<uint8_t, 9>, 9> solution{};
  std::array<std::array<bool, 9>, 9> fixed{};

  int cursorR = 0;
  int cursorC = 0;
  bool completed = false;

  void makePuzzle();
  bool isValid(const std::array<std::array<uint8_t, 9>, 9>& b, int r, int c, int v) const;
  bool solve(std::array<std::array<uint8_t, 9>, 9>& b, int idx) const;

 public:
  explicit SudokuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Sudoku", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
