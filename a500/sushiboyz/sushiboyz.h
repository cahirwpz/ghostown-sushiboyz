#ifndef __SUSHIBOYZ_H__
#define __SUSHIBOYZ_H__

#include "p61.h"
#include "coplist.h"
#include "gfx.h"
#include "timeline.h"
#include "sync.h"

#ifndef X
#define X(x) ((x) + 0x81)
#endif

#ifndef Y
#define Y(y) ((y) + 0x2c)
#endif

extern BitmapT *screen0, *screen1;
extern TrackT **tracks;
extern struct List *VBlankEvent;

/* References to all effects. */
extern EffectT Ninja1;
extern EffectT Ninja2;
extern EffectT Ninja3;
extern EffectT Toilet;
extern EffectT Floor;
extern EffectT RunningMan;
extern EffectT Blurred3D;
extern EffectT GhostownLogo;
extern EffectT Bumpmap;
extern EffectT Twister;
extern EffectT Watchmaker;
extern EffectT UVMap;
extern EffectT Filled3D;
extern EffectT Credits;
extern EffectT Thunders;
extern EffectT FlatShade;
extern EffectT SushiGirl;
extern EffectT Glitch;

__regargs void FadeBlack(PaletteT *pal, CopInsT *ins, WORD step);
__regargs void FadeWhite(PaletteT *pal, CopInsT *ins, WORD step);
__regargs void FadeIntensify(PaletteT *pal, CopInsT *ins, WORD step);
__regargs void ContrastChange(PaletteT *pal, CopInsT *ins, WORD step);

static inline void FadeIn(PaletteT *pal, CopInsT *ins) {
  FadeBlack(pal, ins, frameFromStart);
}

static inline void FadeOut(PaletteT *pal, CopInsT *ins) {
  FadeBlack(pal, ins, frameTillEnd);
}

extern UBYTE envelope[24];

#endif
