#ifndef GAME_PHASES_H
#define GAME_PHASES_H

#include "game.h"
#include "movement.h"
#include "collide.h"

// Yellow-tinted cow palette for abduction effect (loaded into PAL3)
static const u16 abductPal[16] = {
  0x0000, 0x00EE, 0x006E, 0x0AEE, 0x0AEE, 0x068E, 0x0044, 0x0046,
  0x008E, 0x0288, 0x02CE, 0x04EE, 0x0068, 0x06CA, 0x0244, 0x0EEE
};

// Forward declarations
static void startRound(GameState *gs);
static void handleRoundEnd(GameState *gs);

///////////////////////////////////////////////////////////////////////////////////
// Justifier Light Gun - H32 (256px) mode
//
// The Justifier returns latched HV counter values via JOY_readJoypadX/Y.
// The H counter is NOT linearly mapped to pixel X — it needs piecewise
// calibration using 3 points (left, center, right).
// The V counter is close to 1:1 with scanlines but still benefits from
// multi-point calibration for Y offset and scale.

// H counter range for active display in H32 mode (approximate)
#define H32_LEFT   0
#define H32_RIGHT  148

static s16 xLookup[256];
static s16 yOffset = 0;
static s16 yScale256 = 256;  // Y scale factor * 256 (256 = 1.0)

// 5-point calibration: left/center/right at mid, plus top-center and bottom-center
// Screen positions for calibration targets (pixel coords)
static const s16 calTargetX[CAL_POINTS] = { 32, 128, 224, 128, 128 };
static const s16 calTargetY[CAL_POINTS] = { 112, 112, 112,  32, 192 };

static void buildLookupFromCalibration(s16 rawLeft, s16 rawCenter, s16 rawRight) {
  // Piecewise linear interpolation using all 3 X calibration points
  // Segment 1: rawLeft -> rawCenter maps to pixel 32 -> 128
  // Segment 2: rawCenter -> rawRight maps to pixel 128 -> 224
  // Extrapolate outside the calibration range
  int i;

  if (rawCenter <= rawLeft || rawRight <= rawCenter) {
    // Fallback: use outer points only
    s16 minRaw = rawLeft;
    s16 maxRaw = rawRight;
    if (maxRaw <= minRaw) {
      minRaw = H32_LEFT;
      maxRaw = H32_RIGHT;
    }
    for (i = 0; i < 256; i++) {
      s16 pos = 32 + (((s32)(i - minRaw) * 192) / (maxRaw - minRaw));
      if (pos < 0) pos = 0;
      if (pos > 255) pos = 255;
      xLookup[i] = pos;
    }
    return;
  }

  for (i = 0; i < 256; i++) {
    s16 pos;
    if (i <= rawCenter) {
      // Left segment: rawLeft(32) to rawCenter(128)
      pos = 32 + (((s32)(i - rawLeft) * 96) / (rawCenter - rawLeft));
    } else {
      // Right segment: rawCenter(128) to rawRight(224)
      pos = 128 + (((s32)(i - rawCenter) * 96) / (rawRight - rawCenter));
    }
    if (pos < 0) pos = 0;
    if (pos > 255) pos = 255;
    xLookup[i] = pos;
  }
}

static void calculateXLookup() {
  // Default H32 mapping without calibration
  int i;
  for (i = 0; i < 256; i++) {
    s16 pos = (((s32)(i - H32_LEFT) * 255) / (H32_RIGHT - H32_LEFT));
    if (pos < 0) pos = 0;
    if (pos > 255) pos = 255;
    xLookup[i] = pos;
  }
}

static void drawCalibrationScreen(GameState *gs) {
  VDP_clearPlane(BG_A, TRUE);

  if (gs->calStep < CAL_POINTS) {
    // Draw target crosshair at pixel position
    u8 tx = calTargetX[gs->calStep] >> 3;
    u8 ty = calTargetY[gs->calStep] >> 3;
    VDP_drawText("+", tx, ty);

    VDP_drawText("SHOOT THE TARGET", 8, 0);

    char buf[20];
    sprintf(buf, "POINT %d OF %d", gs->calStep + 1, CAL_POINTS);
    VDP_drawText(buf, 9, 26);

    VDP_drawText("START TO SKIP", 9, 27);
  }
}

static void updateCalibration(GameState *gs, u16 joy1, u16 joy2, s16 rawX, s16 rawY) {
  // START to skip calibration
  if ((joy1 & BUTTON_START) || (joy2 & BUTTON_START)) {
    calculateXLookup();
    yOffset = 0;
    VDP_clearPlane(BG_A, TRUE);
    gs->phase = PHASE_PLAYING;
    gs->calStep = 0;
    startRound(gs);
    return;
  }

  // Wait for trigger release before accepting next shot
  if (gs->triggerPull) {
    if (!(joy2 & BUTTON_A)) {
      gs->triggerPull = 0;
    }
    return;
  }

  // Accept calibration via light gun trigger only
  if ((joy2 & BUTTON_A) && rawX > 0) {
    gs->calRawX[gs->calStep] = rawX;
    gs->calRawY[gs->calStep] = rawY;
    gs->triggerPull = 1;
    gs->calStep++;
  }

  if (gs->calStep > 0 && gs->calStep <= CAL_POINTS && gs->triggerPull) {
    if (gs->calStep >= CAL_POINTS) {
      buildLookupFromCalibration(gs->calRawX[0], gs->calRawX[1], gs->calRawX[2]);

      // Y calibration from points 3 (top, Y=32) and 4 (bottom, Y=192)
      // Calculate offset and scale to map raw V counter to pixel Y
      {
        s16 rawTop = gs->calRawY[3];
        s16 rawBot = gs->calRawY[4];
        s16 rawMid = gs->calRawY[1];
        s16 rawYRange = rawBot - rawTop;
        s16 pixYRange = 192 - 32;  // 160 pixels

        if (rawYRange > 0) {
          // Scale: pixYRange / rawYRange, stored as *256 fixed point
          yScale256 = (s16)(((s32)pixYRange * 256) / rawYRange);
          // Offset so that rawMid maps to pixel 112
          yOffset = 112 - (s16)(((s32)(rawMid - rawTop) * pixYRange) / rawYRange) - 32;
        } else {
          // Fallback: use center point only
          yScale256 = 256;
          yOffset = calTargetY[1] - rawMid;
        }
      }

      VDP_clearPlane(BG_A, TRUE);
      gs->phase = PHASE_PLAYING;
      gs->calStep = 0;
      startRound(gs);
    } else {
      drawCalibrationScreen(gs);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////////
// Round Setup

static void startRound(GameState *gs) {
  // Determine how many cows this round (round 1 = 1, then up to MAX_COWS)
  gs->numCowsThisRound = gs->round;
  if (gs->numCowsThisRound > MAX_COWS)
    gs->numCowsThisRound = MAX_COWS;

  gs->cowsAbducted = 0;
  gs->cowsSaved = 0;
  gs->remainingShots = SHOTS_MAX;
  gs->triggerPull = 0;
  gs->phase = PHASE_PLAYING;
  gs->roundTimer = 0;

  // Initialize cows at their placement positions
  for (u8 i = 0; i < gs->numCowsThisRound; i++) {
    CowState *cow = &gs->cows[i];
    const CowPlacement *cp = &cowPlacements[i];
    cow->isLarge = cp->useLarge;
    cow->x = cp->x;
    cow->y = cp->y - (cow->isLarge ? CW2X_HEIGHT : CW_HEIGHT);
    cow->state = COW_STATE_IDLE;
    cow->abductFrame = 0;
    cow->abductTick = 0;

    SPR_setPosition(cow->sprite, cow->x, cow->y);
    SPR_setVisibility(cow->sprite, VISIBLE);
    SPR_setFrame(cow->sprite, 0);
    SPR_setHFlip(cow->sprite, random() & 1);
    // Depth: near cows (large Y) in front, far cows (small Y) behind
    SPR_setDepth(cow->sprite, SPR_MIN_DEPTH + i);
  }
  // Hide unused cows
  for (u8 i = gs->numCowsThisRound; i < MAX_COWS; i++) {
    SPR_setVisibility(gs->cows[i].sprite, HIDDEN);
  }

  // Initialize UFOs — one per cow, with random entry pattern
  for (u8 i = 0; i < gs->numCowsThisRound; i++) {
    UfoState *ufo = &gs->ufos[i];
    CowState *cow = &gs->cows[i];

    // Far cow (small) gets medium-small UFO anim, near cows get large
    ufo->targetAnim = cow->isLarge ? 0 : 3;

    // Center UFO horizontally over the cow (sprite is always 64px wide)
    s16 cowW = cow->isLarge ? CW2X_WIDTH : CW_WIDTH;
    ufo->targetX = cow->x + (cowW / 2) - (UFO_SPRITE_WIDTH / 2);
    // UFO bottom edge meets cow top edge
    ufo->targetY = cow->y - UFO_SPRITE_HEIGHT;

    // Pick random entry pattern
    ufo->entryPattern = random() % UFO_ENTRY_COUNT;
    ufo->entryTimer = 0;

    switch (ufo->entryPattern) {
      case UFO_ENTRY_HORIZON:
        // Start off-screen at top, at target X
        ufo->entryStartX = ufo->targetX;
        ufo->entryStartY = UFO_START_Y;
        ufo->x = ufo->entryStartX;
        ufo->y = ufo->entryStartY;
        break;
      case UFO_ENTRY_SIDE:
        // Start from left or right edge, near top of screen
        if (random() & 1) {
          ufo->entryStartX = -UFO_SPRITE_WIDTH;
        } else {
          ufo->entryStartX = 256;
        }
        ufo->entryStartY = 20;
        ufo->x = ufo->entryStartX;
        ufo->y = ufo->entryStartY;
        break;
      default: // UFO_ENTRY_TOP
        ufo->entryStartX = ufo->targetX;
        ufo->entryStartY = UFO_START_Y;
        ufo->x = ufo->entryStartX;
        ufo->y = ufo->entryStartY;
        break;
    }

    ufo->state = UFO_STATE_WAITING;
    ufo->spawnDelay = 60 + i * UFO_SPAWN_DELAY;
    ufo->hitTimer = 0;
    ufo->cowIndex = i;
    ufo->bobTimer = 0;

    SPR_setAnim(ufo->sprite, UFO_ANIM_COUNT - 1);
    SPR_setPosition(ufo->sprite, ufo->x, ufo->y);
    SPR_setVisibility(ufo->sprite, HIDDEN);
    // Near cows in front, far cows behind
    SPR_setDepth(ufo->sprite, SPR_MIN_DEPTH + i);
    SPR_setDepth(ufo->sparkSprite, SPR_MIN_DEPTH + i);
    SPR_setVisibility(ufo->sparkSprite, HIDDEN);
  }
  // Hide unused UFOs
  for (u8 i = gs->numCowsThisRound; i < MAX_UFOS; i++) {
    SPR_setVisibility(gs->ufos[i].sprite, HIDDEN);
    SPR_setVisibility(gs->ufos[i].sparkSprite, HIDDEN);
  }
}

///////////////////////////////////////////////////////////////////////////////////
// UFO Logic

static void updateUfo(GameState *gs, u8 idx) {
  UfoState *ufo = &gs->ufos[idx];
  CowState *cow = &gs->cows[ufo->cowIndex];

  switch (ufo->state) {
    case UFO_STATE_WAITING:
      if (ufo->spawnDelay > 0) {
        ufo->spawnDelay--;
      } else {
        ufo->state = UFO_STATE_DESCENDING;
        SPR_setVisibility(ufo->sprite, VISIBLE);
      }
      return;

    case UFO_STATE_DESCENDING: {
      ufo->entryTimer++;

      switch (ufo->entryPattern) {
        case UFO_ENTRY_HORIZON: {
          s16 totalDist, traveled;
          u8 startAnim, animRange, curAnim;

          // Move from horizon toward target position, scaling up
          moveToward(&ufo->y, ufo->targetY, UFO_DESCEND_SPEED);

          // Keep horizontal movement smooth to avoid visible jump snaps.
          moveToward(&ufo->x, ufo->targetX, 2);

          // Scale from smallest to targetAnim based on progress
          totalDist = ufo->targetY - ufo->entryStartY;
          traveled  = ufo->y - ufo->entryStartY;
          startAnim = UFO_ANIM_COUNT - 1;
          animRange = startAnim - ufo->targetAnim;
          if (totalDist > 0) {
            curAnim = startAnim - (u8)((traveled * animRange) / totalDist);
          } else {
            curAnim = ufo->targetAnim;
          }
          if (curAnim < ufo->targetAnim) curAnim = ufo->targetAnim;
          ufo->curAnim = curAnim;
          SPR_setAnim(ufo->sprite, curAnim);

          if (ufo->y == ufo->targetY && ufo->x == ufo->targetX) {
            ufo->state = UFO_STATE_ABDUCTING;
            cow->state = COW_STATE_ABDUCTING;
            SPR_setPalette(cow->sprite, PAL1);
          }
          break;
        }

        case UFO_ENTRY_SIDE: {
          s16 sideTotalX, sideTravX;
          u8 sideStartAnim, sideAnimRange, sideCurAnim;

          // Slide in from side, moving both X and Y toward target
          moveToward(&ufo->x, ufo->targetX, 3);
          moveToward(&ufo->y, ufo->targetY, 2);

          // Scale based on X progress (horizontal travel)
          sideTotalX = ufo->targetX - ufo->entryStartX;
          sideTravX  = ufo->x - ufo->entryStartX;
          sideStartAnim = UFO_ANIM_COUNT - 1;
          sideAnimRange = sideStartAnim - ufo->targetAnim;
          if (sideTotalX != 0) {
            sideCurAnim = sideStartAnim - (u8)((sideTravX * sideAnimRange) / sideTotalX);
          } else {
            sideCurAnim = ufo->targetAnim;
          }
          if (sideCurAnim < ufo->targetAnim) sideCurAnim = ufo->targetAnim;
          if (sideCurAnim > sideStartAnim) sideCurAnim = sideStartAnim;
          ufo->curAnim = sideCurAnim;
          SPR_setAnim(ufo->sprite, sideCurAnim);

          if (ufo->x == ufo->targetX && ufo->y == ufo->targetY) {
            ufo->state = UFO_STATE_ABDUCTING;
            cow->state = COW_STATE_ABDUCTING;
            SPR_setPalette(cow->sprite, PAL1);
          }
          break;
        }

        default: { // UFO_ENTRY_TOP
          s16 totalDist2, traveled2;
          u8 startAnim2, animRange2, curAnim2;

          moveToward(&ufo->y, ufo->targetY, UFO_DESCEND_SPEED);

          // Scale UFO animation based on descent progress
          totalDist2 = ufo->targetY - UFO_START_Y;
          traveled2  = ufo->y - UFO_START_Y;
          // Interpolate from smallest (UFO_ANIM_COUNT-1) to targetAnim
          startAnim2 = UFO_ANIM_COUNT - 1;
          animRange2 = startAnim2 - ufo->targetAnim;
          curAnim2 = ufo->targetAnim;
          if (totalDist2 > 0) {
            curAnim2 = startAnim2 - (u8)((traveled2 * animRange2) / totalDist2);
          }
          if (curAnim2 < ufo->targetAnim) curAnim2 = ufo->targetAnim;
          ufo->curAnim = curAnim2;
          SPR_setAnim(ufo->sprite, curAnim2);

          if (ufo->y == ufo->targetY) {
            ufo->state = UFO_STATE_ABDUCTING;
            cow->state = COW_STATE_ABDUCTING;
            SPR_setPalette(cow->sprite, PAL1);
          }
          break;
        }
      }
      break;
    }

    case UFO_STATE_ABDUCTING: {
      // Bob UFO slightly up and down
      ufo->bobTimer++;
      s16 bobOffset = 0;
      u16 bobPhase = (ufo->bobTimer / UFO_BOB_SPEED) % 4;
      if (bobPhase == 0) bobOffset = 0;
      else if (bobPhase == 1) bobOffset = -UFO_BOB_AMPLITUDE;
      else if (bobPhase == 2) bobOffset = 0;
      else bobOffset = UFO_BOB_AMPLITUDE;

      // Gradually drift UFO downward based on abduction progress
      // Far (small) cows need more drift, near (large) cows less
      s16 maxDrift = cow->isLarge ? 4 : 16;
      s16 drift = (cow->abductFrame * maxDrift) / COW_ANIM_FRAMES;

      ufo->y = ufo->targetY + bobOffset + drift;

      // Advance cow abduction animation
      cow->abductTick++;
      if (cow->abductTick >= ABDUCT_TICK_RATE) {
        cow->abductTick = 0;
        cow->abductFrame++;
        if (cow->abductFrame >= COW_ANIM_FRAMES) {
          // Cow fully abducted
          cow->state = COW_STATE_GONE;
          SPR_setVisibility(cow->sprite, HIDDEN);
          SPR_setPalette(cow->sprite, PAL1);
          PAL_setPalette(PAL3, abductPal, CPU);
          ufo->state = UFO_STATE_RETREATING;
          gs->cowsAbducted++;
        } else {
          SPR_setFrame(cow->sprite, cow->abductFrame);
        }
      }
      break;
    }

    case UFO_STATE_HIT: {
      ufo->hitTimer--;

      // Animate explosion: 4 frames over 28 ticks = 7 ticks per frame (slower)
      u8 exFrame = (28 - ufo->hitTimer) / 7;
      if (exFrame >= EXPLOSION_FRAMES) exFrame = EXPLOSION_FRAMES - 1;
      SPR_setFrame(ufo->sparkSprite, exFrame);

      // Screen flash on first few frames
      if (ufo->hitTimer >= 25) {
        u16 whitePal[16] = {0x0EEE,0x0EEE,0x0EEE,0x0EEE,0x0EEE,0x0EEE,0x0EEE,0x0EEE,
                            0x0EEE,0x0EEE,0x0EEE,0x0EEE,0x0EEE,0x0EEE,0x0EEE,0x0EEE};
        PAL_setPalette(PAL0, whitePal, CPU);
        PAL_setPalette(PAL1, whitePal, CPU);
        PAL_setPalette(PAL2, whitePal, CPU);
        PAL_setPalette(PAL3, whitePal, CPU);
      } else if (ufo->hitTimer == 24) {
        PAL_setPalette(PAL0, bgrect_pal.data, CPU);
        PAL_setPalette(PAL1, cow_pal.data, CPU);
        PAL_setPalette(PAL2, ufo_pal.data, CPU);
        PAL_setPalette(PAL3, abductPal, CPU);
      }

      // Shake explosion position for dramatic effect
      {
        s16 shakeX = (ufo->hitTimer & 2) ? 3 : -3;
        s16 shakeY = (ufo->hitTimer & 1) ? 2 : -2;
        SPR_setPosition(ufo->sparkSprite, ufo->x + shakeX, ufo->y + shakeY);
      }

      // UFO flies away upward
      moveToward(&ufo->y, UFO_START_Y, UFO_RETREAT_SPEED);

      // Add a brief shimmer by combining tiny jitter, flicker, and flip pulse.
      {
        s16 shimmerX = ufo->x + ((ufo->hitTimer & 2) ? 1 : -1);
        s16 shimmerY = ufo->y + ((ufo->hitTimer & 1) ? 0 : -1);
        SPR_setVisibility(ufo->sprite, (ufo->hitTimer & 1) ? VISIBLE : HIDDEN);
        SPR_setHFlip(ufo->sprite, (ufo->hitTimer & 2) ? TRUE : FALSE);
        SPR_setPosition(ufo->sprite, shimmerX, shimmerY);
      }

      if (ufo->hitTimer == 0) {
        SPR_setVisibility(ufo->sparkSprite, HIDDEN);
        SPR_setVisibility(ufo->sprite, VISIBLE);
        SPR_setHFlip(ufo->sprite, FALSE);
        ufo->state = UFO_STATE_RETREATING;
      }
      return;
    }

    case UFO_STATE_RETREATING: {
      s16 retTotalDist = ufo->targetY - UFO_START_Y;
      s16 retTraveled  = ufo->y - UFO_START_Y;
      u8 retStartAnim = UFO_ANIM_COUNT - 1;
      u8 retAnimRange = retStartAnim - ufo->targetAnim;
      u8 retCurAnim;

      moveToward(&ufo->y, UFO_START_Y, UFO_RETREAT_SPEED);

      // Scale UFO animation back toward smallest as it retreats
      if (retTotalDist > 0) {
        retCurAnim = retStartAnim - (u8)((retTraveled * retAnimRange) / retTotalDist);
      } else {
        retCurAnim = retStartAnim;
      }
      if (retCurAnim > retStartAnim) retCurAnim = retStartAnim;
      ufo->curAnim = retCurAnim;
      SPR_setAnim(ufo->sprite, retCurAnim);

      if (ufo->y <= UFO_START_Y) {
        ufo->state = UFO_STATE_INACTIVE;
        SPR_setVisibility(ufo->sprite, HIDDEN);
      }
      break;
    }

    default:
      return;
  }

  SPR_setPosition(ufo->sprite, ufo->x, ufo->y);
}

///////////////////////////////////////////////////////////////////////////////////
// Shooting

static void handleShooting(GameState *gs, s16 shotX, s16 shotY) {
  XGM_stopPlayPCM(SFX_CH_SHOT);
  XGM_startPlayPCM(SFX_SHOT_ID, SFX_PRIO_MAX, SFX_CH_SHOT);

  // Check if we hit any active UFO
  for (u8 i = 0; i < gs->numCowsThisRound; i++) {
    UfoState *ufo = &gs->ufos[i];
    if (ufo->state == UFO_STATE_DESCENDING || ufo->state == UFO_STATE_ABDUCTING) {
      // Pixel-correct hitbox from current animation
      const UfoHitbox *hb = &ufoHitboxes[ufo->curAnim];
      if (isPointInBox(shotX, shotY, ufo->x + hb->offX, ufo->y + hb->offY, hb->w, hb->h)) {
        XGM_stopPlayPCM(SFX_CH_HIT);
        XGM_startPlayPCM(SFX_HIT_ID, SFX_PRIO_MAX, SFX_CH_HIT);
        ufo->state = UFO_STATE_HIT;
        ufo->hitTimer = 28;
        // Start cow rescue animation immediately
        {
          CowState *hitCow = &gs->cows[ufo->cowIndex];
          if (hitCow->state == COW_STATE_ABDUCTING) {
            hitCow->state = COW_STATE_RESCUING;
            hitCow->abductTick = 0;
            SPR_setPalette(hitCow->sprite, PAL1);
            PAL_setPalette(PAL3, abductPal, CPU);
            gs->cowsSaved++;
          }
        }
        // Show explosion — map 8 UFO scales to 3 explosion scales
        {
          u8 exAnim = 0;
          if (ufo->curAnim >= 5) exAnim = 2;
          else if (ufo->curAnim >= 2) exAnim = 1;
          SPR_setAnim(ufo->sparkSprite, exAnim);
        }
        SPR_setFrame(ufo->sparkSprite, 0);
        SPR_setPosition(ufo->sparkSprite, ufo->x, ufo->y);
        SPR_setVisibility(ufo->sparkSprite, VISIBLE);
        gs->score += 100;
        break;
      }
    }
  }

  gs->remainingShots--;
  gs->triggerPull = 1;
}

///////////////////////////////////////////////////////////////////////////////////
// Round Check

static void checkRoundEnd(GameState *gs) {
  // Count active UFOs (including waiting ones) and rescuing cows
  u8 activeUfos = 0;
  u8 i;
  for (i = 0; i < gs->numCowsThisRound; i++) {
    if (gs->ufos[i].state != UFO_STATE_INACTIVE)
      activeUfos++;
    if (gs->cows[i].state == COW_STATE_RESCUING)
      activeUfos++;
  }

  // Round ends when all UFOs are gone
  if (activeUfos == 0) {
    gs->phase = PHASE_ROUND_END;
    gs->roundTimer = 90;

    // Hide all sprites
    for (u8 i = 0; i < MAX_COWS; i++)
      SPR_setVisibility(gs->cows[i].sprite, HIDDEN);
    for (u8 i = 0; i < MAX_UFOS; i++) {
      SPR_setVisibility(gs->ufos[i].sprite, HIDDEN);
      SPR_setVisibility(gs->ufos[i].sparkSprite, HIDDEN);
    }

    // Black out background
    VDP_clearPlane(BG_B, TRUE);
    PAL_setColor(0, 0x0000);

    // Show round text and score
    VDP_drawText("NEXT ROUND", 11, 13);
    char scoreBuf[20];
    sprintf(scoreBuf, "SCORE: %d", gs->score);
    VDP_drawText(scoreBuf, 11, 15);
  }
}

static void handleRoundEnd(GameState *gs) {
  gs->roundTimer--;
  if (gs->roundTimer == 0) {
    // Clear text and restore background + palettes
    VDP_clearPlane(BG_A, TRUE);

    s16 indexB = TILE_USER_INDEX;
    VDP_loadTileSet(bgrect.tileset, indexB, CPU);
    VDP_drawImageEx(BG_B, &bgrect, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, indexB), 0, 0, FALSE, TRUE);

    PAL_setPalette(PAL0, bgrect_pal.data, CPU);
    PAL_setPalette(PAL1, cow_pal.data, CPU);
    PAL_setPalette(PAL2, ufo_pal.data, CPU);
    PAL_setPalette(PAL3, abductPal, CPU);
    VDP_setTextPalette(3);

    gs->round++;
    startRound(gs);
  }
}

///////////////////////////////////////////////////////////////////////////////////
// Main Update

static void updateGame(GameState *gs, u16 joy1, u16 joy2, s16 shotX, s16 shotY) {
  if (gs->phase == PHASE_ROUND_END) {
    handleRoundEnd(gs);
    return;
  }

  // Handle shooting
  if (gs->remainingShots > 0) {
    if (!gs->triggerPull && ((joy2 & BUTTON_A) || (joy1 & BUTTON_A) || (joy1 & BUTTON_B))) {
      handleShooting(gs, shotX, shotY);
    }
  }
  // Clear trigger latch when all shoot buttons released
  if (!(joy2 & BUTTON_A) && !(joy1 & BUTTON_A) && !(joy1 & BUTTON_B)) {
    gs->triggerPull = 0;
  }

  // Reload: START, B button, or off-screen shot (coords at 0,0 or negative)
  if ((joy1 & BUTTON_START) || (joy2 & BUTTON_START) ||
      (joy1 & BUTTON_C) ||
      (shotX <= 0 && shotY <= 0 && (joy2 & BUTTON_A))) {
    gs->remainingShots = SHOTS_MAX;
  }

  // Update all active UFOs
  for (u8 i = 0; i < gs->numCowsThisRound; i++) {
    updateUfo(gs, i);
  }

  // Update rescue animations (reverse cow frames quickly)
  {
    u8 ri;
    for (ri = 0; ri < gs->numCowsThisRound; ri++) {
      CowState *cow = &gs->cows[ri];
      if (cow->state == COW_STATE_RESCUING) {
        cow->abductTick++;
        if (cow->abductTick >= RESCUE_TICK_RATE) {
          cow->abductTick = 0;
          if (cow->abductFrame > 0) {
            cow->abductFrame--;
            SPR_setFrame(cow->sprite, cow->abductFrame);
          } else {
            cow->state = COW_STATE_IDLE;
          }
        }
      }
    }
  }

  // Show score and ammo at top of screen
  char scoreBuf[20];
  sprintf(scoreBuf, "SCORE:%d", gs->score);
  VDP_drawText(scoreBuf, 0, 1);

  char shotsBuf[12];
  sprintf(shotsBuf, "AMMO:%d", gs->remainingShots);
  VDP_drawText(shotsBuf, 25, 1);

  checkRoundEnd(gs);
}

#endif // GAME_PHASES_H
