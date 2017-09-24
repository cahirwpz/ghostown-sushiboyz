#ifndef __SUSHIBOYZ_H__
#define __SUSHIBOYZ_H__

#include "gfx.h"
#include "sync.h"
#include "effect.h"
#include "palops.h"

#ifndef X
#define X(x) ((x) + 0x81)
#endif

#ifndef Y
#define Y(y) ((y) + 0x2c)
#endif

/* Common resources. */
extern BitmapT *screen0, *screen1;
extern TrackT **tracks;
extern struct List *VBlankEvent;

/* Demo timeline related variables and routines. */
extern UWORD frameFromStart;
extern UWORD frameTillEnd;
extern UWORD frameCount;
extern UWORD lastFrameCount;

void UpdateFrameCount();

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

/* Loading and Music are handled outside of normal effect processing. */
extern EffectT Loading;
extern EffectT Music;

#endif
