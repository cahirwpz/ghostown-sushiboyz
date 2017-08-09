#include <proto/alib.h>
#include <exec/tasks.h>
#include <exec/execbase.h>

#include "hardware.h"
#include "interrupts.h"
#include "memory.h"
#include "reader.h"
#include "color.h"
#include "io.h"
#include "serial.h"

#include "sushiboyz.h"

#define WIDTH 320
#define HEIGHT 256
#define DEPTH 5

#define _TA_LOAD(effect)  { TA_LOAD, -1, &(effect) }
#define _TA_WAIT(time)    { TA_WAIT, PT_FRAME(time), NULL }
#define _TA_END()         { 0, -1, NULL }

typedef enum __attribute__((packed)) {
  TA_WAIT = 1, TA_LOAD, TA_PREPARE 
} TimeActionTag;

typedef struct {
  TimeActionTag type;
  UWORD frame;
  EffectT *effect;
} TimeActionT;

/* Resources shared among all effects. */
BitmapT *screen0, *screen1;
TrackT **tracks;

static TimeSlotT timeline[] = {
  _TS(0x0000, 0x0020, 0, Ninja1),
  _TS(0x0020, 0x0120, 0, Ninja2),
  _TS(0x0120, 0x0210, 0, Ninja3),
  _TS(0x0210, 0x0300, 0, Toilet),
  _TS(0x0300, 0x0500, 0, Floor),
  _TS(0x0500, 0x0700, 0, FlatShade),
  _TS(0x0700, 0x0900, 0, RunningMan),
  _TS(0x0900, 0x0b00, 0, Blurred3D),
  _TS(0x0b00, 0x0c00, 0, GhostownLogo),
  _TS(0x0c00, 0x0d00, 0, Bumpmap),
  _TS(0x0d00, 0x1100, 0, Credits),
  _TS(0x1100, 0x1200, 1, Thunders),
  _TS(0x1200, 0x1300, 1, Twister),
  _TS(0x1300, 0x1400, 1, Watchmaker),
  _TS(0x1400, 0x1500, 2, UVMap),
  _TS(0x1500, 0x1700, 0, Filled3D),
  _TS(0x1700, 0x1800, 2, SushiGirl),
  _TS(0x1800, 0x183C, 3, Glitch),
  _TS_END()
};

static TimeActionT actions[] = {
  _TA_LOAD(Ninja1),
  _TA_LOAD(Ninja2),
  _TA_LOAD(Ninja3),
  _TA_LOAD(Toilet),
  _TA_LOAD(Floor),
  _TA_LOAD(FlatShade),
  _TA_LOAD(RunningMan),
  _TA_LOAD(Blurred3D),
  _TA_LOAD(GhostownLogo),
  _TA_LOAD(Bumpmap),
  _TA_LOAD(Credits),
  _TA_LOAD(Filled3D),
  _TA_WAIT(0x1100),
  _TA_LOAD(Thunders),
  _TA_LOAD(Twister),
  _TA_LOAD(Watchmaker),
  _TA_WAIT(0x1400),
  _TA_LOAD(UVMap),
  _TA_LOAD(SushiGirl),
  _TA_LOAD(Glitch),
  _TA_END()
};

INTERRUPT(P61PlayerInterrupt, 10, P61_Music);

static struct Task *demoTask = NULL;
static struct Task *workerTask = NULL;

static __interrupt LONG Reschedule() {
  SetTaskPri(demoTask, 0);
  return 0;
}

INTERRUPT(RescheduleInterrupt, 20, Reschedule);

static void WorkerTask() {
  Log("Worker Task!\n");
  LoadEffects(timeline, -1);
  SetTaskPri(demoTask, 0);

  for (;;);
}

int main() {
  demoTask = FindTask(NULL);
  workerTask = CreateTask("Worker Task", -10, WorkerTask, 4096);

  SerialInit(9600);

  Log("  _.___    ___.    ______ ___     ___    ______   ____ ______\n"
      "  \\| _/___.\\ _|___/     //  /___ /  /__ /     /  /   /      /\n"
      " _ \\ \\_   |  \\_  .  7  /_\\_    ./ ____/.  7  / /\\   /   7  /_\n"
      " //__.   _|__./  |____/./     /|_./   _|___./  /\\__/.  /___\\\\\n"
      "     |___\\   /    /    |_____/   |____\\    |__/     |_/   \n"
      " - ------- ./    / ---------------------------------------- -\n"
      "       km! |____/ aSL\n\n");

  Log("Loading artwork... Please wait...\n");

  SerialPrint("FPR %ld\n", (LONG)FRAMES_PER_ROW);

  {
    TimeSlotT *item = timeline;
    for (; item->effect; item++)
      SerialPrint("TS %ld %ld %ld %s\n", (LONG)item->start, (LONG)item->end,
                  (LONG)item->phase, item->effect->name);
  }

  if ((tracks = LoadTrackList("sushiboyz.sync"))) {
    APTR module = LoadFile("p61.jazzcat-sushiboyz", MEMF_PUBLIC);
    APTR samples = LoadFile("smp.jazzcat-sushiboyz", MEMF_CHIP);

    screen0 = NewBitmap(WIDTH, HEIGHT, DEPTH);
    screen1 = NewBitmap(WIDTH, HEIGHT, DEPTH);

    /* Wait for worker task to load data. */
    SetTaskPri(demoTask, -20);

    P61_Init(module, samples, NULL);
    P61_ControlBlock.Play = 1;
    AddIntServer(INTB_VERTB, &P61PlayerInterrupt);
    AddIntServer(INTB_VERTB, &RescheduleInterrupt);

    RunEffects(timeline);

    RemIntServer(INTB_VERTB, &RescheduleInterrupt);
    RemIntServer(INTB_VERTB, &P61PlayerInterrupt);
    P61_ControlBlock.Play = 0;
    P61_End();

    UnLoadEffects(timeline);

    DeleteBitmap(screen0);
    DeleteBitmap(screen1);

    MemFree(module);
    MemFree(samples);

    DeleteTrackList(tracks);
  }

  SerialKill();
  RemTask(workerTask);

  return 0;
}

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

UBYTE envelope[24] = {
  0, 0, 1, 1, 2, 3, 5, 8, 11, 13, 14, 15,
  15, 14, 13, 11, 8, 5, 3, 2, 1, 1, 0, 0 
};
