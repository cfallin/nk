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
  NK_SCHOB_STATE_FINISHED, // finished, now waiting to be joined.
  NK_SCHOB_STATE_ZOMBIE,   // zombie, now waiting to be freed.
} nk_schob_state;

typedef enum nk_schob_type {
  NK_SCHOB_TYPE_THD, // cooperatively-scheduled thread with own stack.
  NK_SCHOB_TYPE_DPC, // deferred procedure call to execute only once.
} nk_schob_type;

struct nk_schob {
  nk_schob_state state;
  nk_schob_type type;

  // State lock: protects `state`, `joiner` and `joined`. Whenever lock is not
  // held, the schob must be in one of the following states:
  // - RUNNING and running (or about to be run, or just returned from running)
  //   on some host thread, and not on runq.
  // - READY and on the runq.
  // - WAITING and on a port or semaphore's runq or waiting to join a schob.
  // - FINISHED and not on any runq.
  // - ZOMBIE and owned by the hostthd that transitioned it to that state, about
  //   to be freed.
  //
  // Furthermore, when the lock is not held, one of the following is true:
  // - `joiner` and `joined` are both NULL/zero respectively.
  // - `joiner` is set to a joining thread, that thread is in WAITING state,
  //   and this schob is not in FINISHED state.
  // - `joiner` is set to a joining thread, that thread has been moved to READY
  //   state and will soon be placed on the runqueue (if transitioned by the
  //   schob becoming ready) or is already running (if joiner finds schob in
  //   finished state already), this schob is in FINISHED state, and `joined`
  //   is set to one (true).
  //
  // When two locks must be taken on two schobs, they are always locked in
  // address order and unlocked in reverse address order.
  //
  // All schob locks are ordered after the runq mutex.
  pthread_spinlock_t lock;

  // on a global scheduler queue, host-thread queue, msg or sem queue, join
  // queue, or cleanup queue.
  queue_entry runq;

  // running on a host thread?
  nk_hostthd *hostthd;

  // Thread waiting to join. Only one can wait. `joiner` is set when a joiner
  // claims this object to join. `joined` is set when the joiner has returned
  // -- it is used to wake up the joiner exactly once, only if needed.
  nk_thd *joiner;
  int joined;

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

/**
 * Creates a new thread. Must only be called from within a DPC or thread
 * context. `attrs` may be NULL.
 */
nk_status nk_thd_create(nk_thd **ret, nk_thd_entrypoint entry, void *data,
                        const nk_thd_attrs *attrs);
/**
 * Yields to the scheduler. Control may return at any time.
 */
void nk_thd_yield();
/**
 * Exits the thread with the given return value. Control will never return. If a
 * thread is blocked on a join of this thread, it will be unblocked.
 */
void __attribute__((noreturn)) nk_thd_exit(void *retval);
/**
 * Joins a thread. Only one join may be performed on a given thread. A join must
 * be performed on every thread not created in "detached" state. The return
 * value that the thread body passed to `nk_thd_exit` or returned from its main
 * function will be placed in `*ret`. Must be called from within a thread
 * context.
 */
nk_status nk_thd_join(nk_thd *thd, void **ret);
/**
 * Returns the current thread, or NULL if in DPC or other context.
 */
nk_thd *nk_thd_self();

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

/**
 * Create a new DPC (deferred procedure call). It will run exactly once, at some
 * later time, outside the context of the calling DPC/thread. This function must
 * be called in the context of another DPC or thread.
 */
nk_status nk_dpc_create(nk_dpc **ret, nk_dpc_func func, void *data,
                        const nk_dpc_attrs *attrs);
/**
 * Join a DPC (wait for it to finish). This must be called from within a thread
 * context. A DPC must be joined if and only if it was not created with
 * "detached" state. Its return value will be placed in `*ret`.
 */
nk_status nk_dpc_join(nk_dpc *dpc, void **ret);
/**
 * Returns the current DPC context, if any, or NULL if in thread or other
 * context.
 */
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
  // How many running DPCs? We need at least one more hostthd than this in
  // order to avoid deadlock. Protected by runq_mutex.
  int running_dpc_count;
  // How many host-threads are active? Protected by hostthd_mutex.
  int hostthd_count;
  // Host-thread list.
  pthread_mutex_t hostthd_mutex;
  queue_head hostthds;
  // Shutdown flag. Protected under, and signaled by, runq_lock / runq_cond.
  int shutdown;
  // Main DPC.
  nk_dpc *main_dpc;
};

/**
 * Creates a new host instance. This is the kernel context that runs all
 * threads/DPCs. Multiple host contexts may exist within one program.
 */
nk_status nk_host_create(nk_host **ret);
/**
 * Runs the host instance, returning after the instance is shut down. The given
 * DPC function/arg is executed as a DPC within the host context, and is the
 * only point from which other threads/DPCs may be created. The main DPC should
 * join all other non-detached threads/DPCs and then call shutdown() when done
 * (otherwise resources will leak).
 *
 * The return value of the main DPC is returned.
 */
void *nk_host_run(nk_host *host, int workers, nk_dpc_func main, void *data);
/**
 * Destroys the host instance. Must be called only after `nk_host_run()`
 * returns.
 */
void nk_host_destroy(nk_host *host);

/**
 * Initiates a shutdown on the given host instance. Should be called while the
 * host is running.
 */
void nk_host_shutdown(nk_host *host);

// --------------- arch-specific stuff. ------------------

// Returns new top-of-stack.
void *nk_arch_create_ctx(void *stacktop,
                         void (*entry)(void *data1, void *data2, void *data3),
                         void *data1, void *data2, void *data3);
void nk_arch_switch_ctx(void **fromstack, void *tostack);

#endif // __NK_THD_H__
