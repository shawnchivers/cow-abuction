#include <genesis.h>
#include "resources.h"

#include "game.h"

static u8 gunType = 0;  // 0=none, 1=justifier

#include "game_phases.h"

///////////////////////////////////////////////////////////////////////////////////
// Initialization

static void initPalettes() {
  PAL_setPalette(PAL0, bgrect_pal.data, CPU);
  PAL_setPalette(PAL1, cow_pal.data, CPU);
  PAL_setPalette(PAL2, ufo_pal.data, CPU);
  PAL_setPalette(PAL3, abductPal, CPU);
  VDP_setTextPalette(3);
}

static void initBackground() {
  s16 indexB = TILE_USER_INDEX;
  VDP_loadTileSet(bgrect.tileset, indexB, CPU);
  VDP_drawImageEx(BG_B, &bgrect, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, indexB), 0, 0, FALSE, TRUE);
}

static void initGun() {
  calculateXLookup();

  u8 portType = JOY_getPortType(PORT_2);
  if (portType == PORT_TYPE_JUSTIFIER) {
    JOY_setSupport(PORT_2, JOY_SUPPORT_JUSTIFIER_BLUE);
    gunType = 1;
  } else {
    // No supported light gun found on port 2
    JOY_setSupport(PORT_2, JOY_SUPPORT_3BTN);
  }

  JOY_setSupport(PORT_1, JOY_SUPPORT_3BTN);
}

static void initAudio() {
  // Start music after first VBlank so the sound driver is fully initialized.
  SYS_doVBlankProcess();
  XGM_setPCM(SFX_SHOT_ID, gunshot_sfx, sizeof(gunshot_sfx));
  XGM_setPCM(SFX_HIT_ID, hitgun_mix_sfx, sizeof(hitgun_mix_sfx));
  XGM_startPlay(gameon);
  XGM_setLoopNumber(-1);
}

static void initGameState(GameState *gs) {
  *gs = (GameState){0};
  gs->round = 1;
  gs->reticleX = 128;
  gs->reticleY = 112;
  gs->phase = PHASE_CALIBRATE;
  gs->calStep = 0;
}

static void createSprites(GameState *gs) {
  // Create reticle sprite (always on top, hidden during calibration)
  gs->reticle = SPR_addSprite(&reticle, 0, 0, TILE_ATTR(PAL2, TRUE, FALSE, FALSE));
  SPR_setDepth(gs->reticle, SPR_MIN_DEPTH);
  SPR_setVisibility(gs->reticle, HIDDEN);

  // Create cow sprites (2 small + 1 large foreground) — hidden until game starts
  gs->cows[0].sprite = SPR_addSprite(&cow, 0, 0, TILE_ATTR(PAL1, FALSE, FALSE, FALSE));
  gs->cows[1].sprite = SPR_addSprite(&cow2x, 0, 0, TILE_ATTR(PAL1, FALSE, FALSE, FALSE));
  gs->cows[2].sprite = SPR_addSprite(&cow2x, 0, 0, TILE_ATTR(PAL1, FALSE, FALSE, FALSE));
  for (u8 c = 0; c < MAX_COWS; c++) {
    SPR_setVisibility(gs->cows[c].sprite, HIDDEN);
  }

  // Create UFO sprites + spark sprites + explosion sprites
  for (u8 i = 0; i < MAX_UFOS; i++) {
    gs->ufos[i].sprite = SPR_addSprite(&ufo, 0, 0, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    gs->ufos[i].sparkSprite = SPR_addSprite(&explosion, 0, 0, TILE_ATTR(PAL2, FALSE, FALSE, FALSE));
    SPR_setVisibility(gs->ufos[i].sprite, HIDDEN);
    SPR_setVisibility(gs->ufos[i].sparkSprite, HIDDEN);
  }
}

static u8 updateReticleFromInput(GameState *gs, u16 joy1, s16 xVal, s16 yVal) {
  // D-pad moves reticle and shows it
  u8 dpadUsed = 0;
  if (joy1 & BUTTON_LEFT)  { gs->reticleX -= RETICLE_SPEED; dpadUsed = 1; }
  if (joy1 & BUTTON_RIGHT) { gs->reticleX += RETICLE_SPEED; dpadUsed = 1; }
  if (joy1 & BUTTON_UP)    { gs->reticleY -= RETICLE_SPEED; dpadUsed = 1; }
  if (joy1 & BUTTON_DOWN)  { gs->reticleY += RETICLE_SPEED; dpadUsed = 1; }

  // Clamp reticle to screen before gun update
  if (gs->reticleX < 0) gs->reticleX = 0;
  if (gs->reticleX > 240) gs->reticleX = 240;
  if (gs->reticleY < 0) gs->reticleY = 0;
  if (gs->reticleY > 208) gs->reticleY = 208;

  // Update reticle from gun if available
  if (gunType == 1 && xVal > 0) {
    // Justifier: raw values are HV counter, use calibrated lookup + Y scale
    s16 newX = xLookup[xVal];
    s16 newY = (s16)(((s32)yVal * yScale256) / 256) + yOffset;
    gs->reticleX = newX;
    gs->reticleY = newY;
  }

  // Clamp after gun update too
  if (gs->reticleX < 0) gs->reticleX = 0;
  if (gs->reticleX > 248) gs->reticleX = 248;
  if (gs->reticleY < 0) gs->reticleY = 0;
  if (gs->reticleY > 216) gs->reticleY = 216;

  return dpadUsed;
}

static void updateReticleSprite(GameState *gs, u8 dpadUsed, s16 xVal) {
  if (gs->phase == PHASE_CALIBRATE) return;

  // Hide reticle when Justifier is active, show when d-pad is used
  if (gunType == 1) {
    if (dpadUsed) {
      SPR_setVisibility(gs->reticle, VISIBLE);
    } else if (xVal > 0) {
      SPR_setVisibility(gs->reticle, HIDDEN);
    }
  } else {
    SPR_setVisibility(gs->reticle, VISIBLE);
  }

  SPR_setPosition(gs->reticle, gs->reticleX - 16, gs->reticleY - 16);
}

///////////////////////////////////////////////////////////////////////////////////
// Candle flicker for yellow (PAL0 color 8)
// Base color: #fbf236 -> VDP: E E 2 (Mega Drive uses 3-bit per channel: 0-E)
// Flicker varies channels for a warm candle effect
// Mega Drive palette: 0x0BGR, each channel 0-E (even values only)
// Base yellow = 0x02EE (B=0x2, G=0xE, R=0xE)

#define YELLOW_PAL_INDEX  8

static const u16 flickerColors[] = {
  0x02EE,  // base bright yellow
  0x02CE,  // green slightly dimmed
  0x02EC,  // red slightly dimmed
  0x02EE,  // base
  0x02CC,  // both dimmed - darker flicker
  0x02EE,  // base
  0x02CE,  // green dimmed
  0x02EE,  // base
};
#define FLICKER_COUNT (sizeof(flickerColors) / sizeof(flickerColors[0]))

static u8 flickerTimer = 0;
static u8 flickerIndex = 0;

static void updateCandleFlicker() {
  flickerTimer++;
  if (flickerTimer >= 6) {  // change every ~6 frames for subtle effect
    flickerTimer = 0;
    flickerIndex = (flickerIndex + 1) % FLICKER_COUNT;
    // Use random skip occasionally for organic feel
    if (random() % 3 == 0)
      flickerIndex = (flickerIndex + 1) % FLICKER_COUNT;
    PAL_setColor(YELLOW_PAL_INDEX, flickerColors[flickerIndex]);
  }
}

///////////////////////////////////////////////////////////////////////////////////
// Main

int main(bool hard)
{
  VDP_setScreenWidth256();
  initBackground();
  SPR_init();
  initPalettes();
  initGun();

  GameState gs;
  initGameState(&gs);
  createSprites(&gs);

  // Show calibration screen (startRound called after calibration completes)
  drawCalibrationScreen(&gs);
  initAudio();

  while (TRUE) {
    updateCandleFlicker();
    u16 joy1 = JOY_readJoypad(JOY_1);
    u16 joy2 = JOY_readJoypad(JOY_2);
    s16 xVal = JOY_readJoypadX(JOY_2);
    s16 yVal = JOY_readJoypadY(JOY_2);
    u8 dpadUsed = updateReticleFromInput(&gs, joy1, xVal, yVal);
    updateReticleSprite(&gs, dpadUsed, xVal);

    if (gs.phase == PHASE_CALIBRATE) {
      updateCalibration(&gs, joy1, joy2, xVal, yVal);
    } else if (gs.phase != PHASE_ROUND_END) {
      updateGame(&gs, joy1, joy2, gs.reticleX, gs.reticleY);
    } else {
      handleRoundEnd(&gs);
    }

    SPR_update();
    SYS_doVBlankProcess();
  }
}
