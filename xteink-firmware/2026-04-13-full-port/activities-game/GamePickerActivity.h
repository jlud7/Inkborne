#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class GamePickerActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectorIndex = 0;

  void openSelectedGame();

 public:
  explicit GamePickerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Games", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
