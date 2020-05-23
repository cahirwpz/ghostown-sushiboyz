#include <exec/tasks.h>
#include <exec/execbase.h>

#include "hardware.h"
#include "memory.h"
#include "blitter.h"
#include "reader.h"
#include "io.h"
#include "ilbm.h"
#include "serial.h"
#include "coplist.h"
#include "tasks.h"
#include "worker.h"

#include "sushiboyz.h"

#define WIDTH 320
#define HEIGHT 256
#define DEPTH 5

/* Resources shared among all effects. */
BitmapT *screen0, *screen1;
TrackT **tracks;

/*****************************************************************************/
/* List of effects.                                                          */
/*****************************************************************************/

static EffectT *effects[] = {
  &Ninja1,
  &Ninja2,
  &Ninja3,
  &Toilet,
  &Floor,
  &FlatShade,
  &RunningMan,
  &Blurred3D,
  &GhostownLogo,
  &Bumpmap,
  &Credits,
  &Thunders,
  &Twister,
  &Watchmaker,
  &UVMap,
  &Filled3D,
  &SushiGirl,
  &Glitch,
  NULL
};

/*****************************************************************************/
/* Demo time line.                                                           */
/*****************************************************************************/

typedef struct TimeSlot {
  UWORD start, end;
  EffectT *effect;
} TimeSlotT;

#ifndef FRAMES_PER_ROW
#error "FRAMES_PER_ROW: not defined!"
#endif

#define PT_FRAME(pos) \
  (((((pos) >> 8) & 0xff) * 64 + ((pos) & 0x3f)) * FRAMES_PER_ROW)

#define _TS(begin, end, effect) \
  { PT_FRAME(begin), PT_FRAME(end), &(effect) }

#define _TS_END() {0, 0, NULL}

#define TS_LAST_P(TS) ((TS)->effect == NULL)

static TimeSlotT timeline[] = {
  _TS(0x0000, 0x0020, Ninja1),
  _TS(0x0020, 0x0120, Ninja2),
  _TS(0x0120, 0x0210, Ninja3),
  _TS(0x0210, 0x0300, Toilet),
  _TS(0x0300, 0x0500, Floor),
  _TS(0x0500, 0x0700, FlatShade),
  _TS(0x0700, 0x0900, RunningMan),
  _TS(0x0900, 0x0b00, Blurred3D),
  _TS(0x0b00, 0x0c00, GhostownLogo),
  _TS(0x0c00, 0x0d00, Bumpmap),
  _TS(0x0d00, 0x1100, Credits),
  _TS(0x1100, 0x1200, Thunders),
  _TS(0x1200, 0x1300, Twister),
  _TS(0x1300, 0x1400, Watchmaker),
  _TS(0x1400, 0x1500, UVMap),
  _TS(0x1500, 0x1700, Filled3D),
  _TS(0x1700, 0x1800, SushiGirl),
  _TS(0x1800, 0x183C, Glitch),
  _TS_END()
};

UWORD frameFromStart;
UWORD frameTillEnd;
UWORD frameCount;
UWORD lastFrameCount;

static TimeSlotT *currentTimeSlot;

#if 0
TimeSlotT *TimelineForward(TimeSlotT *item, WORD pos) {
  for (; item->effect; item++)
    if ((item->start <= pos) && (pos < item->end))
      break;
  return item->effect ? item : NULL;
}
#endif

void UpdateFrameCount() {
  frameCount = ReadFrameCounter();
  frameFromStart = frameCount - currentTimeSlot->start;
  frameTillEnd = currentTimeSlot->end - frameCount;
}

/*****************************************************************************/
/* Actions to be performed in background while demo is running.              */
/*****************************************************************************/

typedef struct TimeAction {
  WorkT work;
  UWORD time;
} TimeActionT;

#define _WORK(func, data) \
  (WorkT){ {}, WS_NEW, (WorkFuncT)(func), (void *)(data) }

#define _TA_LOAD(time, effect) \
  { _WORK(EffectLoad, &(effect)), PT_FRAME(time) }
#define _TA_PREP(time, effect) \
  { _WORK(EffectPrepare, &(effect)), PT_FRAME(time) }
#define _TA_END() {}

#define TA_LAST_P(TA) ((TA)->work.func == NULL)
#define TA_AFTER_P(TA, TIME) ((TA)->time > (TIME))

/* 'actions' must be sorted by time */
static TimeActionT actions[] = {
  _TA_LOAD(0x0000, Music),
  _TA_LOAD(0x0000, Ninja1),
  _TA_LOAD(0x0000, Ninja2),
  _TA_LOAD(0x0000, Ninja3),
  _TA_LOAD(0x0000, Toilet),
  _TA_LOAD(0x0210, Floor),              /* Toilet */
  _TA_PREP(0x0210, Floor),
  _TA_LOAD(0x0210, FlatShade),
  _TA_LOAD(0x0210, RunningMan),
  _TA_LOAD(0x0700, Blurred3D),          /* RunningMan */
  _TA_LOAD(0x0900, GhostownLogo),       /* Blurred3D */
  _TA_LOAD(0x0900, Bumpmap),
  _TA_PREP(0x0900, Bumpmap),
  _TA_LOAD(0x0900, Credits),
  _TA_PREP(0x0900, Credits),
  _TA_LOAD(0x1000, Thunders),
  _TA_PREP(0x1000, Thunders),
  _TA_LOAD(0x1100, Twister),
  _TA_LOAD(0x1100, Watchmaker),
  _TA_PREP(0x1100, Watchmaker),
  _TA_LOAD(0x1300, UVMap),              /* Watchmaker */
  _TA_PREP(0x1300, UVMap),
  _TA_LOAD(0x1300, Filled3D),
  _TA_PREP(0x1300, Filled3D),
  _TA_LOAD(0x1500, SushiGirl),          /* Filled3D */
  _TA_PREP(0x1500, SushiGirl),
  _TA_LOAD(0x1700, Glitch),
  _TA_END()
};

static TimeActionT *todoTimeAction = actions;

/* Schedule all time actions that should be finished before given time. */
void TimeActionSchedule(UWORD time) {
  while (!TA_LAST_P(todoTimeAction) && !TA_AFTER_P(todoTimeAction, time)) {
    WorkAdd(&todoTimeAction->work);
    todoTimeAction++;
  }
}

/* Has worker thread finished all scheduled actions till given time? */
BOOL TimeActionIsDone(UWORD time) {
  TimeActionT *ta = todoTimeAction;

  /* Return if no actions were scheduled so far... */
  if (ta == actions)
    return TRUE;

  /* Go backwards to find last TA with time that is not after given time. */
  do {
    --ta;
  } while (TA_AFTER_P(ta, time));

  return ta->work.state == WS_DONE;
}

/*****************************************************************************/
/* Main routine driving demo.                                                */
/*****************************************************************************/

static void RunEffects(TimeSlotT *item) {
  BOOL exit = FALSE;

#if REMOTE_CONTROL
  char cmd[16];
  WORD cmdLen = 0;
  memset(cmd, 0, sizeof(cmd));
#endif

  SetFrameCounter(item->start);

  for (; item->effect && !exit; item++) {
    EffectT *effect = item->effect;
    WORD realStart;

    if (!(ReadFrameCounter() < item->end))
      continue;

    while (ReadFrameCounter() < item->start) {
      if (LeftMouseButton()) {
        exit = TRUE;
        break;
      }
    }

    currentTimeSlot = item;

    EffectPrepare(effect);
    EffectInit(effect);

    frameFromStart = 0;
    frameTillEnd = item->end - item->start;
    realStart = ReadFrameCounter();

    lastFrameCount = ReadFrameCounter();
    while (frameCount < item->end) {
      WORD t = ReadFrameCounter();
      TimeActionSchedule(t);
#if REMOTE_CONTROL
      SerialPrint("F %ld\n", (LONG)t);
#endif
      frameCount = t;
      frameFromStart = frameCount - realStart;
      frameTillEnd = item->end - frameCount;
      if (effect->Render)
        effect->Render();
      else
        TaskWait(VBlankEvent);
      lastFrameCount = t;

      if (LeftMouseButton()) {
        exit = TRUE;
        break;
      }
#if REMOTE_CONTROL
      {
        LONG c;

        while ((c = SerialGet()) >= 0) {
          if (c == '\n') {
            Log("[Serial] Received line '%s'\n", cmd);
            memset(cmd, 0, sizeof(cmd));
            cmdLen = 0;
          } else {
            cmd[cmdLen++] = c;
          }
        }
      }
#endif
    }

    EffectKill(effect);
    WaitVBlank();
    EffectUnLoad(effect);

    currentTimeSlot = NULL;
  }
}

int main() {
  SerialInit(9600);
  WorkerStart();

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
    EffectT **item;
    for (item = effects; *item; item++) {
      EffectT *effect = *item;
      SerialPrint("E %s\n", effect->name);
    }
  }

  {
    TimeSlotT *item;
    for (item = timeline; !TS_LAST_P(item); item++)
      SerialPrint("TS %ld %ld %s\n", (LONG)item->start, (LONG)item->end,
                  item->effect->name);
  }

  {
    TimeActionT *item;
    for (item = actions; !TA_LAST_P(item); item++) {
      EffectT *effect = item->work.data;
      const char *action = "?";
      if (item->work.func == (WorkFuncT)&EffectLoad)
        action = "LOAD";
      if (item->work.func == (WorkFuncT)&EffectPrepare)
        action = "PREPARE";
      SerialPrint("TA %ld %s %s\n", (LONG)item->time, action, effect->name);
    }
  }

  screen0 = NewBitmap(WIDTH, HEIGHT, DEPTH);
  screen1 = NewBitmap(WIDTH, HEIGHT, DEPTH);

  /* Display loading screen and load demo. */
  {
    TimeActionSchedule(0);

    EffectLoad(&Loading);
    EffectInit(&Loading);

    /* Reset frame counter and wait for all time actions to finish. */
    SetFrameCounter(0);

    while (!TimeActionIsDone(0)) {
      if (Loading.Render)
        Loading.Render();
      else
        TaskWait(VBlankEvent);
    }

    EffectKill(&Loading);
  }

  /* Run all effects listed in demo timeline. */
  EffectInit(&Music);
  RunEffects(timeline);
  EffectKill(&Music);
  EffectUnLoad(&Music);

  /* Free all resources allocated by effects. */
  {
    TimeSlotT *item;

    for (item = timeline; item->effect; item++)
      EffectUnLoad(item->effect);
  }

  DeleteBitmap(screen1);
  DeleteBitmap(screen0);

  WorkerShutdown();
  SerialKill();

  return 0;
}
