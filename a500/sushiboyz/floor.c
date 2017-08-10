#include "sushiboyz.h"
#include "blitter.h"
#include "coplist.h"
#include "gfx.h"
#include "ilbm.h"
#include "png.h"
#include "memory.h"
#include "fx.h"
#include "color.h"
#include "random.h"
#include "sprite.h"

#define WIDTH 320
#define HEIGHT 256
#define DEPTH 4

#define FAR   4
#define NEAR 16

typedef struct {
  CopListT *cp;
  SpriteT *sprite[2];
  CopInsT *sprptr[8];
  CopInsT *line[HEIGHT];
} FrameT;

static FrameT *frame0, *frame1;

static WORD *stripeWidth;
static WORD *stripeLight;
static WORD stripeColor[] = { 0x27B, 0xF71, 0x4BE, 0x27B, 0x456, 0x4BE, 0x27B, 0xF71 };
static BitmapT *ball, *floor;
static TrackT *ball_x, *ball_y, *floor_x;

typedef struct {
  WORD step, orig, color;
} StripeT;

static StripeT stripe[15];
static WORD rotated[15];
static UBYTE *table;

static void GenerateStripeLight() {
  WORD *light = stripeLight;
  WORD level = 11;
  WORD i;

  for (i = 0; i < HEIGHT / 2; i++)
    *light++ = level;
  for (i = 0; i < HEIGHT / 2; i++)
    *light++ = level - (12 * i) / (HEIGHT / 2);
}

static void GenerateStripeWidth() {
  WORD *width = stripeWidth;
  WORD i;

  for (i = 0; i < HEIGHT / 2; i++)
    *width++ = (FAR << 4);
  for (i = 0; i < HEIGHT / 2; i++)
    *width++ = (FAR << 4) + ((i << 4) * (NEAR - FAR)) / (HEIGHT / 2);
}

static void GenerateTable() {
  UBYTE *data = table;
  WORD i, j;

  for (j = 0; j < 16; j++) {
    for (i = 0; i < 256; i++) {
      WORD s = 1 + ((i * j) >> 8);
      *data++ = (s << 4) | s;
    }
  }
}

static void InitStripes() {
  StripeT *s = stripe;
  WORD n = 15;

  while (--n >= 0) {
    s->step = -16 * (random() & 7);
    s->orig = stripeColor[0];
    s->color = 0;
    s++;
  }
}

static void Load() {
  ball = LoadILBMCustom("06_ball.iff", BM_KEEP_PACKED|BM_LOAD_PALETTE);
  floor = LoadILBMCustom("07_floor.iff", BM_DISPLAYABLE);

  ball_x = TrackLookup(tracks, "floor.ball.x");
  ball_y = TrackLookup(tracks, "floor.ball.y");
  floor_x = TrackLookup(tracks, "floor.x");

  table = MemAlloc(4096, MEMF_PUBLIC);
  stripeWidth = MemAlloc(HEIGHT * sizeof(UWORD), MEMF_PUBLIC);
  stripeLight = MemAlloc(HEIGHT * sizeof(UWORD), MEMF_PUBLIC);

  GenerateTable();
  GenerateStripeLight();
  GenerateStripeWidth();
  InitStripes();
}

static void Prepare() {
  BitmapUnpack(ball, BM_DISPLAYABLE);
}

static void UnLoad() {
  DeleteBitmap(floor);
  DeletePalette(ball->palette);
  DeleteBitmap(ball);
  MemFree(table);
  MemFree(stripeWidth);
  MemFree(stripeLight);
}

static FrameT *NewFrame() {
  FrameT *frame = MemAlloc(sizeof(FrameT), MEMF_PUBLIC|MEMF_CLEAR);
  CopListT *cp = frame->cp = NewCopList(200 + 2 * HEIGHT + 15 * HEIGHT / 8);
  CopInsT **line = frame->line;
  WORD i, j;

  CopInit(cp);
  CopSetupMode(cp, MODE_LORES, DEPTH);
  CopSetupDisplayWindow(cp, MODE_LORES, X(16), Y(0), WIDTH - 16, HEIGHT);
  CopSetupBitplaneFetch(cp, MODE_LORES, X(0), WIDTH);
  CopSetupBitplanes(cp, NULL, floor, DEPTH);
  CopMove16(cp, bpl1mod, 2);
  CopMove16(cp, bpl2mod, 2);
  CopSetupSprites(cp, frame->sprptr);
  CopLoadPal(cp, ball->palette, 16);
  CopSetRGB(cp, 0, 0);
  for (i = 0; i < HEIGHT; i++) {
    CopWait(cp, Y(i), 0);
    line[i] = CopMove16(cp, bplcon1, 0);

    if ((i & 7) == 0) {
      for (j = 1; j < 16; j++)
        CopSetRGB(cp, j, 0);
    }
  }
  CopEnd(cp);

  frame->sprite[0] = NewSprite(32, TRUE);
  frame->sprite[1] = NewSprite(32, TRUE);

  return frame;
}

static void DeleteFrame(FrameT *frame) {
  DeleteCopList(frame->cp);
  DeleteSprite(frame->sprite[0]);
  DeleteSprite(frame->sprite[1]);
  MemFree(frame);
}

static void Init() {
  frame0 = NewFrame();
  frame1 = NewFrame();

  CopListActivate(frame0->cp);
  EnableDMA(DMAF_RASTER | DMAF_SPRITE | DMAF_BLITTER);
}

static void Kill() {
  DisableDMA(DMAF_RASTER | DMAF_COPPER | DMAF_SPRITE | DMAF_BLITTER);

  DeleteFrame(frame0);
  DeleteFrame(frame1);
}

static void ShiftColors(WORD offset) {
  WORD *dst = rotated;
  WORD n = 15;
  WORD i = 0;

  offset = mod16(offset / 16, 15);

  while (--n >= 0) {
    WORD c = i++ - offset;
    if (c < 0)
      c += 15;
    *dst++ = stripe[c].color;
  }
}

static __regargs void ColorizeStripes(CopInsT **stripeLine) {
  WORD i;

  for (i = 1; i < 16; i++) {
    CopInsT **line = stripeLine;
    WORD *light = stripeLight;
    WORD n = HEIGHT / 8;
    WORD r, g, b;

    {
      WORD s = rotated[i - 1];

      r = s & 0xf00;
      s <<= 4;
      g = s & 0xf00;
      s <<= 4;
      b = s & 0xf00;
    }

    while (--n >= 0) {
      UBYTE *tab = colortab + (*light);
      WORD color = (tab[r] << 4) | (UBYTE)(tab[g] | (tab[b] >> 4));

      CopInsSet16(*line + i, color);

      line += 8; light += 8;
    }
  }
}

static __regargs void ShiftStripes(CopInsT **line, WORD offset) {
  WORD *width = stripeWidth;
  UBYTE *data = table;
  UBYTE *ptr;
  WORD n = HEIGHT;

  offset = (offset & 15) << 8;
  data += offset;

  while (--n >= 0) {
    ptr = (UBYTE *)(*line++);
    ptr[3] = data[*width++];
  }
}

static void ControlStripes() {
  StripeT *s = stripe;
  WORD diff = frameCount - lastFrameCount;
  WORD n = 15;

  while (--n >= 0) {
    s->step -= diff * 2;

    if (s->step >= 0) {
      WORD step = s->step / 8;
      WORD from, to;

      if (step > 15) {
        from = s->orig;
        to = 0xfff;
        step -= 16;
      } else {
        from = 0x000;
        to = s->orig;
      }
      s->color = ColorTransition(from, to, step);
    }

    s++;
  }
}

static __regargs void BitmapToSprite(BitmapT *input, SpriteT **sprite, WORD frame) {
  APTR planes = input->planes[0];
  WORD bltsize = (input->height << 6) | 1;
  WORD i;

  WaitBlitter();

  custom->bltafwm = -1;
  custom->bltalwm = -1;
  custom->bltcon0 = (SRCA | DEST) | A_TO_D;
  custom->bltcon1 = 0;
  custom->bltamod = (input->width - 16) / 8;
  custom->bltdmod = 2;

  for (i = frame * 2; i < (frame + 1) * 2; i++) {
    SpriteT *spr = *sprite++;

    WaitBlitter();
    custom->bltapt = planes + i * sizeof(WORD);
    custom->bltdpt = &spr->data[2];
    custom->bltsize = bltsize;

    WaitBlitter();
    custom->bltdpt = &spr->data[3];
    custom->bltsize = bltsize;

    WaitBlitter();
    custom->bltdpt = &spr->attached->data[2];
    custom->bltsize = bltsize;

    WaitBlitter();
    custom->bltdpt = &spr->attached->data[3];
    custom->bltsize = bltsize;
  }
}

static __regargs void PositionSprite(FrameT *frame, WORD x, WORD y) {
  SpriteT **sprite = frame->sprite;
  CopInsT **ptr = frame->sprptr;
  WORD n = 2;

  while (--n >= 0) {
    SpriteT *spr = *sprite++;

    UpdateSprite(spr, x, y);

    CopInsSet32(*ptr++, spr->data);
    CopInsSet32(*ptr++, spr->attached->data);

    x += 16;
  }
}

static void Render() {
  // PROFILE_BEGIN(floor);
  {
    WORD offset = TrackValueGet(floor_x, frameCount) + 1024; // normfx(SIN(frameCount * 8) * 1024) + 1024;
    WORD x = TrackValueGet(ball_x, frameCount);
    WORD y = TrackValueGet(ball_y, frameCount);

    {
      WORD b = SIN(frameFromStart * 4096 / 48) >> 6;
      WORD f = mod16((-offset) >> 4, 13);

      if (b < 0)
        b = -b;

      y -= b;

      if (f < 0)
        f += 13;

      if (b == 0) {
        WORD s = div16((x + 16) << 4, stripeWidth[y + 32]);   // ball tile offset
        WORD p = div16(offset << 4, stripeWidth[HEIGHT - 1]); // screen tile offset
        WORD color = stripeColor[random() & 7];

        s = mod16(s - p, 15);
        if (s < 0)
          s += 15;

        /* s \in [0, 14] */

        {
          WORD prev = s - 1;
          WORD next = s + 1;

          if (prev < 0)
            prev += 15;

          if (next > 14)
            next -= 15;

          stripe[prev].step = 128;
          stripe[prev].orig = color;
          stripe[s].step = 256;
          stripe[s].orig = color;
          stripe[next].step = 128;
          stripe[next].orig = color;
        }
      }

      PositionSprite(frame0, X(x), Y(y));
      BitmapToSprite(ball, frame0->sprite, f);
    }

    {
      CopInsT **line = frame0->line;
      ControlStripes();
      ShiftColors(offset);
      ColorizeStripes(line);
      ShiftStripes(line, offset);
    }
  }
  // PROFILE_END(floor);

  WaitVBlank();
  CopListActivate(frame0->cp);
  swapr(frame0, frame1);
}

EFFECT(Floor, Load, UnLoad, Init, Kill, Render, Prepare);
