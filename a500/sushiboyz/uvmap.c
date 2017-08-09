#include "sushiboyz.h"
#include "blitter.h"
#include "coplist.h"
#include "interrupts.h"
#include "memory.h"
#include "io.h"
#include "png.h"
#include "ilbm.h"
#include "sprite.h"

#define WIDTH 160
#define HEIGHT 100
#define DEPTH 4
#define FULLPIXEL 1

static PixmapT *textureHi, *textureLo, *texture;
static BitmapT *window0, *window1, *sushi;
static PaletteT *sushiPal, *texturePal;
static UWORD *uvmap;
static CopListT *cp;
static CopInsT *bplptr[DEPTH], *pal;
static SpriteT *sprite[2];

static TrackT *pos_u, *pos_v, *sprite_x, *sprite_y;

#define UVMapRenderSize (WIDTH * HEIGHT / 2 * 10 + 2)
void (*UVMapRender)(UBYTE *chunky asm("a0"),
                    UBYTE *textureHi asm("a1"),
                    UBYTE *textureLo asm("a2"));

static __regargs void 
PixmapToTexture(PixmapT *image, PixmapT *imageHi, PixmapT *imageLo) {
  ULONG *data = image->pixels;
  LONG size = image->width * image->height;
  /* Extra halves for cheap texture motion. */
  ULONG *hi0 = imageHi->pixels;
  ULONG *hi1 = imageHi->pixels + size;
  ULONG *lo0 = imageLo->pixels;
  ULONG *lo1 = imageLo->pixels + size;
  WORD n = size / 4;
  register ULONG m1 asm("d6") = 0x0c0c0c0c;
  register ULONG m2 asm("d7") = 0x03030303;

  while (--n >= 0) {
    ULONG c = *data++;
    /* [0 0 0 0 a0 a1 a2 a3] => [a0 a1 0 0 a2 a3 0 0] */
    ULONG hi = ((c & m1) << 4) | ((c & m2) << 2);
    /* [0 0 0 0 b0 b1 b2 b3] => [ 0 0 b0 b1 0 0 b2 b3] */
    ULONG lo = ((c & m1) << 2) | (c & m2);
    *hi0++ = hi;
    *hi1++ = hi;
    *lo0++ = lo;
    *lo1++ = lo;
  }
}

static void MakeUVMapRenderCode() {
  UWORD *code = (APTR)UVMapRender;
  UWORD *data = uvmap;
  WORD n = WIDTH * HEIGHT / 2;

  /* The map is pre-scrambled to avoid one c2p pass: [a B C d] => [a C B d] */
  while (n--) {
    *code++ = 0x1029;  /* 1029 xxxx | move.b xxxx(a1),d0 */
    *code++ = *data++;
    *code++ = 0x802a;  /* 802a yyyy | or.b   yyyy(a2),d0 */
    *code++ = *data++;
    *code++ = 0x10c0;  /* 10c0      | move.b d0,(a0)+    */
  }

  *code++ = 0x4e75; /* rts */
}

static void Load() {
  uvmap = LoadFile("20_uvmap.bin", MEMF_PUBLIC);
  texture = LoadPNG("20_uvmap_texture.png", PM_CMAP8, MEMF_PUBLIC);
  texturePal = texture->palette;
  sushi = LoadILBMCustom("20_klip.iff", BM_LOAD_PALETTE|BM_KEEP_PACKED);
  sushiPal = sushi->palette;

  pos_u = TrackLookup(tracks, "uvmap.pos.u");
  pos_v = TrackLookup(tracks, "uvmap.pos.v");

  sprite_x = TrackLookup(tracks, "uvmap.sprite.x");
  sprite_y = TrackLookup(tracks, "uvmap.sprite.y");
}

static void Prepare() {
  textureHi = NewPixmap(texture->width, texture->height * 2,
                        PM_CMAP8, MEMF_PUBLIC);
  textureLo = NewPixmap(texture->width, texture->height * 2,
                        PM_CMAP8, MEMF_PUBLIC);
  PixmapToTexture(texture, textureHi, textureLo);
  DeletePixmap(texture);

  BitmapUnpack(sushi, 0);

  sprite[0] = NewSpriteFromBitmap(sushi->height, sushi, 0, 0);
  sprite[1] = NewSpriteFromBitmap(sushi->height, sushi, 16, 0);

  UpdateSprite(sprite[0], X(128), Y(112));
  UpdateSprite(sprite[1], X(128) + 16, Y(112));
  DeleteBitmap(sushi);

  UVMapRender = MemAlloc(UVMapRenderSize, MEMF_PUBLIC);
  MakeUVMapRenderCode();
  MemFree(uvmap);
}

static void UnLoad() {
  DeletePalette(texturePal);
  DeletePalette(sushiPal);
  DeleteSprite(sprite[0]);
  DeleteSprite(sprite[1]);
}

static struct {
  WORD phase;
  APTR *bpl;
} c2p = { 256, NULL };

#define BPLSIZE ((WIDTH * 2) * (HEIGHT * 2) / 8) /* 8000 bytes */
#define BLTSIZE ((WIDTH / 2) * HEIGHT)           /* 8000 bytes */

static void ChunkyToPlanar() {
  register APTR *bpl asm("a0") = c2p.bpl;

  switch (c2p.phase) {
    case 0:
      /* Initialize chunky to planar. */
      custom->bltamod = 2;
      custom->bltbmod = 2;
      custom->bltdmod = 0;
      custom->bltcdat = 0xF0F0;
      custom->bltafwm = -1;
      custom->bltalwm = -1;

      /* Swap 4x2, pass 1, high-bits. */
      custom->bltapt = bpl[0];
      custom->bltbpt = bpl[0] + 2;
      custom->bltdpt = bpl[1] + BLTSIZE / 2;

      /* (a & 0xF0F0) | ((b >> 4) & ~0xF0F0) */
      custom->bltcon0 = (SRCA | SRCB | DEST) | (ABC | ABNC | ANBC | NABNC);
      custom->bltcon1 = 4 << BSHIFTSHIFT;
      custom->bltsize = 1 | ((BLTSIZE / 8) << 6);
      break;

    case 1:
      custom->bltsize = 1 | ((BLTSIZE / 8) << 6); /* overall size: BLTSIZE / 2 bytes */
      break;

    case 2:
      /* Swap 4x2, pass 2, low-bits. */
      custom->bltapt = bpl[1] - 4;
      custom->bltbpt = bpl[1] - 2;
      custom->bltdpt = bpl[1] + BLTSIZE / 2 - 2;

      /* ((a << 4) & 0xF0F0) | (b & ~0xF0F0) */
      custom->bltcon0 = (SRCA | SRCB | DEST) | (ABC | ABNC | ANBC | NABNC) | (4 << ASHIFTSHIFT);
      custom->bltcon1 = BLITREVERSE;
      custom->bltsize = 1 | ((BLTSIZE / 8) << 6);
      break;

    case 3:
      custom->bltsize = 1 | ((BLTSIZE / 8) << 6); /* overall size: BLTSIZE / 2 bytes */
      break;

    case 4:
      custom->bltamod = 0;
      custom->bltbmod = 0;
      custom->bltdmod = 0;
      custom->bltcdat = 0xAAAA;

      custom->bltapt = bpl[1];
      custom->bltbpt = bpl[1];
      custom->bltdpt = bpl[2] + BLTSIZE / 2;

#if FULLPIXEL
      /* (a & 0xAAAA) | ((b >> 1) & ~0xAAAA) */
      custom->bltcon0 = (SRCA | SRCB | DEST) | (ABC | ABNC | ANBC | NABNC);
      custom->bltcon1 = 1 << BSHIFTSHIFT;
#else
      /* (a & 0xAAAA) */
      custom->bltcon0 = (SRCA | DEST) | (ABC | ANBC);
      custom->bltcon1 = 0;
#endif
      custom->bltsize = 2 | ((BLTSIZE / 8) << 6);
      break;

    case 5:
      custom->bltapt = bpl[1] + BLTSIZE / 2;
      custom->bltbpt = bpl[1] + BLTSIZE / 2;
      custom->bltdpt = bpl[3] + BLTSIZE / 2;
      custom->bltsize = 2 | ((BLTSIZE / 8) << 6);
      break;

    case 6:
      custom->bltapt = bpl[1] + BLTSIZE / 2 - 2;
      custom->bltbpt = bpl[1] + BLTSIZE / 2 - 2;
      custom->bltdpt = bpl[2] + BLTSIZE / 2 - 2;
      custom->bltcdat = 0xAAAA;

#if FULLPIXEL
      /* ((a << 1) & 0xAAAA) | (b & ~0xAAAA) */
      custom->bltcon0 = (SRCA | SRCB | DEST) | (ABC | ABNC | ANBC | NABNC) | (1 << ASHIFTSHIFT);
      custom->bltcon1 = BLITREVERSE;
#else
      /* (a & ~0xAAAA) */
      custom->bltcon0 = (SRCA | DEST) | (ABNC | ANBNC);
      custom->bltcon1 = BLITREVERSE;
#endif
      custom->bltsize = 2 | ((BLTSIZE / 8) << 6);
      break;

    case 7:
      custom->bltapt = bpl[1] + BLTSIZE - 2;
      custom->bltbpt = bpl[1] + BLTSIZE - 2;
      custom->bltdpt = bpl[3] + BLTSIZE / 2 - 2;
      custom->bltsize = 2 | ((BLTSIZE / 8) << 6);
      break;

    case 8:
      CopInsSet32(bplptr[0], bpl[2]);
      CopInsSet32(bplptr[1], bpl[2] + BLTSIZE / 2);
      CopInsSet32(bplptr[2], bpl[3]);
      CopInsSet32(bplptr[3], bpl[3] + BLTSIZE / 2);
      break;

    default:
      break;
  }

  c2p.phase++;

  custom->intreq = INTF_BLIT;
}

static __interrupt LONG FadeInFadeOut() {
  UpdateFrameCount();

  {
    WORD x = TrackValueGet(sprite_x, frameCount);
    WORD y = TrackValueGet(sprite_y, frameCount);

    UpdateSprite(sprite[0], X(x), Y(y));
    UpdateSprite(sprite[1], X(x + 16), Y(y));
  }

  if (frameFromStart < 16) {
    FadeBlack(texturePal, pal, frameFromStart);
    FadeBlack(sushiPal, pal + 16, frameFromStart);
  } else if (frameTillEnd < 16) {
    FadeBlack(texturePal, pal, frameTillEnd);
    FadeBlack(sushiPal, pal + 16, frameTillEnd);
  }

  return 0;
}

INTERRUPT(FadeInFadeOutInterrupt, 0, FadeInFadeOut);
INTERRUPT(ChunkyToPlanarInterrupt, 0, ChunkyToPlanar);

static struct Interrupt *oldBlitInt;

static void MakeCopperList(CopListT *cp) {
  CopInsT *sprptr[8];
  WORD i;

  CopInit(cp);
  CopSetupGfxSimple(cp, MODE_LORES, DEPTH, X(0), Y(28), WIDTH * 2, HEIGHT * 2);
  CopSetupBitplanes(cp, bplptr, window0, DEPTH);
  CopSetupSprites(cp, sprptr);
  pal = CopLoadColor(cp, 0, 31, 0);
  for (i = 0; i < HEIGHT * 2; i++) {
    CopWait(cp, Y(i + 28), 0);
    /* Line doubling. */
    CopMove16(cp, bpl1mod, (i & 1) ? 0 : -40);
    CopMove16(cp, bpl2mod, (i & 1) ? 0 : -40);
#if !FULLPIXEL
    /* Alternating shift by one for bitplane data. */
    CopMove16(cp, bplcon1, (i & 1) ? 0x0010 : 0x0021);
#endif
  }
  CopEnd(cp);

  CopInsSet32(sprptr[0], sprite[0]->data);
  CopInsSet32(sprptr[1], sprite[0]->attached->data);
  CopInsSet32(sprptr[2], sprite[1]->data);
  CopInsSet32(sprptr[3], sprite[1]->attached->data);
}

static void Init() {
  static BitmapT recycled0, recycled1;

  window0 = &recycled0;
  window1 = &recycled1;

  InitSharedBitmap(window0, WIDTH * 2, HEIGHT * 2, DEPTH, screen0);
  InitSharedBitmap(window1, WIDTH * 2, HEIGHT * 2, DEPTH, screen1);

  cp = NewCopList(900);
  MakeCopperList(cp);
  CopListActivate(cp);

  custom->dmacon = DMAF_SETCLR | DMAF_RASTER | DMAF_BLITTER | DMAF_SPRITE;
  custom->intreq = INTF_BLIT;
  custom->intena = INTF_SETCLR | INTF_BLIT;

  oldBlitInt = SetIntVector(INTB_BLIT, &ChunkyToPlanarInterrupt);
  AddIntServer(INTB_VERTB, &FadeInFadeOutInterrupt);
}

static void Kill() {
  custom->dmacon = DMAF_COPPER | DMAF_RASTER | DMAF_BLITTER | DMAF_SPRITE;
  custom->dmacon = DMAF_COPPER | DMAF_RASTER | DMAF_BLITTER;

  custom->intena = INTF_BLIT;
  RemIntServer(INTB_VERTB, &FadeInFadeOutInterrupt);
  SetIntVector(INTB_BLIT, oldBlitInt);

  DeleteCopList(cp);
  DeletePixmap(textureHi);
  DeletePixmap(textureLo);
  MemFree(UVMapRender);
}

static void Render() {
  WORD u = TrackValueGet(pos_u, frameCount) & 127;
  WORD v = TrackValueGet(pos_v, frameCount) & 127;
  WORD offset = (u << 7) | v;
  UBYTE *txtHi = textureHi->pixels + offset;
  UBYTE *txtLo = textureLo->pixels + offset;
  (*UVMapRender)(window0->planes[0], txtHi, txtLo);

  c2p.phase = 0;
  c2p.bpl = window0->planes;
  ChunkyToPlanar();
  swapr(window0, window1);
}

EFFECT(UVMap, Load, UnLoad, Init, Kill, Render, Prepare);
