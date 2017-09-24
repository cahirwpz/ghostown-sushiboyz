#include "ilbm.h"
#include "gfx.h"
#include "blitter.h"
#include "sushiboyz.h"

#define WIDTH 320
#define HEIGHT 256
#define DEPTH 5

/* Display image while demo is booting. */

static CopListT *cp;
static BitmapT *boot;

static void Load() {
  boot = LoadILBM("boot.ilbm");
}

static void UnLoad() {
  DeletePalette(boot->palette);
  DeleteBitmap(boot);
}

static void Init() {
  UWORD cx = (WIDTH - boot->width) / 2;
  UWORD cy = (HEIGHT - boot->height) / 2;

  EnableDMA(DMAF_BLITTER);
  BitmapCopyFast(screen0, cx, cy, boot);
  BitmapCopyFast(screen1, cx, cy, boot);
  DisableDMA(DMAF_BLITTER);

  cp = NewCopList(100);

  CopInit(cp);
  CopSetupGfxSimple(cp, MODE_LORES, DEPTH, X(0), Y(0), WIDTH, HEIGHT);
  CopSetupBitplanes(cp, NULL, screen0, DEPTH);
  CopLoadPal(cp, boot->palette, 0);
  CopEnd(cp);

  CopListActivate(cp);
  EnableDMA(DMAF_RASTER);
}

static void Kill() {
  DisableDMA(DMAF_RASTER | DMAF_COPPER);
  DeleteCopList(cp);
}

EFFECT(Loading, Load, UnLoad, Init, Kill, NULL, NULL);
