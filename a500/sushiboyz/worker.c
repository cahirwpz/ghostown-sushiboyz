#include <exec/lists.h>
#include <exec/nodes.h>
#include <exec/tasks.h>
#include <proto/alib.h>
#include <proto/exec.h>

#include "common.h"
#include "worker.h"
#include "tasks.h"

typedef struct Task TaskT;
typedef struct List ListT;
typedef struct Node NodeT;
typedef struct MinNode MinNodeT;

#define LIST() &(struct List){}

static struct Task *workerTask = NULL;
static volatile BOOL workerDrain = FALSE;
static ListT *workQueue = LIST();
static ListT *workQueueNotEmpty = LIST();
static ListT *workDone = LIST();

static void WorkerTask() {
  Log("[Init] Worker task %lx started!\n", (LONG)FindTask(NULL));

  for (;;) {
    /* The non-empty condition must be rechecked,
     * as the task may get preempted before RemHead executes. */
    TaskWait(workQueueNotEmpty);

    for (;;) {
      WorkT *work;

      /* Dequeue an action to run. */
      Forbid();
      work = (WorkT *)RemHead(workQueue);
      if (work)
        work->state = WS_RUNNING;
      Permit();

      /* Exit loop if nothing to be done. */
      if (work == NULL)
        break;

      /* Execute the work. */
      work->func(work->data);

      /* Signal all tasks that were waiting for an action to become DONE. */
      Forbid();
      work->state = WS_DONE;
      TaskSignal(workDone);
      Permit();
    }
  }
}

void WorkAdd(WorkT *work) {
  Forbid();
  if (!workerDrain) {
    work->state = WS_READY;
    AddTail(workQueue, (NodeT *)work);
  }
  Permit();
}

/* XXX Assumes that current task has higher priority than worker! */
void WorkWaitFor(WorkT *work) {
  while (work->state == WS_READY || work->state == WS_RUNNING)
    TaskWait(workDone);
}

void WorkerStart() {
  /* TODO Should not use system memory allocator! */
  workerTask = CreateTask("Worker Task", -10, WorkerTask, 1024);

  NewList(workQueue);
  NewList(workQueueNotEmpty);
  NewList(workDone);
}

void WorkerShutdown() {
  /* Prevent from more work being added. */
  workerDrain = TRUE;

  /* Remove all work from the queue. */
  Forbid();
  {
    WorkT *work;
    while ((work = (WorkT *)RemHead(workQueue)))
      work->state = WS_ABORTED;
    /* If someone was waiting for a work to finish, we've just aborted it. */
    TaskSignal(workDone);
  }
  Permit();

  {
    TaskT *self = FindTask(NULL);
    /* Lower our priority to let worker task to finish. */
    BYTE pri = SetTaskPri(self, workerTask->tc_Node.ln_Pri);
    /* Wait for the worker to suspend on workQueueNotEmpty */
    while (workerTask->tc_State != TS_WAIT)
      TaskYield();
    /* Restore our priority. */
    SetTaskPri(self, pri);
  }

  /* Now worker task is safe to remove. */
  RemTask(workerTask);
}
