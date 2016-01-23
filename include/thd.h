#ifndef __NK_THD_H__
#define __NK_THD_H__

#include "kernel.h"
#include "queue.h"
#include "databuf.h"

// typedefs.
typedef struct nk_schob nk_schob;
typedef struct nk_thd_attrs nk_thd_attrs;
typedef struct nk_thd nk_thd;
typedef struct nk_dpc_attrs nk_dpc_attrs;
typedef struct nk_dpc nk_dpc;
typedef struct nk_host nk_host;
typedef struct nk_hostthd nk_hostthd;

// -------------- schobs: schedulable entities. --------------

typedef enum nk_schob_state {
  NK_SCHOB_STATE_READY,    // on a scheduler run queue.
  NK_SCHOB_STATE_RUNNING,  // actually running in a host thread.
  NK_SCHOB_STATE_WAITING,  // waiting at a port/semaphore.
  NK_SCHOB_STATE_FINISHED, // finished waiting to be joined.
  NK_SCHOB_STATE_ZOMBIE,   // zombie waiting to be freed.
} nk_schob_state;

typedef enum nk_schob_type {
  NK_SCHOB_TYPE_THD, // cooperatively-scheduled thread with own stack.
  NK_SCHOB_TYPE_DPC, // deferred procedure call to execute only once.
} nk_schob_type;

struct nk_schob {
  nk_schob_state state;
  nk_schob_type type;

  // on a global scheduler queue, host-thread queue, msg or sem queue, join
  // queue, or cleanup queue.
  queue_entry runq;

  // running on a host thread?
  nk_hostthd *hostthd;

  // list of threads waiting to join on this schob's completion.
  queue_head joinq;
  pthread_spinlock_t joinq_lock;
  nk_thd *joined; // which thread joined us?

  uint32_t prio;

  void *retval;
  int detached; // do we become a zombie (true) or will we be joined (false)?
};

QUEUE_DEFINE(nk_schob, runq);

#define NK_PRIO_MIN 0
#define NK_PRIO_DEFAULT 0x80000000
#define NK_PRIO_MAX 0xffffffff

// ----------------- thds: conventional green threads. ------------

struct nk_thd_attrs {
  // Stack size.
  uint32_t stacksize;
  uint32_t prio;
  int detached;
};

#define NK_THD_ATTRS_INIT                                                      \
  { NK_THD_STACKSIZE_DEFAULT, NK_PRIO_DEFAULT, 0 }

#define NK_THD_STACKSIZE_MIN 4096
#define NK_THD_STACKSIZE_DEFAULT 8192
#define NK_THD_STACKSIZE_MAX (4 * 1024 * 1024)

struct nk_thd {
  nk_schob schob; // parent class
  void *stack;
  void *stacktop;
  size_t stacklen; // actual stacklen, as opposed to attrs-specified len.
};

typedef void *(*nk_thd_entrypoint)(nk_thd *self, void *data);

nk_status nk_thd_create(nk_thd **ret, nk_thd_entrypoint entry, void *data,
                        const nk_thd_attrs *attrs);
void nk_thd_yield();
void __attribute__((noreturn)) nk_thd_exit(void *retval);
nk_status nk_thd_join(nk_thd *thd, void **ret);
nk_thd *nk_thd_self(); // get the current thread, if any.

// ------------- dpcs: deferred procedure calls. ----------------

struct nk_dpc_attrs {
  uint32_t prio;
  int detached;
};

#define NK_DPC_ATTRS_INIT                                                      \
  { NK_PRIO_DEFAULT, 0 }

typedef void *(*nk_dpc_func)(void *data);

struct nk_dpc {
  nk_schob schob; // parent class
  nk_dpc_func func;
  void *data;
};

nk_status nk_dpc_create(nk_dpc **ret, nk_dpc_func func, void *data,
                        const nk_dpc_attrs *attrs);
nk_status nk_dpc_join(nk_dpc *dpc, void **ret);
nk_dpc *nk_dpc_self(); // get the current dpc, if any.

// ---------- host threads: these run thds and dpcs. ---------------

struct nk_hostthd {
  // Host that owns this thread.
  nk_host *host;
  // List of all host-threads.
  queue_entry list;
  // running schob -- thd or dpc.
  nk_schob *running;
  // corresponding system thread.
  pthread_t pthread;
  // system thread stack on which scheduler and dpcs run.
  void *hoststack;
};

QUEUE_DEFINE(nk_hostthd, list);

// Global host context.
struct nk_host {
  // Global runqueue.
  pthread_mutex_t runq_mutex;
  pthread_cond_t runq_cond;
  queue_head runq;
  // Host-thread list.
  pthread_mutex_t hostthd_mutex;
  queue_head hostthds;
  // Shutdown flag. Protected under, and signaled by, runq_lock / runq_cond.
  int shutdown;
  // Monitor thread.
  pthread_t monitor_thd;
};

// Creates a new host instance.
nk_status nk_host_create(nk_host **ret);
// automatically creates host threads as needed.
void nk_host_run(nk_host *host);
// called while host is running: sets shutdown flag.
void nk_host_shutdown(nk_host *host);

// --------------- arch-specific stuff. ------------------

// Returns new top-of-stack.
void *nk_arch_create_ctx(void *stacktop,
                         void (*entry)(void *data1, void *data2, void *data3),
                         void *data1, void *data2, void *data3);
void nk_arch_switch_ctx(void **fromstack, void *tostack);

#endif // __NK_THD_H__
