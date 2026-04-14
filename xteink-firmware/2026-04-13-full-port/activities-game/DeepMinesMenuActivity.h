#pragma once

#include "../Activity.h"
#include "util/ButtonNavigator.h"

class DeepMinesMenuActivity final : public Activity {
  enum class Screen { Menu, Inventory, Character };

  Screen currentScreen = Screen::Menu;
  int selectedIndex = 0;
  ButtonNavigator buttonNavigator;

  void renderMenu();
  void renderInventory();
  void renderCharacter();
  void useInventoryItem(int index);

 public:
  explicit DeepMinesMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("DeepMinesMenu", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
