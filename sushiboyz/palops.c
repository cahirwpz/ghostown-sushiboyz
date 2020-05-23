#include "palops.h"
#include "color.h"

#include "sushiboyz.h"

__regargs void FadeBlack(PaletteT *pal, CopInsT *ins, WORD step) {
  WORD n = pal->count;
  UBYTE *c = (UBYTE *)pal->colors;

  if (step < 0)
    step = 0;
  if (step > 15)
    step = 15;

  while (--n >= 0) {
    WORD r = *c++;
    WORD g = *c++;
    WORD b = *c++;

    r = (r & 0xf0) | step;
    g = (g & 0xf0) | step;
    b = (b & 0xf0) | step;
    
    CopInsSet16(ins++, (colortab[r] << 4) | colortab[g] | (colortab[b] >> 4));
  }
}

__regargs void FadeWhite(PaletteT *pal, CopInsT *ins, WORD step) {
  WORD n = pal->count;
  UBYTE *c = (UBYTE *)pal->colors;

  if (step < 0)
    step = 0;
  if (step > 15)
    step = 15;

  while (--n >= 0) {
    WORD r = *c++;
    WORD g = *c++;
    WORD b = *c++;

    r = ((r & 0xf0) << 4) | 0xf0 | step;
    g = ((g & 0xf0) << 4) | 0xf0 | step;
    b = ((b & 0xf0) << 4) | 0xf0 | step;

    CopInsSet16(ins++, (colortab[r] << 4) | colortab[g] | (colortab[b] >> 4));
  }
}

__regargs void FadeIntensify(PaletteT *pal, CopInsT *ins, WORD step) {
  WORD n = pal->count;
  UBYTE *c = (UBYTE *)pal->colors;

  if (step < 0)
    step = 0;
  if (step > 15)
    step = 15;

  while (--n >= 0) {
    WORD r = *c++;
    WORD g = *c++;
    WORD b = *c++;

    CopInsSet16(ins++, ColorIncreaseContrastRGB(r, g, b, step));
  }
}

__regargs void ContrastChange(PaletteT *pal, CopInsT *ins, WORD step) {
  WORD n = pal->count;
  UBYTE *c = (UBYTE *)pal->colors;

  if (step >= 0) {
    if (step > 15)
      step = 15;

    while (--n >= 0) {
      WORD r = *c++;
      WORD g = *c++;
      WORD b = *c++;

      CopInsSet16(ins++, ColorIncreaseContrastRGB(r, g, b, step));
    }
  } else {
    step = -step;
    if (step > 15)
      step = 15;

    while (--n >= 0) {
      WORD r = *c++;
      WORD g = *c++;
      WORD b = *c++;

      CopInsSet16(ins++, ColorDecreaseContrastRGB(r, g, b, step));
    }
  }
}

__regargs void FadeIn(PaletteT *pal, CopInsT *ins) {
  FadeBlack(pal, ins, frameFromStart);
}

__regargs void FadeOut(PaletteT *pal, CopInsT *ins) {
  FadeBlack(pal, ins, frameTillEnd);
}

UBYTE envelope[24] = {
  0, 0, 1, 1, 2, 3, 5, 8, 11, 13, 14, 15,
  15, 14, 13, 11, 8, 5, 3, 2, 1, 1, 0, 0 
};
