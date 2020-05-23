#ifndef __PALOPS_H__
#define __PALOPS_H__

#include "coplist.h"

__regargs void FadeBlack(PaletteT *pal, CopInsT *ins, WORD step);
__regargs void FadeWhite(PaletteT *pal, CopInsT *ins, WORD step);
__regargs void FadeIntensify(PaletteT *pal, CopInsT *ins, WORD step);
__regargs void ContrastChange(PaletteT *pal, CopInsT *ins, WORD step);
__regargs void FadeIn(PaletteT *pal, CopInsT *ins);
__regargs void FadeOut(PaletteT *pal, CopInsT *ins);

extern UBYTE envelope[24];

#endif
