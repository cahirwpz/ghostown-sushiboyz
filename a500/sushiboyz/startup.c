#include <exec/execbase.h>
#include <graphics/gfxbase.h>

#include <proto/alib.h>
#include <proto/exec.h>
#include <proto/graphics.h>

#include "hardware.h"
#include "interrupts.h"
#include "io.h"
#include "sushiboyz.h"

STRPTR __cwdpath = "nigiri";
LONG __chipmem = 450 * 1024;
LONG __fastmem = 380 * 1024;

static WORD kickVer;
static struct List PortsIntChain;
static struct List CoperIntChain;
static struct List VertbIntChain;
static struct List TaskReady;
static struct List TaskWait;

static struct View *oldView;
static UWORD oldDmacon, oldIntena, oldAdkcon;
static ULONG oldCacheBits = 0;

static struct Task *idleTask = NULL;

static void IdleTask() {
  Log("[Init] Idle task %lx started!\n", (LONG)FindTask(NULL));
  for (;;) {}
}

void KillOS() {
  Log("[Startup] Save AmigaOS state.\n");

  /* Allocate blitter. */
  WaitBlit();
  OwnBlitter();

  /* No calls to any other library than exec beyond this point or expect
   * undefined behaviour including crashes. */
  Forbid();

  /* Disable CPU caches. */
  if (kickVer >= 36)
    oldCacheBits = CacheControl(0, -1);

  /* Intercept the view of AmigaOS. */
  oldView = GfxBase->ActiView;
  LoadView(NULL);
  WaitTOF();
  WaitTOF();

  /* DMA & interrupts take-over. */
  oldAdkcon = custom->adkconr;
  oldDmacon = custom->dmaconr;
  oldIntena = custom->intenar;

  /* Prohibit dma & interrupts. */
  custom->adkcon = (UWORD)~ADKF_SETCLR;
  custom->dmacon = (UWORD)~DMAF_SETCLR;
  custom->intena = (UWORD)~INTF_SETCLR;
  WaitVBlank();

  /* Clear all interrupt requests. Really. */
  custom->intreq = (UWORD)~INTF_SETCLR;
  custom->intreq = (UWORD)~INTF_SETCLR;

  /* Enable master switches...
   * .. and SOFTINT which is presumably used by Exec's scheduler. */
  custom->dmacon = DMAF_SETCLR | DMAF_MASTER;
  custom->intena = INTF_SETCLR | INTF_INTEN | INTF_SOFTINT;

  /* Save original interrupt server chains. */
  CopyMem(SysBase->IntVects[INTB_PORTS].iv_Data,
          &PortsIntChain, sizeof(struct List));
  CopyMem(SysBase->IntVects[INTB_COPER].iv_Data,
          &CoperIntChain, sizeof(struct List));
  CopyMem(SysBase->IntVects[INTB_VERTB].iv_Data, 
          &VertbIntChain, sizeof(struct List));

  /* Reset system's interrupt server chains. */
  NewList(SysBase->IntVects[INTB_PORTS].iv_Data);
  NewList(SysBase->IntVects[INTB_COPER].iv_Data);
  NewList(SysBase->IntVects[INTB_VERTB].iv_Data);

  /* Save original task lists. */
  CopyMem(&SysBase->TaskReady, &TaskReady, sizeof(struct List));
  CopyMem(&SysBase->TaskWait, &TaskWait, sizeof(struct List));

  /* Reset system's task lists. */
  NewList(&SysBase->TaskReady);
  NewList(&SysBase->TaskWait);

  /* Restore multitasking. */
  Permit();

  idleTask = CreateTask("Idle Task", -10, IdleTask, 4096);
}

void RestoreOS() {
  Log("[Startup] Restore AmigaOS state.\n");

  RemTask(idleTask);

  /* Suspend multitasking. */
  Forbid();

  /* firstly... disable dma and interrupts that were used in Main */
  custom->dmacon = (UWORD)~DMAF_SETCLR;
  custom->intena = (UWORD)~INTF_SETCLR;
  WaitVBlank();

  /* Restore original task lists. */
  CopyMem(&TaskReady, &SysBase->TaskReady, sizeof(struct List));
  CopyMem(&TaskWait, &SysBase->TaskWait, sizeof(struct List));

  /* Restore original interrupt server chains. */
  CopyMem(&PortsIntChain, SysBase->IntVects[INTB_PORTS].iv_Data,
          sizeof(struct List));
  CopyMem(&CoperIntChain, SysBase->IntVects[INTB_COPER].iv_Data,
          sizeof(struct List));
  CopyMem(&VertbIntChain, SysBase->IntVects[INTB_VERTB].iv_Data, 
          sizeof(struct List));

  /* Restore AmigaOS state of dma & interrupts. */
  custom->dmacon = oldDmacon | DMAF_SETCLR;
  custom->intena = oldIntena | INTF_SETCLR;
  custom->adkcon = oldAdkcon | ADKF_SETCLR;

  /* Restore old copper list... */
  custom->cop1lc = (ULONG)GfxBase->copinit;
  WaitVBlank();

  /* ... and original view. */
  LoadView(oldView);
  WaitTOF();
  WaitTOF();

  /* Enable CPU caches. */
  if (kickVer >= 36)
    CacheControl(oldCacheBits, -1);

  /* Restore multitasking. */
  Permit();

  /* Deallocate blitter. */
  DisownBlitter();
}

ADD2INIT(KillOS, -50);
ADD2EXIT(RestoreOS, -50);

void SystemInfo() {
  WORD kickRev;
  WORD cpu = 0;

  if (SysBase->AttnFlags & AFF_68060)
    cpu = 6;
  else if (SysBase->AttnFlags & AFF_68040)
    cpu = 4;
  else if (SysBase->AttnFlags & AFF_68030)
    cpu = 3;
  else if (SysBase->AttnFlags & AFF_68020)
    cpu = 2;
  else if (SysBase->AttnFlags & AFF_68010)
    cpu = 1;

  /* Based on WhichAmiga method. */
  {
    APTR kickEnd = (APTR)0x1000000;
    ULONG kickSize = *(ULONG *)(kickEnd - 0x14);
    UWORD *kick = kickEnd - kickSize;

    kickVer = kick[6];
    kickRev = kick[7];
  }

  Log("[Main] ROM: %ld.%ld, CPU: 680%ld0, CHIP: %ldkB, FAST: %ldkB\n",
      (LONG)kickVer, (LONG)kickRev, (LONG)cpu,
      (LONG)(AvailMem(MEMF_CHIP | MEMF_LARGEST) / 1024),
      (LONG)(AvailMem(MEMF_FAST | MEMF_LARGEST) / 1024));
}

ADD2INIT(SystemInfo, -100);

typedef struct LibDesc {
  struct Library *base;
  char *name;
} LibDescT;

extern LibDescT *__LIB_LIST__[];

#define OSLIBVERSION 33

void InitLibraries() {
  LibDescT **list = __LIB_LIST__;
  ULONG numbases = (ULONG)*list++;

  while (numbases-- > 0) {
    LibDescT *lib = *list++;
    if (!(lib->base = OpenLibrary(lib->name, OSLIBVERSION))) {
      Log("Cannot open '%s'!\n", lib->name);
      exit(20);
    }
  }
}

void KillLibraries() {
  LibDescT **list = __LIB_LIST__;
  ULONG numbases = (ULONG)*list++;

  while (numbases-- > 0) {
    LibDescT *lib = *list++;
    if (lib->base)
      CloseLibrary(lib->base);
  }
}

ADD2INIT(InitLibraries, -120);
ADD2EXIT(KillLibraries, -120);
