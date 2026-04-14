#include "TetrisActivity.h"

#include <Arduino.h>
#include <HalDisplay.h>

#include <cstring>

#include "components/UITheme.h"
#include "fontIds.h"

using Button = MappedInputManager::Button;

// ---------------------------------------------------------------------------
// Tetromino definitions
// ---------------------------------------------------------------------------
namespace {

struct PieceDef {
  int8_t cells[4][4];
  int8_t size;  // bounding box dimension
};

// clang-format off
constexpr PieceDef kPieces[7] = {
  // I
  {{{0,0,0,0},
    {1,1,1,1},
    {0,0,0,0},
    {0,0,0,0}}, 4},
  // O
  {{{1,1,0,0},
    {1,1,0,0},
    {0,0,0,0},
    {0,0,0,0}}, 2},
  // T
  {{{0,1,0,0},
    {1,1,1,0},
    {0,0,0,0},
    {0,0,0,0}}, 3},
  // S
  {{{0,1,1,0},
    {1,1,0,0},
    {0,0,0,0},
    {0,0,0,0}}, 3},
  // Z
  {{{1,1,0,0},
    {0,1,1,0},
    {0,0,0,0},
    {0,0,0,0}}, 3},
  // J
  {{{1,0,0,0},
    {1,1,1,0},
    {0,0,0,0},
    {0,0,0,0}}, 3},
  // L
  {{{0,0,1,0},
    {1,1,1,0},
    {0,0,0,0},
    {0,0,0,0}}, 3},
};
// clang-format on

void rotateCW(const int src[4][4], int n, int dst[4][4]) {
  memset(dst, 0, sizeof(int) * 16);
  for (int i = 0; i < n; i++)
    for (int j = 0; j < n; j++) dst[i][j] = src[n - j - 1][i];
}

void rotateCCW(const int src[4][4], int n, int dst[4][4]) {
  memset(dst, 0, sizeof(int) * 16);
  for (int i = 0; i < n; i++)
    for (int j = 0; j < n; j++) dst[i][j] = src[j][n - i - 1];
}

// Scoring table: points for 1, 2, 3, 4 lines cleared
constexpr int kLinePoints[4] = {100, 300, 500, 800};

}  // namespace

// ---------------------------------------------------------------------------
// Collision detection
// ---------------------------------------------------------------------------

bool TetrisActivity::fits(const int shape[4][4], int size, int px, int py) const {
  for (int r = 0; r < size; r++) {
    for (int c = 0; c < size; c++) {
      if (shape[r][c] == 0) continue;
      int bx = px + c;
      int by = py + r;
      if (bx < 0 || bx >= COLS || by >= ROWS) return false;
      if (by >= 0 && board_[by][bx] != 0) return false;
    }
  }
  return true;
}

// ---------------------------------------------------------------------------
// Lock piece into the board
// ---------------------------------------------------------------------------

void TetrisActivity::lockPiece() {
  for (int r = 0; r < curSize_; r++) {
    for (int c = 0; c < curSize_; c++) {
      if (curShape_[r][c] == 0) continue;
      int bx = curX_ + c;
      int by = curY_ + r;
      if (by >= 0 && by < ROWS && bx >= 0 && bx < COLS) {
        board_[by][bx] = curType_;
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Clear completed lines, return count
// ---------------------------------------------------------------------------

int TetrisActivity::clearFullLines() {
  int cleared = 0;
  // Scan from bottom to top
  for (int r = ROWS - 1; r >= 0; r--) {
    bool full = true;
    for (int c = 0; c < COLS; c++) {
      if (board_[r][c] == 0) {
        full = false;
        break;
      }
    }
    if (full) {
      cleared++;
      // Shift everything above down by one
      for (int rr = r; rr > 0; rr--) {
        memcpy(board_[rr], board_[rr - 1], sizeof(int) * COLS);
      }
      memset(board_[0], 0, sizeof(int) * COLS);
      r++;  // re-check this row (it now has what was above)
    }
  }
  return cleared;
}

// ---------------------------------------------------------------------------
// 7-bag randomizer
// ---------------------------------------------------------------------------

int TetrisActivity::nextFromBag() {
  if (bagIdx_ >= 7) {
    // Fill bag with piece types 1-7
    for (int i = 0; i < 7; i++) bag_[i] = i + 1;
    // Fisher-Yates shuffle
    for (int i = 6; i > 0; i--) {
      int j = random(0, i + 1);
      int tmp = bag_[i];
      bag_[i] = bag_[j];
      bag_[j] = tmp;
    }
    bagIdx_ = 0;
  }
  return bag_[bagIdx_++];
}

// ---------------------------------------------------------------------------
// Spawn a new piece at the top
// ---------------------------------------------------------------------------

void TetrisActivity::spawnPiece(int type) {
  curType_ = type;
  const auto& def = kPieces[type - 1];
  curSize_ = def.size;
  memset(curShape_, 0, sizeof(curShape_));
  for (int r = 0; r < curSize_; r++)
    for (int c = 0; c < curSize_; c++) curShape_[r][c] = def.cells[r][c];

  curX_ = (COLS - curSize_) / 2;
  curY_ = 0;
  canHold_ = true;

  if (!fits(curShape_, curSize_, curX_, curY_)) {
    gameOver_ = true;
    needsFullRefresh_ = true;
  }
}

// ---------------------------------------------------------------------------
// Ghost Y: where the piece would land
// ---------------------------------------------------------------------------

int TetrisActivity::ghostY() const {
  int gy = curY_;
  while (fits(curShape_, curSize_, curX_, gy + 1)) gy++;
  return gy;
}

// ---------------------------------------------------------------------------
// Rotation with wall kicks
// ---------------------------------------------------------------------------

bool TetrisActivity::tryRotate(bool clockwise) {
  int rotated[4][4];
  if (clockwise)
    rotateCW(curShape_, curSize_, rotated);
  else
    rotateCCW(curShape_, curSize_, rotated);

  // Try offsets: no shift, then left/right/up, then 2 left/right
  static constexpr int kicks[][2] = {{0, 0}, {-1, 0}, {1, 0}, {0, -1}, {-2, 0}, {2, 0}};
  for (const auto& k : kicks) {
    if (fits(rotated, curSize_, curX_ + k[0], curY_ + k[1])) {
      memcpy(curShape_, rotated, sizeof(curShape_));
      curX_ += k[0];
      curY_ += k[1];
      return true;
    }
  }
  return false;
}

// ---------------------------------------------------------------------------
// Hard drop
// ---------------------------------------------------------------------------

void TetrisActivity::hardDrop() {
  int gy = ghostY();
  score_ += (gy - curY_) * 2;
  curY_ = gy;
  lockPiece();
  int cleared = clearFullLines();
  if (cleared > 0) {
    lines_ += cleared;
    score_ += kLinePoints[std::min(cleared, 4) - 1] * level_;
    level_ = lines_ / 10 + 1;
    dropMs_ = std::max(400, 1500 - (level_ - 1) * 150);
  }
  spawnPiece(nextType_);
  nextType_ = nextFromBag();
  lastTick_ = millis();
}

// ---------------------------------------------------------------------------
// Hold/swap piece
// ---------------------------------------------------------------------------

void TetrisActivity::doHoldPiece() {
  if (!canHold_) return;
  canHold_ = false;
  int prev = heldType_;
  heldType_ = curType_;
  if (prev == 0) {
    spawnPiece(nextType_);
    nextType_ = nextFromBag();
  } else {
    spawnPiece(prev);
  }
}

// ---------------------------------------------------------------------------
// Initialize/reset the game
// ---------------------------------------------------------------------------

void TetrisActivity::initGame() {
  memset(board_, 0, sizeof(board_));
  score_ = 0;
  level_ = 1;
  lines_ = 0;
  gameOver_ = false;
  paused_ = false;
  dropMs_ = 1500;
  heldType_ = 0;
  bagIdx_ = 7;  // force a fresh shuffle
  needsFullRefresh_ = true;
  upConsumed_ = false;

  randomSeed(esp_random());
  nextType_ = nextFromBag();
  spawnPiece(nextFromBag());
  lastTick_ = millis();
}

// ---------------------------------------------------------------------------
// Draw a small piece preview (for next/held panels)
// ---------------------------------------------------------------------------

void TetrisActivity::drawMiniPiece(int type, int ox, int oy, int cs) const {
  if (type < 1 || type > 7) return;
  const auto& def = kPieces[type - 1];
  for (int r = 0; r < def.size; r++) {
    for (int c = 0; c < def.size; c++) {
      if (def.cells[r][c] != 0) {
        renderer.fillRect(ox + c * cs, oy + r * cs, cs - 1, cs - 1, true);
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Activity lifecycle
// ---------------------------------------------------------------------------

void TetrisActivity::onEnter() {
  Activity::onEnter();

  screenW_ = renderer.getScreenWidth();
  screenH_ = renderer.getScreenHeight();
  boardX_ = (screenW_ - COLS * CELL) / 2;
  boardY_ = 56;

  initGame();
  requestUpdate();
}

void TetrisActivity::onExit() { Activity::onExit(); }

bool TetrisActivity::preventAutoSleep() {
  return !paused_ && !gameOver_;
}

// ---------------------------------------------------------------------------
// Main loop: input handling + gravity
// ---------------------------------------------------------------------------

void TetrisActivity::loop() {
  // --- Power button: pause/unpause ---
  if (mappedInput.wasReleased(Button::Power)) {
    if (!gameOver_) {
      paused_ = !paused_;
      if (!paused_) lastTick_ = millis();  // reset gravity timer on unpause
      needsFullRefresh_ = true;  // clean refresh on pause state change
      requestUpdate();
    }
    return;
  }

  // --- Game over state ---
  if (gameOver_) {
    if (mappedInput.wasPressed(Button::Back)) {
      onGoHome();
      return;
    }
    if (mappedInput.wasAnyPressed()) {
      initGame();
      requestUpdate();
    }
    return;
  }

  // --- Paused state ---
  if (paused_) {
    // Any button unpauses (except power, handled above)
    if (mappedInput.wasPressed(Button::Back)) {
      onGoHome();
      return;
    }
    return;
  }

  // --- Active gameplay ---
  bool needsUpdate = false;

  // Button 5 (Up): hold piece (short press) / exit home (long hold)
  if (mappedInput.wasPressed(Button::Up)) {
    upConsumed_ = false;
  }
  if (mappedInput.isPressed(Button::Up) && !upConsumed_) {
    if (mappedInput.getHeldTime() > 1500) {
      upConsumed_ = true;
      onGoHome();
      return;
    }
  }
  if (mappedInput.wasReleased(Button::Up) && !upConsumed_) {
    doHoldPiece();
    needsUpdate = true;
  }

  // Button 1 (Back): move left
  if (mappedInput.wasPressed(Button::Back)) {
    if (fits(curShape_, curSize_, curX_ - 1, curY_)) {
      curX_--;
      needsUpdate = true;
    }
  }

  // Button 2 (Confirm): move right
  if (mappedInput.wasPressed(Button::Confirm)) {
    if (fits(curShape_, curSize_, curX_ + 1, curY_)) {
      curX_++;
      needsUpdate = true;
    }
  }

  // Button 3 (Left): rotate CCW
  if (mappedInput.wasPressed(Button::Left)) {
    if (tryRotate(false)) needsUpdate = true;
  }

  // Button 4 (Right): rotate CW
  if (mappedInput.wasPressed(Button::Right)) {
    if (tryRotate(true)) needsUpdate = true;
  }

  // Button 6 (Down): hard drop
  if (mappedInput.wasPressed(Button::Down)) {
    hardDrop();
    needsUpdate = true;
  }

  // --- Gravity ---
  unsigned long now = millis();
  if (now - lastTick_ >= static_cast<unsigned long>(dropMs_)) {
    lastTick_ = now;
    if (fits(curShape_, curSize_, curX_, curY_ + 1)) {
      curY_++;
    } else {
      // Piece has landed
      lockPiece();
      int cleared = clearFullLines();
      if (cleared > 0) {
        lines_ += cleared;
        score_ += kLinePoints[std::min(cleared, 4) - 1] * level_;
        level_ = lines_ / 10 + 1;
        dropMs_ = std::max(400, 1500 - (level_ - 1) * 150);
      }
      spawnPiece(nextType_);
      nextType_ = nextFromBag();
    }
    needsUpdate = true;
  }

  if (needsUpdate) requestUpdate();
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

void TetrisActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const int bx = boardX_;
  const int by = boardY_;
  const int bw = COLS * CELL;
  const int bh = ROWS * CELL;

  // --- Header: title and score ---
  renderer.drawText(UI_12_FONT_ID, bx, 8, "TETRIS", true, EpdFontFamily::BOLD);

  char buf[32];
  snprintf(buf, sizeof(buf), "%d", score_);
  int scoreW = renderer.getTextWidth(UI_12_FONT_ID, buf, EpdFontFamily::BOLD);
  renderer.drawText(UI_12_FONT_ID, bx + bw - scoreW, 8, buf, true, EpdFontFamily::BOLD);

  // Level and lines
  snprintf(buf, sizeof(buf), "Lv %d  Ln %d", level_, lines_);
  renderer.drawText(UI_10_FONT_ID, bx, 30, buf, true);

  // --- Board border (double line) ---
  renderer.drawRect(bx - 3, by - 3, bw + 6, bh + 6, true);
  renderer.drawRect(bx - 2, by - 2, bw + 4, bh + 4, true);

  // --- Grid lines ---
  for (int r = 0; r <= ROWS; r++) {
    renderer.drawLine(bx, by + r * CELL, bx + bw, by + r * CELL, true);
  }
  for (int c = 0; c <= COLS; c++) {
    renderer.drawLine(bx + c * CELL, by, bx + c * CELL, by + bh, true);
  }

  // --- Locked cells ---
  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      if (board_[r][c] != 0) {
        renderer.fillRect(bx + c * CELL + 1, by + r * CELL + 1, CELL - 1, CELL - 1, true);
      }
    }
  }

  if (!gameOver_ && !paused_) {
    // --- Ghost piece (landing preview, drawn as small inner outline) ---
    int gy = ghostY();
    for (int r = 0; r < curSize_; r++) {
      for (int c = 0; c < curSize_; c++) {
        if (curShape_[r][c] == 0) continue;
        int gr = gy + r;
        int gc = curX_ + c;
        if (gr >= 0 && gr < ROWS && gc >= 0 && gc < COLS) {
          int px = bx + gc * CELL + 4;
          int py = by + gr * CELL + 4;
          renderer.drawRect(px, py, CELL - 7, CELL - 7, true);
        }
      }
    }

    // --- Active piece (solid black) ---
    for (int r = 0; r < curSize_; r++) {
      for (int c = 0; c < curSize_; c++) {
        if (curShape_[r][c] == 0) continue;
        int br = curY_ + r;
        int bc = curX_ + c;
        if (br >= 0 && br < ROWS && bc >= 0 && bc < COLS) {
          renderer.fillRect(bx + bc * CELL + 1, by + br * CELL + 1, CELL - 1, CELL - 1, true);
        }
      }
    }
  }

  // --- Side panel: NEXT piece ---
  {
    int nx = bx + bw + 8;
    renderer.drawText(SMALL_FONT_ID, nx, by + 2, "NEXT", true);
    drawMiniPiece(nextType_, nx + 2, by + 20, 12);
  }

  // --- Side panel: HELD piece ---
  {
    int hx = bx - 58;
    if (hx < 4) hx = 4;
    renderer.drawText(SMALL_FONT_ID, hx, by + 2, "HOLD", true);
    drawMiniPiece(heldType_, hx + 2, by + 20, 12);
  }

  // --- Pause overlay ---
  if (paused_) {
    int ow = bw - 24;
    int oh = 48;
    int ox = bx + 12;
    int oy = by + bh / 2 - 24;
    renderer.fillRect(ox, oy, ow, oh, false);  // white background
    renderer.drawRect(ox, oy, ow, oh, 2, true);
    renderer.drawCenteredText(UI_12_FONT_ID, oy + 14, "PAUSED", true, EpdFontFamily::BOLD);
  }

  // --- Game over overlay ---
  if (gameOver_) {
    int ow = bw - 20;
    int oh = 80;
    int ox = bx + 10;
    int oy = by + bh / 2 - 40;
    renderer.fillRect(ox, oy, ow, oh, false);  // white background
    renderer.drawRect(ox, oy, ow, oh, 2, true);
    renderer.drawCenteredText(UI_12_FONT_ID, oy + 10, "GAME OVER", true, EpdFontFamily::BOLD);

    snprintf(buf, sizeof(buf), "Score: %d", score_);
    renderer.drawCenteredText(UI_10_FONT_ID, oy + 34, buf, true);

    snprintf(buf, sizeof(buf), "Lv %d / %d lines", level_, lines_);
    renderer.drawCenteredText(UI_10_FONT_ID, oy + 52, buf, true);
  }

  // --- Button hints ---
  auto labels = mappedInput.mapLabels("<", ">", "RotL", "RotR");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  GUI.drawSideButtonHints(renderer, "Hold", "Drop");

  // --- Refresh display ---
  // Full refresh only on state transitions (pause/unpause/gameover/start) to clear ghosting.
  // During active gameplay, always fast refresh for a smooth experience.
  if (needsFullRefresh_) {
    needsFullRefresh_ = false;
    renderer.displayBuffer(HalDisplay::FULL_REFRESH);
  } else {
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  }
}
