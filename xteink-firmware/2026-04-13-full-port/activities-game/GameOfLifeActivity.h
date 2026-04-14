#pragma once

#include <array>

#include "../Activity.h"

class GameOfLifeActivity final : public Activity {
  static constexpr int W = 40;
  static constexpr int H = 28;
  std::array<std::array<uint8_t, W>, H> grid{};
  std::array<std::array<uint8_t, W>, H> next{};

  int cursorX = W / 2;
  int cursorY = H / 2;
  int generation = 0;
  bool running = false;
  unsigned long lastStep = 0;

  void stepLife();

 public:
  explicit GameOfLifeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("GameOfLife", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool skipLoopDelay() override { return true; }
};
