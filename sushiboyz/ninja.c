#include "sushiboyz.h"
#include "hardware.h"
#include "coplist.h"
#include "gfx.h"
#include "ilbm.h"
#include "blitter.h"
#include "2d.h"
#include "interrupts.h"
#include "tasks.h"

#define WIDTH 256
#define HEIGHT 144
#define DEPTH 4

#define STARTX X((320 - WIDTH) / 2)
#define STARTY Y((256 - HEIGHT) / 2)

static BitmapT *window0, *window1, *background;
static BitmapT *anim[4], *head, *eyes, *standing;
static BitmapT *eyeLeft[4], *eyeRight[4], *bigmask, *bigeyes;
static CopListT *cp;
static CopInsT *bplptr[DEPTH], *pal;

static TrackT *ninja_phase, *ninja_eyes;

static UWORD pissedPal[6];

static void Load1() {
  background = LoadILBMCustom("01_ninja_bkg_sold.iff", BM_DISPLAYABLE|BM_LOAD_PALETTE);
  anim[0] = LoadILBMCustom("01_ninja_anim1.iff", BM_DISPLAYABLE);
  anim[1] = LoadILBMCustom("01_ninja_anim2.iff", BM_DISPLAYABLE);
  anim[2] = LoadILBMCustom("01_ninja_anim3.iff", BM_DISPLAYABLE);
  anim[3] = LoadILBMCustom("01_ninja_anim4.iff", BM_DISPLAYABLE);
  standing = LoadILBMCustom("01_ninja_standing.iff", BM_DISPLAYABLE);

  ninja_eyes = TrackLookup(tracks, "ninja.eyes");
  ninja_phase = TrackLookup(tracks, "ninja.phase");
}

static void Load2() {
  head = LoadILBMCustom("02_ninja_leb_maska.iff", BM_DISPLAYABLE);
  eyes = LoadILBMCustom("02_ninja_oczy_male.iff", BM_DISPLAYABLE);
  eyeLeft[0] = LoadILBMCustom("03_oko_lewe_lewo.iff", BM_DISPLAYABLE);
  eyeLeft[1] = LoadILBMCustom("03_oko_lewe_lewo_s.iff", BM_DISPLAYABLE);
  eyeLeft[2] = LoadILBMCustom("03_oko_lewe_prawo_s.iff", BM_DISPLAYABLE);
  eyeLeft[3] = LoadILBMCustom("03_oko_lewe_prawo.iff", BM_DISPLAYABLE);
  eyeRight[0] = LoadILBMCustom("03_oko_prawe_lewo.iff", BM_DISPLAYABLE);
  eyeRight[1] = LoadILBMCustom("03_oko_prawe_lewo_s.iff", BM_DISPLAYABLE);
  eyeRight[2] = LoadILBMCustom("03_oko_prawe_prawo_s.iff", BM_DISPLAYABLE);
  eyeRight[3] = LoadILBMCustom("03_oko_prawe_prawo.iff", BM_DISPLAYABLE);
}
  
static void Load3() {
  PaletteT *pissed = LoadPalette("04_ninja_wkurw.iff");
  ConvertPaletteToRGB4(pissed, pissedPal, 6);
  DeletePalette(pissed);

  bigmask = LoadILBMCustom("04_ninja_big_maska.iff", BM_DISPLAYABLE);
  bigeyes = LoadILBMCustom("04_ninja_oczy_duze.iff", BM_DISPLAYABLE);
}

static void UnLoad() {
  WORD i;

  DeletePalette(background->palette);
  DeleteBitmap(background);
  DeleteBitmap(standing);
  
  DeleteBitmap(eyes);
  DeleteBitmap(head);

  for (i = 0; i < 4; i++) {
    DeleteBitmap(anim[i]);
  }

  for (i = 0; i < 4; i++) {
    DeleteBitmap(eyeLeft[i]);
    DeleteBitmap(eyeRight[i]);
  }

  DeleteBitmap(bigeyes);
  DeleteBitmap(bigmask);
}

static __interrupt LONG RunEachFrame() {
  UpdateFrameCount();

  if (frameFromStart < 16) {
    FadeBlack(background->palette, pal, frameFromStart);
  } else if (frameTillEnd < 16) {
    FadeBlack(background->palette, pal, frameTillEnd);
  }

  return 0;
}

INTERRUPT(FrameInterrupt, 0, RunEachFrame, NULL);

static void Init() {
  static BitmapT recycled0, recycled1;

  window0 = &recycled0;
  window1 = &recycled1;

  InitSharedBitmap(window0, WIDTH, HEIGHT, DEPTH, screen0);
  InitSharedBitmap(window1, WIDTH, HEIGHT, DEPTH, screen1);

  EnableDMA(DMAF_BLITTER);

  BitmapClear(window0);
  BitmapClear(window1);

  BitmapCopy(window0, 0, 0, background);
  BitmapCopy(window1, 0, 0, background);

  cp = NewCopList(200);
  CopInit(cp);
  CopSetupGfxSimple(cp, MODE_LORES, DEPTH, STARTX, STARTY, WIDTH, HEIGHT);
  CopSetupBitplanes(cp, bplptr, window0, DEPTH);
  pal = CopLoadColor(cp, 0, 31, 0);
  CopEnd(cp);

  CopListActivate(cp);
  EnableDMA(DMAF_RASTER);

  AddIntServer(INTB_VERTB, FrameInterrupt);
}

static void Kill() {
  RemIntServer(INTB_VERTB, FrameInterrupt);

  DisableDMA(DMAF_RASTER);

  DeleteCopList(cp);
}

static void SwapScreen() {
  CopUpdateBitplanes(bplptr, window0, DEPTH);
  TaskWait(VBlankEvent);
  swapr(window0, window1);
}

static Box2D clipWindow = { 0, 0, WIDTH - 1, HEIGHT - 1 };

static void Render1() {
  static WORD count = 0;
  static WORD frame = 0;
  static WORD dir = 1;

  BitmapCopy(window0, 16, 80, standing);
  BitmapCopy(window0, 16, 80, anim[frame]);

  count += frameCount - lastFrameCount;

  if (count > 5) {
    frame += dir;
    if (frame < 1)
      dir = 1;
    if (frame > 2)
      dir = -1;
    count -= 5;
  }

  SwapScreen();
}

static void Init2() {
  BitmapCopy(window0, 0, 0, background);
  BitmapCopy(window1, 0, 0, background);
}

static void Render2() {
  switch (TrackValueGet(ninja_phase, frameCount)) {
    case 2:
      {
        Point2D headPos = { 64, max(HEIGHT - (frameCount - 200), 10) };
        Area2D headArea = { 0, 0, head->width, head->height };

        if (ClipBitmap(&clipWindow, &headPos, &headArea)) {
          Area2D backgroundArea = { headPos.x, headPos.y, headArea.w, headArea.h };

          BitmapCopyArea(window0, headPos.x, headPos.y, background, &backgroundArea);
          BlitterSetMaskArea(window0, 0, headPos.x, headPos.y, head, &headArea, 0);
          BlitterSetMaskArea(window0, 1, headPos.x, headPos.y, head, &headArea, 0);
          BlitterSetMaskArea(window0, 2, headPos.x, headPos.y, head, &headArea, 0);
          BlitterSetMaskArea(window0, 3, headPos.x, headPos.y, head, &headArea, -1);
        }

        {
          Point2D eyesPos = { headPos.x + 8, headPos.y + 70 };
          Area2D eyesArea = { 0, 0, eyes->width, eyes->height };

          if (ClipBitmap(&clipWindow, &eyesPos, &eyesArea))
            BitmapCopyArea(window0, eyesPos.x, eyesPos.y, eyes, &eyesArea);
        }
      }
      break;

    case 3:
      {
        WORD frame = TrackValueGet(ninja_eyes, frameCount);

        switch (frame) {
          case 0:
            BitmapCopy(window0, 64 + 8, 80 + 16, eyeLeft[0]);
            BitmapCopy(window0, 64 + 8 + 64, 80 + 16, eyeRight[0]);
            break;

          case 1:
            BitmapCopy(window0, 64 + 8, 80 + 16, eyeLeft[1]);
            BitmapCopy(window0, 64 + 8 + 64, 80 + 16, eyeRight[1]);
            break;

          case 2: 
            BitmapCopy(window0, 64 + 8, 80, eyes);
            break;

          case 3:
            BitmapCopy(window0, 64 + 8, 80 + 16, eyeLeft[2]);
            BitmapCopy(window0, 64 + 8 + 64, 80 + 16, eyeRight[2]);
            break;

          case 4:
            BitmapCopy(window0, 64 + 8, 80 + 16, eyeLeft[3]);
            BitmapCopy(window0, 64 + 8 + 64, 80 + 16, eyeRight[3]);
            break;

          default:
            break;
        }
      }
      break;

    default:
      break;
  }

  SwapScreen();
}

static void Render3() {
  static WORD count = 0;
  static WORD frame = 0;
  static WORD color = 0;

  if (frame < 2) {
    WORD i, j;

    BitmapClear(window0);
    BlitterLineSetup(window0, 1, LINE_EOR|LINE_ONEDOT);
    BlitterLine(WIDTH - 1, -1, WIDTH - 1, 12);

    for (i = 0, j = 12; j < HEIGHT; i++, j += 24) {
      BlitterLine(WIDTH / 4, HEIGHT / 4 + j / 2, 0, j);
      BlitterLine(WIDTH * 3 / 4, HEIGHT / 4 + j / 2, WIDTH - 2, j);
      if (i & 1)
        BlitterLine(WIDTH - 1, j, WIDTH - 1, j + 24);
    }

    for (i = 0, j = 16; j < WIDTH; i++, j += 32) {
      BlitterLine(WIDTH / 4 + j / 2, HEIGHT / 4 + 6, j, -1);
      BlitterLine(WIDTH / 4 + j / 2, HEIGHT * 3 / 4 - 6, j, HEIGHT - 1);
    }

    BlitterSet(window0, 0, -1);
    BlitterFill(window0, 1);
    BlitterClear(window0, 2);
    BlitterSet(window0, 3, -1);

    BlitterSetMask(window0, 0, 16, 0, bigmask, 0);
    BlitterSetMask(window0, 1, 16, 0, bigmask, 0);
    BlitterSetMask(window0, 2, 16, 0, bigmask, 0);
    BlitterSetMask(window0, 3, 16, 0, bigmask, -1);
  }

  if (frame & 1)
    BitmapCopy(window0, 34, 32, bigeyes);
  else
    BitmapCopy(window0, 34, 33, bigeyes);

  count += frameCount - lastFrameCount;

  if (count > 3) {
    count -= 3;
    frame++;
    color++;
  }

  if (color > 11)
    color = 0;

  if (color > 5) {
    CopInsSet16(pal + 9, pissedPal[color - 6]);
    CopInsSet16(pal + 11, 0xDDA);
  } else {
    CopInsSet16(pal + 9, 0xDDA);
    CopInsSet16(pal + 11, pissedPal[color]);
  }

  SwapScreen();
}

EFFECT(Ninja1, Load1, NULL, Init, NULL, Render1, NULL);
EFFECT(Ninja2, Load2, NULL, Init2, NULL, Render2, NULL);
EFFECT(Ninja3, Load3, UnLoad, NULL, Kill, Render3, NULL);
