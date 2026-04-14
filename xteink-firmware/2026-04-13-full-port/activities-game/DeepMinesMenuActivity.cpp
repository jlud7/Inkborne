#include "DeepMinesMenuActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "game/GameState.h"
#include "game/GameTypes.h"

// --- Lifecycle ---

void DeepMinesMenuActivity::onEnter() {
  Activity::onEnter();
  currentScreen = Screen::Menu;
  selectedIndex = 0;
  requestUpdate();
}

// --- Input ---

void DeepMinesMenuActivity::loop() {
  using Button = MappedInputManager::Button;

  switch (currentScreen) {
    case Screen::Menu: {
      constexpr int menuSize = 6;

      buttonNavigator.onNext([this] {
        selectedIndex = ButtonNavigator::nextIndex(selectedIndex, menuSize);
        requestUpdate();
      });

      buttonNavigator.onPrevious([this] {
        selectedIndex = ButtonNavigator::previousIndex(selectedIndex, menuSize);
        requestUpdate();
      });

      if (mappedInput.wasReleased(Button::Confirm)) {
        switch (selectedIndex) {
          case 0:  // Resume
            setResult(MenuResult{.action = 0});
            finish();
            return;
          case 1:  // Inventory
            currentScreen = Screen::Inventory;
            selectedIndex = 0;
            requestUpdate();
            break;
          case 2:  // Character
            currentScreen = Screen::Character;
            selectedIndex = 0;
            requestUpdate();
            break;
          case 3:  // Save
            setResult(MenuResult{.action = 1});
            finish();
            return;
          case 4:  // Save & Quit
            setResult(MenuResult{.action = 2});
            finish();
            return;
          case 5:  // Abandon Run
            setResult(MenuResult{.action = 3});
            finish();
            return;
        }
      }

      if (mappedInput.wasReleased(Button::Back)) {
        setResult(MenuResult{.action = 0});
        finish();
        return;
      }
      break;
    }

    case Screen::Inventory: {
      int invCount = static_cast<int>(GAME_STATE.inventoryCount);
      if (invCount == 0) {
        if (mappedInput.wasReleased(Button::Back) || mappedInput.wasReleased(Button::Confirm)) {
          currentScreen = Screen::Menu;
          selectedIndex = 1;
          requestUpdate();
        }
        break;
      }

      buttonNavigator.onNext([this, invCount] {
        selectedIndex = ButtonNavigator::nextIndex(selectedIndex, invCount);
        requestUpdate();
      });

      buttonNavigator.onPrevious([this, invCount] {
        selectedIndex = ButtonNavigator::previousIndex(selectedIndex, invCount);
        requestUpdate();
      });

      if (mappedInput.wasReleased(Button::Confirm)) {
        useInventoryItem(selectedIndex);
        requestUpdate();
      }

      if (mappedInput.wasReleased(Button::Back)) {
        currentScreen = Screen::Menu;
        selectedIndex = 1;
        requestUpdate();
      }
      break;
    }

    case Screen::Character: {
      if (mappedInput.wasReleased(Button::Back) || mappedInput.wasReleased(Button::Confirm)) {
        currentScreen = Screen::Menu;
        selectedIndex = 2;
        requestUpdate();
      }
      break;
    }
  }
}

// --- Use Inventory Item ---

void DeepMinesMenuActivity::useInventoryItem(int index) {
  if (index < 0 || index >= GAME_STATE.inventoryCount) return;

  auto& item = GAME_STATE.inventory[index];
  auto& p = GAME_STATE.player;
  auto type = static_cast<game::ItemType>(item.type);

  bool consumed = false;
  char msgBuf[64];

  switch (type) {
    case game::ItemType::Potion:
      if (item.subtype == 0) {
        uint16_t heal = p.maxHp / 3;
        if (heal < 5) heal = 5;
        p.hp = std::min(static_cast<uint16_t>(p.hp + heal), p.maxHp);
        snprintf(msgBuf, sizeof(msgBuf), "You feel better! (HP +%u)", heal);
        consumed = true;
      } else if (item.subtype == 1) {
        uint16_t mana = p.maxMp / 3;
        if (mana < 3) mana = 3;
        p.mp = std::min(static_cast<uint16_t>(p.mp + mana), p.maxMp);
        snprintf(msgBuf, sizeof(msgBuf), "Magical energy flows! (MP +%u)", mana);
        consumed = true;
      } else if (item.subtype == 2) {
        p.strength += 2;
        snprintf(msgBuf, sizeof(msgBuf), "You feel stronger! (STR +2)");
        consumed = true;
      }
      break;

    case game::ItemType::Food:
      if (item.subtype == 0) {
        uint16_t heal = 5;
        p.hp = std::min(static_cast<uint16_t>(p.hp + heal), p.maxHp);
        snprintf(msgBuf, sizeof(msgBuf), "That hit the spot. (HP +%u)", heal);
        consumed = true;
      } else if (item.subtype == 1) {
        uint16_t heal = p.maxHp / 2;
        p.hp = std::min(static_cast<uint16_t>(p.hp + heal), p.maxHp);
        uint16_t mana = p.maxMp / 2;
        p.mp = std::min(static_cast<uint16_t>(p.mp + mana), p.maxMp);
        snprintf(msgBuf, sizeof(msgBuf), "The elven bread restores you!");
        consumed = true;
      }
      break;

    case game::ItemType::Scroll:
      if (item.subtype == 0) {
        for (uint8_t i = 0; i < GAME_STATE.inventoryCount; i++) {
          GAME_STATE.inventory[i].flags |= static_cast<uint8_t>(game::ItemFlag::Identified);
        }
        snprintf(msgBuf, sizeof(msgBuf), "Your items glow briefly.");
        consumed = true;
      } else if (item.subtype == 1) {
        snprintf(msgBuf, sizeof(msgBuf), "You blink! (use in dungeon)");
        consumed = true;
      } else if (item.subtype == 2) {
        snprintf(msgBuf, sizeof(msgBuf), "The level is revealed!");
        consumed = true;
      }
      break;

    default:
      if (item.flags & static_cast<uint8_t>(game::ItemFlag::Equipped)) {
        item.flags &= ~static_cast<uint8_t>(game::ItemFlag::Equipped);
        snprintf(msgBuf, sizeof(msgBuf), "Unequipped.");
      } else {
        for (uint8_t i = 0; i < GAME_STATE.inventoryCount; i++) {
          if (static_cast<int>(i) != index && GAME_STATE.inventory[i].type == item.type &&
              (GAME_STATE.inventory[i].flags & static_cast<uint8_t>(game::ItemFlag::Equipped))) {
            GAME_STATE.inventory[i].flags &= ~static_cast<uint8_t>(game::ItemFlag::Equipped);
          }
        }
        item.flags |= static_cast<uint8_t>(game::ItemFlag::Equipped);
        snprintf(msgBuf, sizeof(msgBuf), "Equipped!");
      }
      break;
  }

  GAME_STATE.addMessage(msgBuf);

  if (consumed) {
    for (int i = index; i < GAME_STATE.inventoryCount - 1; i++) {
      GAME_STATE.inventory[i] = GAME_STATE.inventory[i + 1];
    }
    GAME_STATE.inventoryCount--;
    if (selectedIndex >= GAME_STATE.inventoryCount && selectedIndex > 0) {
      selectedIndex--;
    }
  }
}

// --- Rendering ---

void DeepMinesMenuActivity::render(RenderLock&&) {
  switch (currentScreen) {
    case Screen::Menu:
      renderMenu();
      break;
    case Screen::Inventory:
      renderInventory();
      break;
    case Screen::Character:
      renderCharacter();
      break;
  }
}

void DeepMinesMenuActivity::renderMenu() {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  auto metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Deep Mines");

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  static const char* items[] = {"Resume Game", "Inventory", "Character", "Save", "Save & Quit", "Abandon Run"};

  GUI.drawButtonMenu(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, 6, selectedIndex,
      [](int index) { return std::string(items[index]); }, nullptr);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

void DeepMinesMenuActivity::renderInventory() {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  auto metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Inventory");

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  int invCount = static_cast<int>(GAME_STATE.inventoryCount);

  if (invCount == 0) {
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, "Your pack is empty.");
  } else {
    GUI.drawList(
        renderer, Rect{0, contentTop, pageWidth, contentHeight}, invCount, selectedIndex,
        [](int index) {
          const auto& item = GAME_STATE.inventory[index];
          for (int d = 0; d < game::ITEM_DEF_COUNT; d++) {
            if (game::ITEM_DEFS[d].type == item.type && game::ITEM_DEFS[d].subtype == item.subtype) {
              std::string name = game::ITEM_DEFS[d].name;
              if (item.enchantment > 0) {
                name += " +" + std::to_string(item.enchantment);
              }
              if (item.flags & static_cast<uint8_t>(game::ItemFlag::Equipped)) {
                name += " [E]";
              }
              return name;
            }
          }
          return std::string("Unknown Item");
        },
        [](int index) {
          const auto& item = GAME_STATE.inventory[index];
          auto type = static_cast<game::ItemType>(item.type);
          switch (type) {
            case game::ItemType::Weapon: return std::string("Weapon");
            case game::ItemType::Armor: return std::string("Armor");
            case game::ItemType::Shield: return std::string("Shield");
            case game::ItemType::Potion: return std::string("Potion");
            case game::ItemType::Scroll: return std::string("Scroll");
            case game::ItemType::Food: return std::string("Food");
            case game::ItemType::Ring: return std::string("Ring");
            case game::ItemType::Amulet: return std::string("Amulet");
            default: return std::string("");
          }
        },
        nullptr, nullptr);
  }

  const char* confirmLabel = invCount > 0 ? "Use/Equip" : "";
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirmLabel, tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

void DeepMinesMenuActivity::renderCharacter() {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  auto metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Character");

  const auto& p = GAME_STATE.player;
  const int x = metrics.contentSidePadding;
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing + 10;
  constexpr int lineH = 28;

  char buf[64];

  snprintf(buf, sizeof(buf), "Level: %u", p.charLevel);
  renderer.drawText(UI_10_FONT_ID, x, y, buf, true, EpdFontFamily::BOLD);
  y += lineH;

  snprintf(buf, sizeof(buf), "HP: %u / %u", p.hp, p.maxHp);
  renderer.drawText(UI_10_FONT_ID, x, y, buf);
  y += lineH;

  snprintf(buf, sizeof(buf), "MP: %u / %u", p.mp, p.maxMp);
  renderer.drawText(UI_10_FONT_ID, x, y, buf);
  y += lineH + 10;

  snprintf(buf, sizeof(buf), "Strength:     %u", p.strength);
  renderer.drawText(UI_10_FONT_ID, x, y, buf);
  y += lineH;

  snprintf(buf, sizeof(buf), "Dexterity:    %u", p.dexterity);
  renderer.drawText(UI_10_FONT_ID, x, y, buf);
  y += lineH;

  snprintf(buf, sizeof(buf), "Constitution: %u", p.constitution);
  renderer.drawText(UI_10_FONT_ID, x, y, buf);
  y += lineH;

  snprintf(buf, sizeof(buf), "Intelligence: %u", p.intelligence);
  renderer.drawText(UI_10_FONT_ID, x, y, buf);
  y += lineH + 10;

  snprintf(buf, sizeof(buf), "Experience: %lu", static_cast<unsigned long>(p.experience));
  renderer.drawText(UI_10_FONT_ID, x, y, buf);
  y += lineH;

  uint32_t nextLvlXp = game::xpForLevel(p.charLevel + 1);
  uint32_t xpRemaining = (nextLvlXp > p.experience) ? (nextLvlXp - p.experience) : 0;
  snprintf(buf, sizeof(buf), "Next level: %lu XP", static_cast<unsigned long>(xpRemaining));
  renderer.drawText(UI_10_FONT_ID, x, y, buf);
  y += lineH;

  snprintf(buf, sizeof(buf), "Gold: %u", p.gold);
  renderer.drawText(UI_10_FONT_ID, x, y, buf);
  y += lineH;

  snprintf(buf, sizeof(buf), "Dungeon depth: %u", p.dungeonDepth);
  renderer.drawText(UI_10_FONT_ID, x, y, buf);
  y += lineH;

  snprintf(buf, sizeof(buf), "Turns: %u", p.turnCount);
  renderer.drawText(UI_10_FONT_ID, x, y, buf);
  y += lineH + 10;

  renderer.drawText(UI_10_FONT_ID, x, y, "Equipment:", true, EpdFontFamily::BOLD);
  y += lineH;

  bool hasEquipped = false;
  for (uint8_t i = 0; i < GAME_STATE.inventoryCount; i++) {
    const auto& item = GAME_STATE.inventory[i];
    if (item.flags & static_cast<uint8_t>(game::ItemFlag::Equipped)) {
      for (int d = 0; d < game::ITEM_DEF_COUNT; d++) {
        if (game::ITEM_DEFS[d].type == item.type && game::ITEM_DEFS[d].subtype == item.subtype) {
          std::string name = game::ITEM_DEFS[d].name;
          if (item.enchantment > 0) {
            name += " +" + std::to_string(item.enchantment);
          }
          renderer.drawText(UI_10_FONT_ID, x + 10, y, name.c_str());
          y += lineH;
          hasEquipped = true;
          break;
        }
      }
    }
  }
  if (!hasEquipped) {
    renderer.drawText(UI_10_FONT_ID, x + 10, y, "(none)");
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
