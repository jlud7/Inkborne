#pragma once

#include <array>
#include <deque>

#include "../Activity.h"

class SolitaireActivity final : public Activity {
  struct Card {
    uint8_t rank = 0;
    uint8_t suit = 0;
  };

  static constexpr int TABLEAU_COUNT = 7;
  static constexpr int FOUNDATION_COUNT = 4;

  std::array<std::deque<Card>, TABLEAU_COUNT> tableau{};
  std::array<Card, FOUNDATION_COUNT> foundation{};
  std::deque<Card> stock{};
  std::deque<Card> waste{};

  int selectedColumn = 0;
  int selectionMode = 0;
  bool gameWon = false;
  bool gameLost = false;

  void startNewGame();
  void shuffleDeck(std::array<Card, 52>& deck);
  bool canMoveToTableau(const Card& card, int column) const;
  bool canMoveToFoundation(const Card& card) const;
  bool tryAutoMove();
  void updateEndState();

  static bool isRed(uint8_t suit);
  static const char* rankText(uint8_t rank);
  static const char* suitSymbol(uint8_t suit);
  void drawCard(int x, int y, int w, int h, const Card& card, bool selected, bool faceDown = false);

 public:
  explicit SolitaireActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Solitaire", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
