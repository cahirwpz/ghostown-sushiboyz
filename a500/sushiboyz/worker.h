#ifndef __WORKER_H__
#define __WORKER_H__

#include <exec/nodes.h>

typedef struct Node NodeT;
typedef struct MinNode MinNodeT;

typedef enum __attribute__((packed)) {
  WS_NEW, WS_READY, WS_RUNNING, WS_ABORTED, WS_DONE
} WorkStateT;

typedef void (*WorkFuncT)(void * asm("a0"));

typedef struct {
  MinNodeT link;
  volatile WorkStateT state;
  WorkFuncT func;
  void *data;
} WorkT;

void WorkAdd(WorkT *work);
void WorkWaitDone(WorkT *work);

void WorkerStart();
void WorkerShutdown();

#endif
