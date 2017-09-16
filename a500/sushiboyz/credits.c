#include "sushiboyz.h"
#include "hardware.h"
#include "coplist.h"
#include "gfx.h"
#include "ilbm.h"
#include "blitter.h"
#include "fx.h"
#include "sprite.h"
#include "png.h"
#include "interrupts.h"
#include "memory.h"
#include "io.h"
#include "tasks.h"

#define WIDTH 64
#define HEIGHT 64
#define DEPTH 4

static BitmapT *member[5], *logo, *floor, *dance[8], *ball;
static BitmapT *foreground, *background, *insert;
static CopListT *cp0, *cp1;
static SpriteT *sprite[4];
static PaletteT *floorPal, *dancePal, *texturePal, *memberPal, *insertPal, *logoPal;
static PaletteT *rotatedPal;
static CopInsT *pal = NULL;
static BOOL fadeActive = 1;
static BOOL flashActive = 0;

static BitmapT *lower;

static TrackT *credits_dance, *credits_phase, *pos_x, *sprite_active;

static UWORD *uvmap;
static PixmapT *texture, *textureHi, *textureLo, *chunky;

#define UVMapRenderSize (WIDTH * HEIGHT / 2 * 10 + 2)
void (*UVMapRender)(UBYTE *chunky asm("a0"),
                    UBYTE *textureHi asm("a1"),
                    UBYTE *textureLo asm("a2"));

static __regargs void PixmapToTexture(PixmapT *image, PixmapT *imageHi, PixmapT *imageLo)
{
  UBYTE *data = image->pixels;
  LONG size = image->width * image->height;
  /* Extra halves for cheap texture motion. */
  UWORD *hi0 = imageHi->pixels;
  UWORD *hi1 = imageHi->pixels + size;
  UWORD *lo0 = imageLo->pixels;
  UWORD *lo1 = imageLo->pixels + size;
  WORD n = size / 2;

  while (--n >= 0) {
    UBYTE a = *data++;
    UWORD b = ((a << 8) | (a << 1)) & 0xAAAA;
    /* [a0 b0 a1 b1 a2 b2 a3 b3] => [a0 -- a2 -- a1 -- a3 --] */
    *hi0++ = b;
    *hi1++ = b;
    /* [a0 b0 a1 b1 a2 b2 a3 b3] => [-- b0 -- b2 -- b1 -- b3] */
    *lo0++ = b >> 1;
    *lo1++ = b >> 1;
  }
}

static void MakeUVMapRenderCode() {
  UWORD *code = (APTR)UVMapRender;
  UWORD *data = uvmap;
  WORD n = WIDTH * HEIGHT / 2;
  WORD uv;

  while (n--) {
    if ((uv = *data++) >= 0) {
      *code++ = 0x1029;  /* 1029 xxxx | move.b xxxx(a1),d0 */
      *code++ = uv;
    } else {
      *code++ = 0x7000;  /* 7000      | moveq  #0,d0 */
    }
    if ((uv = *data++) >= 0) {
      *code++ = 0x802a;  /* 802a yyyy | or.b   yyyy(a2),d0 */
      *code++ = uv;
    }
    *code++ = 0x10c0;    /* 10c0      | move.b d0,(a0)+    */
  }

  *code++ = 0x4e75; /* rts */
}

static void Load() {
  WORD i;

  texture = LoadPNG("16_ball_texture.png", PM_CMAP4, MEMF_PUBLIC);
  texturePal = texture->palette;
  uvmap = LoadFile("16_ball.bin", MEMF_PUBLIC);

  logo = LoadILBMCustom("16_credits_logo.iff", BM_KEEP_PACKED|BM_LOAD_PALETTE);
  logoPal = logo->palette;
  floor = LoadILBMCustom("16_floor.iff", BM_KEEP_PACKED|BM_LOAD_PALETTE);
  floorPal = floor->palette;
  insert = LoadILBMCustom("16_insert_coin.iff", BM_LOAD_PALETTE|BM_KEEP_PACKED);
  insertPal = insert->palette;

  dance[0] = LoadILBMCustom("16_01_cahir.iff", BM_KEEP_PACKED|BM_LOAD_PALETTE);
  dance[1] = LoadILBMCustom("16_02_slayer.iff", BM_KEEP_PACKED);
  dance[2] = LoadILBMCustom("16_03_jazzcat.iff", BM_KEEP_PACKED);
  dance[3] = LoadILBMCustom("16_04_dkl.iff", BM_KEEP_PACKED);
  dance[4] = LoadILBMCustom("16_05_dance1.iff", BM_KEEP_PACKED);
  dance[5] = LoadILBMCustom("16_06_dance2.iff", BM_KEEP_PACKED);
  dance[6] = LoadILBMCustom("16_07_dance3.iff", BM_KEEP_PACKED);
  dance[7] = LoadILBMCustom("16_08_dance4.iff", BM_KEEP_PACKED);
  dancePal = dance[0]->palette;

  member[0] = LoadILBMCustom("16_txt_cahir.iff", BM_KEEP_PACKED|BM_LOAD_PALETTE);
  member[1] = LoadILBMCustom("16_txt_slay.iff", BM_KEEP_PACKED);
  member[2] = LoadILBMCustom("16_txt_jazz.iff", BM_KEEP_PACKED);
  member[3] = LoadILBMCustom("16_txt_dkl.iff", BM_KEEP_PACKED);
  member[4] = LoadILBMCustom("16_txt_codi.iff", BM_KEEP_PACKED);
  memberPal = member[0]->palette;

  for (i = 1; i < 5; i++)
    member[i]->palette = member[0]->palette;

  pos_x = TrackLookup(tracks, "credits.pos.x");
  credits_phase = TrackLookup(tracks, "credits.phase");
  credits_dance = TrackLookup(tracks, "credits.dancing");
  sprite_active = TrackLookup(tracks, "credits.sprite");
}

static void Prepare() {
  WORD i;

  BitmapUnpack(insert, 0);

  sprite[0] = NewSpriteFromBitmap(insert->height, insert, 0, 0);
  sprite[1] = NewSpriteFromBitmap(insert->height, insert, 16, 0);
  sprite[2] = NewSpriteFromBitmap(insert->height, insert, 32, 0);
  sprite[3] = NewSpriteFromBitmap(insert->height, insert, 48, 0);

  DeleteBitmap(insert); insert = NULL;

  UpdateSprite(sprite[0], X(128), Y(112));
  UpdateSprite(sprite[1], X(128) + 16, Y(112));
  UpdateSprite(sprite[2], X(128) + 32, Y(112));
  UpdateSprite(sprite[3], X(128) + 48, Y(112));

  for (i = 0; i < 8; i++)
    BitmapUnpack(dance[i], BM_DISPLAYABLE);

  BitmapUnpack(floor, 0);
}

static void UnLoad() {
  WORD i;

  DeletePalette(texturePal);

  DeletePalette(logoPal);
  DeleteBitmap(logo);
  DeletePalette(floorPal);
  
  DeletePalette(memberPal);
  for (i = 0; i < 5; i++)
    DeleteBitmap(member[i]);

  DeletePalette(dancePal);
  for (i = 0; i < 8; i++)
    DeleteBitmap(dance[i]);

  for (i = 0; i < 4; i++)
    DeleteSprite(sprite[i]);

  DeletePalette(insertPal);
}

#define DISCO_X X((320 - ball->width) / 2)
#define DISCO_Y Y(0)

#define LOGO_X X(0)
#define LOGO_Y Y(256 - 64)

#define FLOOR_X X((320 - background->width) / 2)
#define FLOOR_Y Y(64)

static void MakeCopperList(CopListT *cp, WORD lower_x) {
  static CopInsT *sprptr[8];

  CopInit(cp);
  CopSetupDisplayWindow(cp, MODE_LORES, X(0), Y(0), 320, 256); 
  CopSetupSprites(cp, sprptr);
  CopLoadPal(cp, insertPal, 16);
  CopLoadPal(cp, insertPal, 20);

  if (TrackValueGet(sprite_active, frameCount)) {
    CopInsSet32(sprptr[0], sprite[0]->data);
    CopInsSet32(sprptr[1], sprite[1]->data);
    CopInsSet32(sprptr[2], sprite[2]->data);
    CopInsSet32(sprptr[3], sprite[3]->data);
  }

  CopMove16(cp, dmacon, DMAF_RASTER);

  /* Display disco ball. */
  CopWait(cp, DISCO_Y - 1, 0);
  if (flashActive)
    CopLoadColor(cp, 0, 15, 0xfff);
  else
    CopLoadPal(cp, texturePal, 0);
  CopSetupMode(cp, MODE_LORES, ball->depth);
  CopSetupBitplanes(cp, NULL, ball, ball->depth);
  CopSetupBitplaneFetch(cp, MODE_LORES, DISCO_X, ball->width);

  CopWait(cp, DISCO_Y, 0);
  CopMove16(cp, dmacon, DMAF_SETCLR | DMAF_RASTER);
  CopWait(cp, DISCO_Y + ball->height, 0);
  CopMove16(cp, dmacon, DMAF_RASTER);

  /* Display logo & credits. */
  CopWait(cp, FLOOR_Y - 1, 0);
  if (fadeActive) {
    pal = CopLoadColor(cp, 0, 7, 0);
  } else {
    pal = NULL;
    if (flashActive)
      CopLoadColor(cp, 0, 7, 0xfff);
    else
      CopLoadPal(cp, rotatedPal, 0);
  }
  
  if (flashActive)
    CopLoadColor(cp, 0, 7, 0xfff);
  else
    CopLoadPal(cp, dancePal, 8);

  CopSetupMode(cp, MODE_DUALPF, 6);
  {
    APTR *planes0 = background->planes;
    APTR *planes1 = foreground->planes;
    WORD i;

    for (i = 0; i < 6;) {
      CopMove32(cp, bplpt[i++], *planes0++);
      CopMove32(cp, bplpt[i++], *planes1++);
    }

    CopMove16(cp, bpl1mod, 0);
    CopMove16(cp, bpl2mod, 0);
  }
  CopSetupBitplaneFetch(cp, MODE_LORES, FLOOR_X, background->width);

  CopWait(cp, FLOOR_Y, 0);
  CopMove16(cp, dmacon, DMAF_SETCLR | DMAF_RASTER);
  CopWait(cp, FLOOR_Y + background->height, 0);
  CopMove16(cp, dmacon, DMAF_RASTER);

  /* Display logo and textual credits. */
  {
    WORD xw = 80 - ((lower_x >> 3) & ~1);
    WORD xs = lower_x & 15;

    // Log("x: %ld\n", (LONG)lower_x);

    CopWaitSafe(cp, LOGO_Y - 1, 0);
    if (flashActive)
      CopLoadColor(cp, 0, 15, 0xfff);
    else
      CopLoadPal(cp, lower->palette, 0);

    CopSetupMode(cp, MODE_LORES, 4);
    CopMove32(cp, bplpt[0], lower->planes[0] + xw);
    CopMove32(cp, bplpt[1], lower->planes[1] + xw);
    CopMove32(cp, bplpt[2], lower->planes[2] + xw);
    CopMove32(cp, bplpt[3], lower->planes[3] + xw);
    CopSetupBitplaneFetch(cp, MODE_LORES, X(-8), 320 + 16);
    CopMove16(cp, bpl1mod, 384 / 8 - 2);
    CopMove16(cp, bpl2mod, 384 / 8 - 2);
    CopMove16(cp, bplcon1, (xs << 4) | xs);

    CopWaitSafe(cp, LOGO_Y, 0);
    CopMove16(cp, dmacon, DMAF_SETCLR | DMAF_RASTER);
    CopWaitSafe(cp, LOGO_Y + lower->height, 0);
    CopMove16(cp, dmacon, DMAF_RASTER);
  }

  CopEnd(cp);
}

static __interrupt LONG RunEachFrame() {
  UpdateFrameCount();

  if (frameFromStart < 32) {
    if (pal)
      FadeBlack(floorPal, pal, frameFromStart / 2);
  } else {
    fadeActive = 0;
  }

  return 0;
}

INTERRUPT(FrameInterrupt, 0, RunEachFrame, NULL);

static void Init() {
  static BitmapT recycled;

  EnableDMA(DMAF_BLITTER | DMAF_BLITHOG);

  lower = &recycled;

  InitSharedBitmap(lower, 320 + 384, 64, DEPTH, screen0);
  lower->palette = memberPal;
  BitmapClear(lower);

  rotatedPal = CopyPalette(floorPal);

  foreground = NewBitmap(320, 128, 3);
  background = NewBitmap(320, 128, 3);
  BitmapMakeDisplayable(floor);
  BitmapCopy(background, 16, 48, floor);
  DeleteBitmap(floor); floor = NULL;
  BitmapClear(foreground);

  ball = NewBitmap(WIDTH, HEIGHT, DEPTH);
  chunky = NewPixmap(WIDTH, HEIGHT, PM_GRAY4, MEMF_CHIP);

  cp0 = NewCopList(300);
  cp1 = NewCopList(300);
  MakeCopperList(cp0, 0);
  CopListActivate(cp0);
  EnableDMA(DMAF_RASTER | DMAF_SPRITE);

  AddIntServer(INTB_VERTB, FrameInterrupt);
}

static void Kill() {
  RemIntServer(INTB_VERTB, FrameInterrupt);

  DisableDMA(DMAF_COPPER | DMAF_RASTER | DMAF_BLITTER | DMAF_SPRITE |
             DMAF_BLITHOG);

  DeletePalette(rotatedPal);
  DeleteCopList(cp0);
  DeleteCopList(cp1);
  DeleteBitmap(foreground);
  DeleteBitmap(background);

  DeletePixmap(chunky);
  DeleteBitmap(ball);
  DeletePixmap(textureHi);
  DeletePixmap(textureLo);
  MemFree(UVMapRender);
}

#define BLTSIZE (WIDTH * HEIGHT / 2)

#if (BLTSIZE / 4) > 1024
#error "blit size too big!"
#endif

static __regargs void ChunkyToPlanar(PixmapT *input, BitmapT *output) {
  APTR planes = output->planes[0];
  APTR chunky = input->pixels;

  /* Swap 8x4, pass 1. */
  {
    WaitBlitter();

    /* (a & 0xFF00) | ((b >> 8) & ~0xFF00) */
    custom->bltcon0 = (SRCA | SRCB | DEST) | (ABC | ABNC | ANBC | NABNC);
    custom->bltcon1 = 8 << BSHIFTSHIFT;
    custom->bltafwm = -1;
    custom->bltalwm = -1;
    custom->bltamod = 4;
    custom->bltbmod = 4;
    custom->bltdmod = 4;
    custom->bltcdat = 0xFF00;

    custom->bltapt = chunky;
    custom->bltbpt = chunky + 4;
    custom->bltdpt = planes;
    custom->bltsize = 2 | ((BLTSIZE / 8) << 6);
  }

  /* Swap 8x4, pass 2. */
  {
    WaitBlitter();

    /* ((a << 8) & 0xFF00) | (b & ~0xFF00) */
    custom->bltcon0 = (SRCA | SRCB | DEST) | (ABC | ABNC | ANBC | NABNC) | (8 << ASHIFTSHIFT);
    custom->bltcon1 = BLITREVERSE;

    custom->bltapt = chunky + BLTSIZE - 6;
    custom->bltbpt = chunky + BLTSIZE - 2;
    custom->bltdpt = planes + BLTSIZE - 2;
    custom->bltsize = 2 | ((BLTSIZE / 8) << 6);
  }

  /* Swap 4x2, pass 1. */
  {
    WaitBlitter();

    /* (a & 0xF0F0) | ((b >> 4) & ~0xF0F0) */
    custom->bltcon0 = (SRCA | SRCB | DEST) | (ABC | ABNC | ANBC | NABNC);
    custom->bltcon1 = 4 << BSHIFTSHIFT;
    custom->bltamod = 2;
    custom->bltbmod = 2;
    custom->bltdmod = 2;
    custom->bltcdat = 0xF0F0;

    custom->bltapt = planes;
    custom->bltbpt = planes + 2;
    custom->bltdpt = chunky;
    custom->bltsize = 1 | ((BLTSIZE / 4) << 6);
  }

  /* Swap 4x2, pass 2. */
  {
    WaitBlitter();

    /* ((a << 4) & 0xF0F0) | (b & ~0xF0F0) */
    custom->bltcon0 = (SRCA | SRCB | DEST) | (ABC | ABNC | ANBC | NABNC) | (4 << ASHIFTSHIFT);
    custom->bltcon1 = BLITREVERSE;

    custom->bltapt = planes + BLTSIZE - 4;
    custom->bltbpt = planes + BLTSIZE - 2;
    custom->bltdpt = chunky + BLTSIZE - 2;
    custom->bltsize = 1 | ((BLTSIZE / 4) << 6);
  }

  /* Swap 2x1, pass 1 & 2. */
  {
    WaitBlitter();

    /* (a & 0xCCCC) | ((b >> 2) & ~0xCCCC) */
    custom->bltamod = 6;
    custom->bltbmod = 6;
    custom->bltdmod = 0;
    custom->bltcdat = 0xCCCC;
    custom->bltcon0 = (SRCA | SRCB | DEST) | (ABC | ABNC | ANBC | NABNC);
    custom->bltcon1 = 2 << BSHIFTSHIFT;

    custom->bltapt = chunky;
    custom->bltbpt = chunky + 4;
    custom->bltdpt = planes + BLTSIZE * 3 / 4;
    custom->bltsize = 1 | ((BLTSIZE / 8) << 6);

    WaitBlitter();
    custom->bltapt = chunky + 2;
    custom->bltbpt = chunky + 6;
    custom->bltdpt = planes + BLTSIZE * 2 / 4;
    custom->bltsize = 1 | ((BLTSIZE / 8) << 6);
  }

  /* Swap 2x1, pass 3 & 4. */
  {
    WaitBlitter();

    /* ((a << 2) & 0xCCCC) | (b & ~0xCCCC) */
    custom->bltcon0 = (SRCA | SRCB | DEST) | (ABC | ABNC | ANBC | NABNC) | (2 << ASHIFTSHIFT);
    custom->bltcon1 = BLITREVERSE;

    custom->bltapt = chunky + BLTSIZE - 8;
    custom->bltbpt = chunky + BLTSIZE - 4;
    custom->bltdpt = planes + BLTSIZE * 2 / 4 - 2;
    custom->bltsize = 1 | ((BLTSIZE / 8) << 6);

    WaitBlitter();
    custom->bltapt = chunky + BLTSIZE - 6;
    custom->bltbpt = chunky + BLTSIZE - 2;
    custom->bltdpt = planes + BLTSIZE * 1 / 4 - 2;
    custom->bltsize = 1 | ((BLTSIZE / 8) << 6);
  }
}

static void Render() {
  WORD event = TrackValueGet(credits_phase, frameCount);
  BOOL dancing = TrackValueGet(credits_dance, frameCount);

  switch (event) {
    case 1:
      {
        WORD i;

        UVMapRender = MemAlloc(UVMapRenderSize, MEMF_PUBLIC);
        MakeUVMapRenderCode();
        MemFree(uvmap);

        EffectPrepare(&Watchmaker);

        textureHi = NewPixmap(texture->width, texture->height * 2,
                              PM_CMAP8, MEMF_PUBLIC);
        textureLo = NewPixmap(texture->width, texture->height * 2,
                              PM_CMAP8, MEMF_PUBLIC);
        PixmapScramble_4_1(texture);
        PixmapToTexture(texture, textureHi, textureLo);
        DeletePixmap(texture);

        BitmapUnpack(logo, BM_DISPLAYABLE);

        for (i = 0; i < 5; i++)
          BitmapUnpack(member[i], 0);
      }
      break;

    case 2:
      BitmapMakeDisplayable(member[0]);
      lower->palette = memberPal;
      BitmapClear(lower);
      BitmapCopy(lower, 320, 0, member[0]);
      BitmapCopy(foreground, 80, 0, dance[0]);
      DeleteBitmap(dance[0]); dance[0] = NULL;
      DeleteBitmap(member[0]); member[0] = NULL;
      break;

    case 3:
      lower->palette = memberPal;
      BitmapMakeDisplayable(member[1]);
      BitmapClear(lower);
      BitmapCopy(lower, 320, 0, member[1]);
      BitmapCopy(foreground, 80, 0, dance[1]);
      DeleteBitmap(dance[1]); dance[1] = NULL;
      DeleteBitmap(member[1]); member[1] = NULL;
      break;

    case 4:
      lower->palette = memberPal;
      BitmapMakeDisplayable(member[2]);
      BitmapClear(lower);
      BitmapCopy(lower, 320, 0, member[2]);
      BitmapCopy(foreground, 80, 0, dance[2]);
      DeleteBitmap(dance[2]); dance[2] = NULL;
      DeleteBitmap(member[2]); member[2] = NULL;
      break;

    case 5:
      lower->palette = memberPal;
      BitmapMakeDisplayable(member[3]);
      BitmapClear(lower);
      BitmapCopy(lower, 320, 0, member[3]);
      BitmapCopy(foreground, 80, 0, dance[3]);
      DeleteBitmap(dance[3]); dance[3] = NULL;
      DeleteBitmap(member[3]); member[3] = NULL;
      break;

    case 6:
      lower->palette = memberPal;
      BitmapMakeDisplayable(member[4]);
      BitmapClear(lower);
      BitmapCopy(lower, 320, 0, member[4]);
      BitmapCopy(foreground, 80, 0, dance[4]);
      DeleteBitmap(member[4]); member[4] = NULL;
      break;

    case 7:
      lower->palette = logoPal;
      BitmapMakeDisplayable(logo);
      BitmapClear(lower);
      BitmapCopy(lower, 320, 0, logo);
      DeleteBitmap(logo); logo = NULL;
      flashActive = 1;
      break;

    default:
      break;
  }

  if (dancing) {
    WORD i = div16(frameCount, 12) & 3;

    BitmapCopy(foreground, 80, 0, dance[i + 4]);
    RotatePalette(rotatedPal, floorPal, 1, 7, - frameCount / 4);
  }

  if (dancing) {
    WORD offset = (frameCount & 127) + 128 * 64;
    UBYTE *txtHi = textureHi->pixels + offset;
    UBYTE *txtLo = textureLo->pixels + offset;

    (*UVMapRender)(chunky->pixels, txtHi, txtLo);
    ChunkyToPlanar(chunky, ball);
  }

#if 0
  {
    static const Box2D window = { 0, 0, 319, 255 }; 
    BitmapT *bm = lower;

    lower_pos.x = 0;
    lower_pos.y = 256 - 64;
    lower_area.x = 160 + (SIN(frameCount * 32) >> 6); // + TrackValueGet(pos_x, frameCount);
    lower_area.y = 0;
    lower_area.w = 320;
    lower_area.h = 64;

    if (!ClipBitmap(&window, &lower_pos, &lower_area))
      bm = NULL;

    MakeCopperList(cp1, bm);
  }
#endif
  MakeCopperList(cp1, 160 + TrackValueGet(pos_x, frameCount));

  CopListRun(cp1);

  TaskWait(VBlankEvent);
  swapr(cp0, cp1);
  flashActive = 0;
}

EFFECT(Credits, Load, UnLoad, Init, Kill, Render, Prepare);
