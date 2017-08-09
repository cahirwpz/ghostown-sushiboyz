#include "sushiboyz.h"
#include "blitter.h"
#include "coplist.h"
#include "ilbm.h"
#include "png.h"
#include "fx.h"
#include "memory.h"
#include "sprite.h"
#include "random.h"
#include "color.h"

#define WIDTH   144
#define HEIGHT  255
#define DEPTH   5
#define STARTX  96

#define STRIPES 16
#define SIZE    128
#define BGCOL   0x000

static BitmapT *twister;
static PixmapT *texture;

typedef struct {
  CopListT *cp;
  CopInsT *bplptr[DEPTH];
  CopInsT *bplmod[HEIGHT];
  CopInsT *colors[HEIGHT];
  CopInsT *lineColor[HEIGHT];
} FrameT;

static FrameT *frame0, *frame1;

static TrackT *twister_rotate, *twister_scroll, *stripes_rotate;

typedef struct {
  WORD y, z;
  WORD color;
} StripeT;

static StripeT stripe[STRIPES];
static StripeT rotatedStripes[STRIPES];

static UWORD colorSet[4] = { 0x701, 0xCB9, 0x456, 0x0BD };
static UWORD colorShades[4 * 32];
static WORD *rotation;

static void GenerateStripes() {
  WORD *s = (WORD *)stripe;
  WORD n = STRIPES;

  while (--n >= 0) {
    *s++ = (random() & (SIZE - 1)) - SIZE / 2;
    *s++ = (random() & (SIZE - 1)) - SIZE / 2;
    *s++ = random() & 0x60;
  }
}

static void GenerateColorShades() {
  WORD i, j;
  UWORD *s = colorSet;
  UWORD *d = colorShades;

  for (i = 0; i < 4; i++) {
    UWORD c = *s++;

    for (j = 0; j < 16; j++)
      *d++ = ColorTransition(0x000, c, j);
    for (j = 0; j < 16; j++)
      *d++ = ColorTransition(c, 0xfff, j);
  }
}

static void GenerateStripeRotate() {
  WORD *table = rotation;
  WORD i;

  for (i = 0; i < 4096; i++)
    *table++ = ((COS(i) >> 3) & 255) * WIDTH / 8;
}

static void Load() {
  twister = LoadILBMCustom("18_twister.iff", BM_DISPLAYABLE);
  texture = LoadPNG("18_twister_texture.png", PM_RGB12, MEMF_PUBLIC);

  twister_scroll = TrackLookup(tracks, "twister.scroll");
  twister_rotate = TrackLookup(tracks, "twister.rotate");
  stripes_rotate = TrackLookup(tracks, "twister.stripes.rotate");
}

static void Prepare() {
  rotation = MemAlloc(4096 * sizeof(WORD), MEMF_PUBLIC);
  GenerateStripes();
  GenerateColorShades();
  GenerateStripeRotate();
}

static void UnLoad() {
  DeleteBitmap(twister);
  DeletePixmap(texture);
  MemFree(rotation);
}

static FrameT *NewFrame() {
  FrameT *frame = MemAlloc(sizeof(FrameT), MEMF_PUBLIC|MEMF_CLEAR);

  frame->cp = NewCopList(100 + HEIGHT * 5 + (31 * HEIGHT / 3));

  {
    WORD *pixels = texture->pixels;
    CopListT *cp = frame->cp;
    WORD i, j, k;

    CopInit(cp);
    CopSetupGfxSimple(cp, MODE_LORES, DEPTH, X(STARTX), Y(0), WIDTH, HEIGHT);
    CopSetupBitplanes(cp, frame->bplptr, twister, DEPTH);
    CopMove16(cp, dmacon, DMAF_SETCLR|DMAF_RASTER);
    CopMove16(cp, diwstrt, 0x2c81);
    CopMove16(cp, diwstop, 0x2bc1);

    for (i = 0, k = 0; i < HEIGHT; i++) {
      CopWait(cp, Y(i), 0);
      frame->bplmod[i] = CopMove16(cp, bpl1mod, -32);
      CopMove16(cp, bpl2mod, -32);
      CopMove16(cp, bpldat[0], 0);
      frame->lineColor[i] = CopSetRGB(cp, 0, BGCOL);

      if ((i % 3) == 0) {
        frame->colors[k++] = CopSetRGB(cp, 1, *pixels++);
        for (j = 2; j < 32; j++)
          CopSetRGB(cp, j, *pixels++);
      }
    }

    CopEnd(cp);
  }

  return frame;
}

static void DeleteFrame(FrameT *frame) {
  DeleteCopList(frame->cp);
  MemFree(frame);
}

static void Init() {
  frame0 = NewFrame();
  frame1 = NewFrame();

  CopListActivate(frame1->cp);
  custom->dmacon = DMAF_SETCLR | DMAF_RASTER;
}

static void Kill() {
  custom->dmacon = DMAF_COPPER | DMAF_RASTER | DMAF_BLITTER;

  DeleteFrame(frame0);
  DeleteFrame(frame1);
}

static void SetupLines(FrameT *frame) {
  WORD f = TrackValueGet(twister_rotate, frameCount) * 4;
  WORD y0 = rotation[f & SIN_MASK];

  /* first line */
  {
    APTR *planes = twister->planes;
    CopInsT **bpl = frame->bplptr;
    WORD n = DEPTH;

    while (--n >= 0)
      CopInsSet32(*bpl++, (*planes++) + y0);
  }

  /* consecutive lines */
  {
    WORD **modptr = (WORD **)frame->bplmod;
    APTR rotate = rotation;
    WORD y1, i, m;

    for (i = 1; i < HEIGHT; i++, y0 = y1, f += 2) {
      WORD *mod = *modptr++;

      f &= SIN_MASK;
      y1 = *(WORD *)(rotate + (WORD)(f + f));
      m = (y1 - y0) - (WIDTH / 8);

      mod[1] = m;
      mod[3] = m;
    }
  }
}

static __regargs void SetupTexture(CopInsT **colors) {
  WORD *pixels = texture->pixels;
  WORD height = texture->height;
  WORD width = texture->width;
  WORD n = height;
  WORD y = TrackValueGet(twister_scroll, frameCount);

  y = mod16(y, height);

  pixels += y * width;

  while (--n >= 0) {
    WORD *ins = (WORD *)(*colors++) + 1;

    ins[0] = *pixels++;
    ins[2] = *pixels++;
    ins[4] = *pixels++;
    ins[6] = *pixels++;
    ins[8] = *pixels++;
    ins[10] = *pixels++;
    ins[12] = *pixels++;
    ins[14] = *pixels++;
    ins[16] = *pixels++;
    ins[18] = *pixels++;
    ins[20] = *pixels++;
    ins[22] = *pixels++;
    ins[24] = *pixels++;
    ins[26] = *pixels++;
    ins[28] = *pixels++;
    ins[30] = *pixels++;
    ins[32] = *pixels++;
    ins[34] = *pixels++;
    ins[36] = *pixels++;
    ins[38] = *pixels++;
    ins[40] = *pixels++;
    ins[42] = *pixels++;
    ins[44] = *pixels++;
    ins[46] = *pixels++;
    ins[48] = *pixels++;
    ins[50] = *pixels++;
    ins[52] = *pixels++;
    ins[54] = *pixels++;
    ins[56] = *pixels++;
    ins[58] = *pixels++;
    ins[60] = *pixels++;

    y++;

    if (y >= height) {
      pixels = texture->pixels;
      y = 0;
    }
  }
}

static WORD centerY = 0;
static WORD centerZ = 224;

static void RotateStripes(WORD *d, WORD *s, WORD rotate) {
  WORD n = STRIPES;
  LONG cy = centerY << 8;
  WORD cz = centerZ;

  while (--n >= 0) {
    WORD sinA = SIN(rotate);
    WORD cosA = COS(rotate);
    WORD y = *s++;
    WORD z = *s++;
    LONG yp = (y * cosA - z * sinA) >> 4; 
    WORD zp = normfx(y * sinA + z * cosA);

    *d++ = div16(yp + cy, zp + cz);
    *d++ = zp;
    *d++ = *s++;
  }
}

static void ClearLineColor(FrameT *frame) {
  CopInsT **line = frame->lineColor;
  WORD n = HEIGHT / 4;

  while (--n >= 0) {
    CopInsSet16(*line++, BGCOL);
    CopInsSet16(*line++, BGCOL);
    CopInsSet16(*line++, BGCOL);
    CopInsSet16(*line++, BGCOL);
  }
}

static void SetLineColor(FrameT *frame, WORD *s) {
  CopInsT **lines = frame->lineColor;
  WORD n = STRIPES;
  APTR shades = colorShades;

  while (--n >= 0) {
    WORD y = *s++;
    WORD z = *s++;
    UWORD color = *s++;

    WORD h = (WORD)(z + 128) >> 5;
    WORD l = (z >> 2) + 16;
    WORD i = y + ((WORD)(HEIGHT - h) >> 1);

    if (l < 0)
      l = 0;
    if (l > 31)
      l = 31;

    {
      CopInsT **line = &lines[i];
      WORD c0 = *(WORD *)(shades + (WORD)((color | l) << 1));
      WORD c1 = *(WORD *)(shades + (WORD)((color | (l >> 1)) << 1));

      h -= 2;

      CopInsSet16(*line++, c1);

      while (--h >= 0)
        CopInsSet16(*line++, c0);

      CopInsSet16(*line++, c1);
    }
  }
}

static void SortStripes(StripeT *table) {
  StripeT *ptr = table + 1;
  register WORD n asm("d7") = STRIPES - 2;

  do {
    StripeT *curr = ptr;
    StripeT *prev = ptr - 1;
    StripeT this = *ptr++;
    while (prev >= table && prev->z > this.z)
      *curr-- = *prev--;
    *curr = this;
  } while (--n != -1);
}

static void Render() {
  WORD rotate = TrackValueGet(stripes_rotate, frameCount) * 4;

  // PROFILE_BEGIN(twister);
  SetupLines(frame0);
  SetupTexture(frame0->colors);
  RotateStripes((WORD *)rotatedStripes, (WORD *)stripe, rotate);
  SortStripes(rotatedStripes);
  ClearLineColor(frame0);
  SetLineColor(frame0, (WORD *)rotatedStripes);
  // PROFILE_END(twister);

  WaitVBlank();
  CopListActivate(frame0->cp);
  swapr(frame0, frame1);
}

EFFECT(Twister, Load, UnLoad, Init, Kill, Render, Prepare);
