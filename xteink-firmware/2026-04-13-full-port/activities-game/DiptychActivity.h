#pragma once

#include <array>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

#include "../Activity.h"

class DiptychActivity final : public Activity {
 public:
  explicit DiptychActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Diptych", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override;

 private:
  static constexpr int GRID = 15;
  static constexpr int TILE = 24;
  static constexpr int SHARDS_PER_CHAPTER = 5;
  static constexpr int TOTAL_CHAPTERS = 3;
  static constexpr int SPLIT_STEPS = 5;
  static constexpr int MIRROR_STEPS = 5;
  static constexpr int MAX_ENTITIES = 8;
  static constexpr int MAX_HEARTS = 5;
  static constexpr int WORLD_RADIUS = 4;
  static constexpr int WORLD_SIZE = 9;  // 2*RADIUS+1

  enum class Screen : uint8_t {
    Intro,
    Controls,
    Play,
    Dialogue,
    Pickup,
    Victory,
    Caught,
  };

  enum class Tile : uint8_t {
    Empty,
    Wall,
    Tree,
    Water,
    Door,
    Switch,
    Mirror,
  };

  enum class EntityType : uint8_t {
    None,
    Npc,
    Sign,
    HalfLight,
    HalfShadow,
    Ghoul,
  };

  enum class SignSprite : uint8_t {
    Sign,
    Headstone,
    Bell,
    MirrorEye,
    PlayerLight,
    PlayerShadow,
  };

  enum class SplitMode : uint8_t {
    None,
    Light,
    Shadow,
  };

  enum class CaughtKind : uint8_t {
    None,
    Light,
    Shadow,
    Dead,
  };

  struct Entity {
    EntityType type = EntityType::None;
    int8_t x = 0;
    int8_t y = 0;
    uint8_t id = 0;         // shard index (0..14), npc id, or sign table id
    bool blocking = false;  // for signs: if true, the sign blocks movement and must be read
    SignSprite signSprite = SignSprite::Sign;
  };

  struct World {
    Tile tiles[GRID][GRID] = {};
    std::array<Entity, MAX_ENTITIES> entities{};
    uint8_t count = 0;
  };

  // ── Game state ──
  Screen screen_ = Screen::Intro;
  World light_{};
  World shadow_{};

  uint8_t chapter_ = 1;
  int roomX_ = 0;
  int roomY_ = 0;
  int lightX_ = 7;
  int lightY_ = 7;
  int shadowX_ = 7;
  int shadowY_ = 7;

  // Split mode (one world walks alone)
  SplitMode splitMode_ = SplitMode::None;
  int splitSteps_ = 0;
  int splitAnchorX_ = 7;
  int splitAnchorY_ = 7;

  // Mirror mode (shadow inverted)
  int mirrorSteps_ = 0;
  int mirrorAnchorX_ = 7;
  int mirrorAnchorY_ = 7;

  // Shard progression (5 bits per chapter)
  uint8_t shardsMask_ = 0;
  uint8_t voidShardsMask_ = 0;
  uint8_t echoShardsMask_ = 0;

  // Chapter complete flag (pending victory screen trigger after dialogue)
  bool pendingVictory_ = false;
  bool victory_ = false;

  // HP
  uint8_t hp_ = MAX_HEARTS;
  bool ambushed_ = false;

  // Pressure plate doors (reset on room entry)
  bool lDoorsLatched_ = false;
  bool sDoorsLatched_ = false;
  bool linkedPlates_ = false;

  // Caught modal
  CaughtKind caughtKind_ = CaughtKind::None;

  // Visited room bitmap (for minimap)
  uint8_t visited_[WORLD_SIZE][WORLD_SIZE] = {};

  // Housekeeping
  uint32_t steps_ = 0;
  bool backConsumed_ = false;
  bool needsFullRefresh_ = true;

  // UI state
  std::string message_;
  std::string pickupMessage_;
  std::vector<std::string> dialogue_;
  size_t dialogueLine_ = 0;

  // ── Methods ──
  void resetGame();
  void resetChapter();
  void respawnAtWatcher();
  void markVisited(int rx, int ry);
  bool wasVisited(int rx, int ry) const;
  bool anyShardRemainingAt(int rx, int ry) const;

  void buildRoom(int rx, int ry, World& light, World& shadow) const;
  bool buildTutorialRoom(int rx, int ry, World& light, World& shadow) const;
  bool buildChapter2Room(int rx, int ry, World& light, World& shadow) const;
  bool buildChapter3Room(int rx, int ry, World& light, World& shadow, bool& linked) const;
  bool buildEasterEggRoom(int rx, int ry, World& light, World& shadow) const;
  void buildProceduralRoom(int rx, int ry, World& light, World& shadow) const;
  void sealBoundaryEdges(int rx, int ry, World& light, World& shadow) const;

  void loadRoom(int rx, int ry, int entryX, int entryY);

  void clearWorld(World& world) const;
  void addEntity(World& world, EntityType type, int x, int y, uint8_t id = 0, bool blocking = false,
                 SignSprite sprite = SignSprite::Sign) const;
  void addPerimeter(World& world) const;
  void addWalls(World& world, std::initializer_list<std::array<int, 2>> points) const;
  void addDoors(World& world, std::initializer_list<std::array<int, 2>> points) const;
  void addWater(World& world, std::initializer_list<std::array<int, 2>> points) const;
  void filterCollectedShardsFromWorld(World& world) const;

  // Shard helpers
  bool isShardCollected(uint8_t chapter, uint8_t index) const;
  int shardCountForChapter(uint8_t chapter) const;
  int totalShardsThisChapter() const;
  const char* shardPickupLine(uint8_t chapter, int countAfterPickup) const;

  // Tile rules
  bool isWalkable(Tile tile, bool doorsOpen) const;
  bool inside(int x, int y) const;
  int snapDoor(int v) const;
  int findEntityAt(const World& world, int x, int y, EntityType type = EntityType::None) const;
  int findHalfAt(const World& world, int x, int y, EntityType halfType) const;
  bool hasBlockingEntity(const World& world, int x, int y) const;
  void removeEntity(World& world, int index);

  // Pressure plate
  bool isPlatePressed(const World& world, int playerX, int playerY) const;
  bool computeLightDoorsOpen() const;   // whether doors in LIGHT world are open
  bool computeShadowDoorsOpen() const;  // whether doors in SHADOW world are open

  // Ghouls
  bool hasGhouls(const World& world) const;
  void moveGhoulsIn(World& world, int targetX, int targetY);
  bool checkGhoulCollision(const World& world, int x, int y) const;
  void tickGhoulsAndCheck(int lightX, int lightY, int shadowX, int shadowY);
  void onCaught(CaughtKind kind);

  // Movement
  bool tryWalkOrPush(World& world, EntityType halfType, int nx, int ny, int dx, int dy, bool execute,
                     bool doorsOpen);
  void moveCoupled(int dx, int dy);
  void moveSplit(int dx, int dy);
  void tryTransition(int dx, int dy);
  void snapSplit(const char* text = "The thread pulls you home.");
  void snapMirror(const char* text = "The eye closes. You return to yourself.");
  void toggleLightSplit();
  void toggleShadowSplit();
  void collectAlignedShard(uint8_t chapter, uint8_t index, int x, int y);
  void updateAlignmentMessage();

  // Dialogue
  bool startAdjacentDialogueOrSign();
  void openWatcherDialogue();
  void openSageDialogue();
  void openSignDialogue(uint8_t signId);
  void advanceDialogue();

  // Drawing
  void drawIntro() const;
  void drawControls() const;
  void drawVictory() const;
  void drawPlay() const;
  void drawWorld(const World& world, int playerX, int playerY, int originY, bool shadowWorld, bool frozen,
                 bool doorsOpen) const;
  void drawStatus() const;
  void drawHearts(int x, int y, bool inverse) const;
  void drawMinimap() const;
  void drawShardRows() const;
  void drawSplitMeter(SplitMode mode, int originY, bool inverse) const;
  void drawMirrorMeter() const;
  void drawDialogue() const;
  void drawPickup() const;
  void drawCaughtModal() const;

  // Sprite primitives
  void drawDiamond(int cx, int cy, int radius, bool filled, bool black) const;
  void drawShardHalf(int tileX, int tileY, EntityType type, bool black) const;
  void drawShardMerged(int tileX, int tileY, bool black) const;
  void drawPlayerSprite(int x, int y, bool black) const;
  void drawNpcSprite(int x, int y, bool black) const;
  void drawGhoulSprite(int x, int y, bool black) const;
  void drawWallSprite(int x, int y, bool black) const;
  void drawTreeSprite(int x, int y, bool black) const;
  void drawWaterSprite(int x, int y, bool black) const;
  void drawDoorSprite(int x, int y, bool open, bool black) const;
  void drawPlateSprite(int x, int y, bool black) const;
  void drawMirrorSprite(int x, int y, bool black) const;
  void drawHeadstoneSprite(int x, int y, bool black) const;
  void drawBellSprite(int x, int y, bool black) const;
  void drawSignSprite(int x, int y, bool black) const;
  void drawSignEntity(const Entity& entity, int px, int py, bool black) const;
};
