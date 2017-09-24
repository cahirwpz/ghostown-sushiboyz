#include "sushiboyz.h"
#include "hardware.h"
#include "coplist.h"
#include "gfx.h"
#include "ilbm.h"
#include "interrupts.h"

static BitmapT *bitmap;
static CopListT *cp;
static CopInsT *pal;

static TrackT *flash;

static void Load() {
  bitmap = LoadILBMCustom("12_ghostown_logo.iff", BM_DISPLAYABLE|BM_LOAD_PALETTE);
  flash = TrackLookup(tracks, "ghostown.flash");
}

static void UnLoad() {
  DeletePalette(bitmap->palette);
  DeleteBitmap(bitmap);
}

static __interrupt LONG RunEachFrame() {
  UpdateFrameCount();

  if (frameFromStart < 32) {
    FadeBlack(bitmap->palette, pal, frameFromStart / 2);
  } else if (frameTillEnd < 32) {
    FadeBlack(bitmap->palette, pal, frameTillEnd / 2);
  } else {
    WORD s = TrackValueGet(flash, frameCount);
    if (s > 0)
      FadeWhite(bitmap->palette, pal, s / 2);
  }

  return 0;
}

INTERRUPT(FrameInterrupt, 0, RunEachFrame, NULL);

static void Init() {
  WORD w = bitmap->width;
  WORD h = bitmap->height;
  WORD xs = X((320 - w) / 2);
  WORD ys = Y((256 - h) / 2);
  cp = NewCopList(100);

  CopInit(cp);
  CopSetupGfxSimple(cp, MODE_LORES, bitmap->depth, xs, ys, w, h);
  CopSetupBitplanes(cp, NULL, bitmap, bitmap->depth);
  pal = CopLoadColor(cp, 0, 31, 0);
  CopEnd(cp);

  CopListActivate(cp);
  EnableDMA(DMAF_RASTER);

  AddIntServer(INTB_VERTB, FrameInterrupt);
}

static void Kill() {
  RemIntServer(INTB_VERTB, FrameInterrupt);

  DisableDMA(DMAF_COPPER | DMAF_RASTER);
  DeleteCopList(cp);
}

EFFECT(GhostownLogo, Load, UnLoad, Init, Kill, NULL, NULL);
