#ifndef GAME_H
#define GAME_H

#include <genesis.h>

///////////////////////////////////////////////////////////////////////////////////
// Game Constants

#define SHOTS_MAX       6
#define MAX_COWS        3
#define MAX_UFOS        3

// UFO states
#define UFO_STATE_INACTIVE   0
#define UFO_STATE_WAITING    1
#define UFO_STATE_DESCENDING 2
#define UFO_STATE_ABDUCTING  3
#define UFO_STATE_HIT        4
#define UFO_STATE_RETREATING 5

// Cow states
#define COW_STATE_IDLE       0
#define COW_STATE_ABDUCTING  1
#define COW_STATE_GONE       2
#define COW_STATE_RESCUING   3

// Game phases
#define PHASE_PLAYING   0
#define PHASE_ROUND_END 1
#define PHASE_CALIBRATE 3

// XGM PCM sample IDs for SFX
#define SFX_SHOT_ID 64
#define SFX_HIT_ID  65

// XGM PCM playback channels/priorities for SFX routing
#define SFX_CH_SHOT 1
#define SFX_CH_HIT  2
#define SFX_PRIO_MAX 15

// Cow placement positions (bottom-left of sprite)
#define COW_POS_FAR_X   146
#define COW_POS_FAR_Y   140
#define COW_POS_NEAR1_X 75
#define COW_POS_NEAR1_Y 186
#define COW_POS_NEAR2_X 192
#define COW_POS_NEAR2_Y 210

// UFO speed
#define UFO_DESCEND_SPEED 1
#define UFO_RETREAT_SPEED 4

// UFO scale animations (0=largest/nearest, 7=smallest/farthest)
#define UFO_ANIM_COUNT 8
#define UFO_START_Y   -40

// UFO widths per scale anim (pixels, approximate from sprite sheet)
#define UFO_SPRITE_WIDTH 64  // all anims are 8 tiles wide
#define UFO_SPRITE_HEIGHT 32 // all anims are 4 tiles tall

// Per-animation UFO hitbox (offsets within sprite cell)
typedef struct {
  s16 offX, offY;
  u16 w, h;
} UfoHitbox;

static const UfoHitbox ufoHitboxes[UFO_ANIM_COUNT] = {
  {  0,  0, 64, 32 },  // anim 0 (largest/nearest)
  {  0,  0, 64, 32 },  // anim 1
  {  0,  0, 64, 32 },  // anim 2
  {  0,  0, 64, 32 },  // anim 3
  {  0,  0, 64, 32 },  // anim 4
  {  0,  0, 64, 32 },  // anim 5
  {  0,  0, 64, 32 },  // anim 6
  {  0,  0, 64, 32 },  // anim 7 (smallest/farthest)
};

// Cow sprite widths
#define CW_WIDTH      32   // 4 tiles * 8px
#define CW2X_WIDTH    48   // 6 tiles * 8px

// UFO spawn stagger delay (frames between each UFO appearing)
#define UFO_SPAWN_DELAY 90

// UFO bob amplitude during abduction (pixels)
#define UFO_BOB_AMPLITUDE 3
#define UFO_BOB_SPEED     4  // frames per bob step

// Abduction speed (frames between cow anim advances)
#define ABDUCT_TICK_RATE 4

// Total cow animation frames (cw.png: 416/32 = 13 frames)
#define COW_ANIM_FRAMES  13
#define RESCUE_TICK_RATE  2

// Cow sprite heights (for bottom-left to top-left conversion)
#define CW_HEIGHT     64   // 8 tiles * 8px
#define CW2X_HEIGHT   80   // 10 tiles * 8px

///////////////////////////////////////////////////////////////////////////////////
// Cow placement table

typedef struct {
  s16 x, y;
  bool useLarge;
} CowPlacement;

static const CowPlacement cowPlacements[MAX_COWS] = {
  { COW_POS_FAR_X,   COW_POS_FAR_Y,   FALSE },
  { COW_POS_NEAR1_X, COW_POS_NEAR1_Y, TRUE  },
  { COW_POS_NEAR2_X, COW_POS_NEAR2_Y, TRUE  },
};

// Explosion animation frames (ex.png: 256x64, 2 anims x 4 frames)
#define EXPLOSION_FRAMES 4

// UFO entry patterns
#define UFO_ENTRY_TOP     0  // classic: descend from top of screen
#define UFO_ENTRY_HORIZON 1  // start small at horizon, scale up with zig-zag
#define UFO_ENTRY_SIDE    2  // enter from left or right side
#define UFO_ENTRY_COUNT   3

///////////////////////////////////////////////////////////////////////////////////
// Per-UFO State

typedef struct {
  Sprite *sprite;
  Sprite *sparkSprite;
  s16 x, y;
  s16 targetX, targetY;
  u8  state;
  u8  hitTimer;
  u8  cowIndex;
  u8  targetAnim;  // final scale anim (0=large near, 3=small far)
  u8  curAnim;     // current scale anim for collision
  u16 spawnDelay;  // frames to wait before descending
  u16 bobTimer;    // bob animation counter
  u8  entryPattern; // how UFO enters the screen
  s16 entryStartX;  // starting X for entry
  s16 entryStartY;  // starting Y for entry
  u16 entryTimer;   // progress counter for entry animation
} UfoState;

///////////////////////////////////////////////////////////////////////////////////
// Per-Cow State

typedef struct {
  Sprite *sprite;
  s16 x, y;
  u8  state;
  u8  abductTick;
  u8  abductFrame;
  bool isLarge;
} CowState;

// Calibration targets (5 points: L/C/R at mid-height, plus top-center and bottom-center)
#define CAL_POINTS 5

// Reticle speed for d-pad control
#define RETICLE_SPEED 3

///////////////////////////////////////////////////////////////////////////////////
// Game State

typedef struct {
  UfoState ufos[MAX_UFOS];
  CowState cows[MAX_COWS];

  s16 reticleX, reticleY;
  Sprite *reticle;

  // Calibration state
  u8  calStep;              // which calibration point (0-4)
  s16 calRawX[CAL_POINTS];  // recorded raw X values
  s16 calRawY[CAL_POINTS];  // recorded raw Y values

  u8  round;
  u8  numCowsThisRound;
  u8  cowsAbducted;
  u8  cowsSaved;
  u8  phase;

  int triggerPull;
  int remainingShots;

  u16 roundTimer;
  u16 score;
} GameState;

#endif // GAME_H
