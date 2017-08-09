#include "sushiboyz.h"
#include "hardware.h"
#include "coplist.h"
#include "gfx.h"
#include "ilbm.h"
#include "blitter.h"
#include "2d.h"
#include "sprite.h"
#include "interrupts.h"

#define WIDTH 144
#define HEIGHT 220
#define DEPTH 4

#define STARTX X((320 - WIDTH) / 2)
#define STARTY Y((256 - HEIGHT) / 2)

static BitmapT *toilet;
static CopListT *cp;
static CopInsT *bplptr[DEPTH], *pal;
static PaletteT *ballPal;
static SpriteT *sprite[2];

static void Load() {
  toilet = LoadILBMCustom("05_ninja_kibel.iff", BM_LOAD_PALETTE);

  {
    BitmapT *ball = LoadILBMCustom("05_kiblo_kulka.iff", BM_LOAD_PALETTE);
    ballPal = ball->palette;
    sprite[0] = NewSpriteFromBitmap(ball->height, ball, 0, 0);
    sprite[1] = NewSpriteFromBitmap(ball->height, ball, 16, 0);
    DeleteBitmap(ball);
  }

  UpdateSprite(sprite[0], X(144), Y(200));
  UpdateSprite(sprite[1], X(144) + 16, Y(200));
}

static void Prepare() {
  BitmapMakeDisplayable(toilet);
}

static void UnLoad() {
  DeletePalette(toilet->palette);
  DeleteBitmap(toilet);
  DeletePalette(ballPal);
  DeleteSprite(sprite[0]);
  DeleteSprite(sprite[1]);
}

static __interrupt LONG RunEachFrame() {
  UpdateFrameCount();

  if (frameFromStart < 32) {
    FadeBlack(toilet->palette, pal, frameFromStart / 2);
    FadeBlack(ballPal, pal + 16, frameFromStart / 2);
  } else if (frameTillEnd < 16) {
    FadeBlack(toilet->palette, pal, frameTillEnd);
    FadeBlack(ballPal, pal + 16, frameTillEnd);
  }

  if (frameTillEnd < 64) {
    UpdateSprite(sprite[0], X(144), Y(200) + 63 - frameTillEnd);
    UpdateSprite(sprite[1], X(144) + 16, Y(200) + 63 - frameTillEnd);
  }

  return 0;
}

INTERRUPT(FrameInterrupt, 0, RunEachFrame);

static void Init() {
  CopInsT *sprptr[8];

  cp = NewCopList(200);
  CopInit(cp);
  CopSetupGfxSimple(cp, MODE_LORES, DEPTH, STARTX, STARTY, WIDTH, HEIGHT);
  CopSetupBitplanes(cp, bplptr, toilet, DEPTH);
  CopSetupSprites(cp, sprptr);
  pal = CopLoadColor(cp, 0, 31, 0);
  CopEnd(cp);

  CopInsSet32(sprptr[0], sprite[0]->data);
  CopInsSet32(sprptr[1], sprite[0]->attached->data);
  CopInsSet32(sprptr[2], sprite[1]->data);
  CopInsSet32(sprptr[3], sprite[1]->attached->data);

  CopListActivate(cp);
  custom->dmacon = DMAF_SETCLR | DMAF_RASTER | DMAF_SPRITE;

  AddIntServer(INTB_VERTB, &FrameInterrupt);
}

static void Kill() {
  RemIntServer(INTB_VERTB, &FrameInterrupt);

  custom->dmacon = DMAF_COPPER | DMAF_RASTER | DMAF_SPRITE;
  DeleteCopList(cp);
}

static void Render() {
  EffectPrepare(&Floor);
}

EFFECT(Toilet, Load, UnLoad, Init, Kill, Render, Prepare);
