#ifndef __WORKER_H__
#define __WORKER_H__

#include <exec/nodes.h>

typedef enum __attribute__((packed)) {
  WS_NEW, WS_READY, WS_RUNNING, WS_ABORTED, WS_DONE
} WorkStateT;

typedef struct {
  struct MinNode link;
  volatile WorkStateT state;
  void (*func)(void *);
  void *data;
} WorkT;

void WorkAdd(WorkT *work);
void WorkWaitDone(WorkT *work);

void WorkerStart();
void WorkerShutdown();

#endif
