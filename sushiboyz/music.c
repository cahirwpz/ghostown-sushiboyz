#include "interrupts.h"
#include "memory.h"
#include "p61.h"
#include "io.h"

#include "sushiboyz.h"

/* Load music and synchronization tracks. */

static APTR module;
static APTR samples;
TrackT **tracks;

INTERRUPT(P61PlayerInterrupt, 10, P61_Music, NULL);

static void Load() {
  tracks = LoadTrackList("sushiboyz.sync");
  module = LoadFile("p61.jazzcat-sushiboyz", MEMF_PUBLIC);
  samples = LoadFile("smp.jazzcat-sushiboyz", MEMF_CHIP);
}

static void UnLoad() {
  MemFree(samples);
  MemFree(module);
  DeleteTrackList(tracks);
}

static void Init() {
  P61_Init(module, samples, NULL);
  P61_ControlBlock.Play = 1;
  AddIntServer(INTB_VERTB, P61PlayerInterrupt);
}

static void Kill() {
  RemIntServer(INTB_VERTB, P61PlayerInterrupt);
  P61_ControlBlock.Play = 0;
  P61_End();
}

EFFECT(Music, Load, UnLoad, Init, Kill, NULL, NULL);
