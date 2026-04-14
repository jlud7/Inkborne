#include "GamePickerActivity.h"

#include <I18n.h>

#include <memory>
#include <vector>

#include "CaroActivity.h"
#include "DeepMinesActivity.h"
#include "DiptychActivity.h"
#include "MazeGameActivity.h"
#include "TetrisActivity.h"
#include "TwentyFortyEightActivity.h"
#include "components/UITheme.h"

namespace {

struct GameEntry {
  const char* name;
  const char* subtitle;
};

constexpr GameEntry kGames[] = {
    {"Diptych", "Two canvases, one soul"},
    {"Tetris", "Classic block puzzle"},
    {"2048", "Slide and merge tiles"},
    {"Gomoku", "Five in a row"},
    {"Maze", "Find the exit"},
    {"Deep Mines", "Roguelike dungeon crawler"},
};

constexpr int kGameCount = sizeof(kGames) / sizeof(kGames[0]);

}  // namespace

void GamePickerActivity::onEnter() {
  Activity::onEnter();
  selectorIndex = 0;
  requestUpdate();
}

void GamePickerActivity::openSelectedGame() {
  std::unique_ptr<Activity> game;
  switch (selectorIndex) {
    case 0:
      game = std::make_unique<DiptychActivity>(renderer, mappedInput);
      break;
    case 1:
      game = std::make_unique<TetrisActivity>(renderer, mappedInput);
      break;
    case 2:
      game = std::make_unique<TwentyFortyEightActivity>(renderer, mappedInput);
      break;
    case 3:
      game = std::make_unique<CaroActivity>(renderer, mappedInput);
      break;
    case 4:
      game = std::make_unique<MazeGameActivity>(renderer, mappedInput);
      break;
    case 5:
      game = std::make_unique<DeepMinesActivity>(renderer, mappedInput);
      break;
    default:
      return;
  }
  activityManager.pushActivity(std::move(game));
}

void GamePickerActivity::loop() {
  const int count = kGameCount;

  buttonNavigator.onNext([this, count] {
    selectorIndex = ButtonNavigator::nextIndex(selectorIndex, count);
    requestUpdate();
  });

  buttonNavigator.onPrevious([this, count] {
    selectorIndex = ButtonNavigator::previousIndex(selectorIndex, count);
    requestUpdate();
  });

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    openSelectedGame();
  }
}

void GamePickerActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Games");

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  const std::function<std::string(int)> rowTitle = [](int index) -> std::string {
    if (index >= 0 && index < kGameCount) {
      return kGames[index].name;
    }
    return {};
  };

  const std::function<std::string(int)> rowSubtitle = [](int index) -> std::string {
    if (index >= 0 && index < kGameCount) {
      return kGames[index].subtitle;
    }
    return {};
  };

  const std::function<UIIcon(int)> rowIcon = [](int index) { return index == 0 ? UIIcon::Game : UIIcon::File; };

  GUI.drawList(renderer, Rect{0, contentTop, pageWidth, contentHeight}, kGameCount, selectorIndex, rowTitle, rowSubtitle,
               rowIcon, nullptr, false);

  const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
