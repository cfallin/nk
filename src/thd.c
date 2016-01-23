#include "thd.h"

#include <assert.h>
#include <pthread.h>
#include <sys/mman.h>

// This is the only (thread-local) global state. Everything else is accessed
// via the host (global nk instance) or host-thread struct.
static pthread_key_t nk_hostthd_self_key;
static pthread_once_t nk_hostthd_self_key_once = PTHREAD_ONCE_INIT;

void setup_hostthd_self_key() {
  pthread_key_create(&nk_hostthd_self_key, NULL);
}

static nk_hostthd *nk_hostthd_self() {
  pthread_once(&nk_hostthd_self_key_once, setup_hostthd_self_key);
  return (nk_hostthd *)pthread_getspecific(nk_hostthd_self_key);
}

nk_thd *nk_thd_self() {
  nk_hostthd *host = nk_hostthd_self();
  if (!host || !host->running) {
    return NULL;
  }
  if (host->running->type != NK_SCHOB_TYPE_THD) {
    return NULL;
  }
  return (nk_thd *)host->running;
}

nk_dpc *nk_dpc_self() {
  nk_hostthd *host = nk_hostthd_self();
  if (!host || !host->running) {
    return NULL;
  }
  if (host->running->type != NK_SCHOB_TYPE_DPC) {
    return NULL;
  }
  return (nk_dpc *)host->running;
}

// ------ schob ------

static nk_status nk_schob_init(nk_schob *schob, nk_schob_type type,
                               uint32_t prio, int detached) {
  // All fields zeroed on entry.
  schob->state = NK_SCHOB_STATE_READY;
  schob->type = type;
  if (pthread_spin_init(&schob->join_lock, PTHREAD_PROCESS_PRIVATE)) {
    return NK_ERR_NOMEM;
  }
  schob->prio = prio;
  schob->detached = detached;
  return NK_OK;
}

static void nk_schob_destroy(nk_schob *schob) {
  pthread_spin_destroy(&schob->join_lock);
}

static void nk_schob_setdone(nk_schob *schob, void *retval) {
  schob->retval = retval;
  schob->state =
      schob->detached ? NK_SCHOB_STATE_ZOMBIE : NK_SCHOB_STATE_FINISHED;
}

static nk_status nk_schob_join(nk_thd *self, nk_schob *joined) {
  if (joined->detached) {
    return NK_ERR_PARAM;
  }

  // Atomically take the join slot on this object, and return immediately if it
  // has already finished.
  pthread_spin_lock(&joined->join_lock);
  if (joined->joined) {
    pthread_spin_unlock(&joined->join_lock);
    return NK_ERR_NOJOIN;
  }
  joined->joined = self;
  if (joined->state == NK_SCHOB_STATE_FINISHED) {
    return NK_OK;
  }
  pthread_spin_unlock(&joined->join_lock);

  // Otherwise, block in the WAITING state until the object reaches FINISHED.
  while (1) {
    self->schob.state = NK_SCHOB_STATE_WAITING;
    nk_thd_yield();
    pthread_spin_lock(&joined->join_lock);
    if (joined->state == NK_SCHOB_STATE_FINISHED) {
      break;
    }
    pthread_spin_unlock(&joined->join_lock);
  }

  return NK_OK;
}

// This is the main scheduler. It picks a schob off to run of the nk_host
// context. Assumes `runq_mutex` is already held.
static nk_schob *nk_schob_next(nk_host *host) {
  if (nk_schob_runq_empty(&host->runq)) {
    return NULL;
  }
  nk_schob *n = nk_schob_runq_shift(&host->runq);
  assert(n->state == NK_SCHOB_STATE_READY);
  return n;
}

// This enqueues a schob onto the runqueue. It assumes no locks are held.
static void nk_schob_enqueue(nk_host *host, nk_schob *schob) {
  pthread_mutex_lock(&host->runq_mutex);
  nk_schob_runq_push(&host->runq, schob);
  pthread_cond_signal(&host->runq_cond);
  pthread_mutex_unlock(&host->runq_mutex);
}

// ------ thd ------

static nk_status allocstack(size_t size, void **stack, void **top,
                            size_t *alloced) {
  // Round up to next page size.
  size = (size + 4095) & ~4095UL;
  // Add one more page as a guard page.
  size += 4096;

  void *p = mmap(NULL, size, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
  if (!p) {
    return NK_ERR_NOMEM;
  }

  // protect the guard page.
  if (mprotect(p, 4096, PROT_NONE)) {
    munmap(p, size);
    return NK_ERR_NOMEM;
  }

  *stack = p;
  *top = ((uint8_t *)p) + size;
  *alloced = size;
  return NK_OK;
}

static nk_status freestack(size_t size, void *stack) {
  if (munmap(stack, size)) {
    return NK_ERR_PARAM;
  } else {
    return NK_OK;
  }
}

static __attribute__((noreturn)) void nk_thd_entry(void *data1, void *data2,
                                                   void *data3) {
  nk_thd *t = (nk_thd *)data1;
  nk_thd_entrypoint f = (nk_thd_entrypoint)data2;
  void *f_data = data3;

  void *retval = f(t, f_data);
  nk_thd_exit(retval);
}

nk_status nk_thd_create(nk_thd **ret, nk_thd_entrypoint entry, void *data,
                        const nk_thd_attrs *attrs) {
  nk_hostthd *host = nk_hostthd_self();
  assert(host != NULL);
  nk_status status;

  status = NK_ERR_NOMEM;
  NK_AUTOPTR(nk_thd) t = NK_ALLOC(nk_thd);
  if (!t) {
    goto err;
  }

  status = NK_ERR_NOMEM;
  size_t stacksize = attrs ? attrs->stacksize : NK_THD_STACKSIZE_DEFAULT;
  status = allocstack(stacksize, &t->stack, &t->stacktop, &t->stacklen);
  if (status != NK_OK) {
    goto err;
  }

  status = nk_schob_init(&t->schob, NK_SCHOB_TYPE_THD,
                         attrs ? attrs->prio : NK_PRIO_DEFAULT,
                         attrs ? attrs->detached : 0);
  if (status != NK_OK) {
    goto err;
  }

  t->stacktop = nk_arch_create_ctx(t->stacktop, nk_thd_entry, /* data1 = */ t,
                                   /* data2 = */ entry, /* data3 = */ data);

  nk_schob_enqueue(host->host, (nk_schob *)t);

  *ret = NK_AUTOPTR_STEAL(nk_thd, t);
  return NK_OK;

err:
  return status;
}

// Called only once thread is joined. joinq is guaranteed empty.
static void nk_thd_destroy(nk_thd *t) {
  freestack(t->stacklen, t->stack);
  nk_schob_destroy(&t->schob);
  NK_FREE(t);
}

void nk_thd_yield() {
  nk_hostthd *host = nk_hostthd_self();
  nk_thd *self = nk_thd_self();
  // Set state to "ready" -- this communicates to host thread to place us back
  // on the run-queue.
  self->schob.state = NK_SCHOB_STATE_READY;
  // Context-switch back to host thread, which will schedule the next item.
  nk_arch_switch_ctx(&self->stacktop, host->hoststack);
}

void nk_thd_exit(void *retval) {
  nk_hostthd *host = nk_hostthd_self();
  nk_thd *self = nk_thd_self();
  nk_schob_setdone((nk_schob *)self, retval);
  // Should never return.
  nk_arch_switch_ctx(&self->stacktop, host->hoststack);
  while (1) {
    assert(0);
  }
}

nk_status nk_thd_join(nk_thd *thd, void **ret) {
  nk_thd *self = nk_thd_self();
  nk_status err = nk_schob_join(self, (nk_schob *)thd);
  if (err != NK_OK) {
    return err;
  }
  *ret = thd->schob.retval;
  nk_thd_destroy(thd);
  return NK_OK;
}

// ------------- dpc -----------
static nk_status nk_dpc_create_(nk_hostthd *host, nk_dpc **ret,
                                nk_dpc_func func, void *data,
                                const nk_dpc_attrs *attrs) {
  nk_status status;

  status = NK_ERR_NOMEM;
  NK_AUTOPTR(nk_dpc) d = NK_ALLOC(nk_dpc);
  if (!d) {
    goto err;
  }

  d->func = func;
  d->data = data;

  status = nk_schob_init(&d->schob, NK_SCHOB_TYPE_DPC,
                         attrs ? attrs->prio : NK_PRIO_DEFAULT,
                         attrs ? attrs->detached : 0);
  if (status != NK_OK) {
    goto err;
  }

  nk_schob_enqueue(host->host, (nk_schob *)d);

  *ret = NK_AUTOPTR_STEAL(nk_dpc, d);
  return NK_OK;

err:
  return status;
}

nk_status nk_dpc_create(nk_dpc **ret, nk_dpc_func func, void *data,
                        const nk_dpc_attrs *attrs) {
  nk_hostthd *host = nk_hostthd_self();
  assert(host != NULL);
  return nk_dpc_create_(host, ret, func, data, attrs);
}

static void nk_dpc_destroy(nk_dpc *dpc) {
  nk_schob_destroy(&dpc->schob);
  NK_FREE(dpc);
}

nk_status nk_dpc_join(nk_dpc *dpc, void **ret) {
  nk_thd *self = nk_thd_self();
  nk_status err = nk_schob_join(self, (nk_schob *)dpc);
  if (err != NK_OK) {
    return err;
  }
  *ret = dpc->schob.retval;
  nk_dpc_destroy(dpc);
  return NK_OK;
}

// --------------- hostthd ---------------

static void *nk_hostthd_main(void *_self) {
  nk_hostthd *self = (nk_hostthd *)_self;
  pthread_once(&nk_hostthd_self_key_once, setup_hostthd_self_key);
  pthread_setspecific(nk_hostthd_self_key, self);

  nk_host *host = self->host;
  while (1) {
    nk_schob *next = NULL;
    pthread_mutex_lock(&host->runq_mutex);
    while (1) {
      if (host->shutdown) {
        goto shutdown;
      }
      next = nk_schob_next(host);
      if (next) {
        break;
      }
      pthread_cond_wait(&host->runq_cond, &host->runq_mutex);
    }
    pthread_mutex_unlock(&host->runq_mutex);

    // If `next` is a dpc, run it here. If `next` is a thd, context-switch to
    // it until it yields.
    self->running = next;
    next->state = NK_SCHOB_STATE_RUNNING;
    switch (next->type) {
    case NK_SCHOB_TYPE_DPC: {
      nk_dpc *dpc = (nk_dpc *)next;
      void *ret = dpc->func(dpc->data);
      nk_schob_setdone(next, ret);
      // If `next` is in the ZOMBIE state, clean it up immediately.
      if (dpc->schob.state == NK_SCHOB_STATE_ZOMBIE) {
        nk_dpc_destroy(dpc);
      }
      // If this was the main DPC, initiate a shutdown.
      if (dpc == host->main_dpc) {
        nk_host_shutdown(host);
      }
      break;
    }
    case NK_SCHOB_TYPE_THD: {
      nk_thd *thd = (nk_thd *)next;
      nk_arch_switch_ctx(&self->hoststack, thd->stacktop);
      // If `next` is in the ZOMBIE state, clean it up immediately.
      if (thd->schob.state == NK_SCHOB_STATE_ZOMBIE) {
        nk_thd_destroy(thd);
      }
      break;
    }
    }
    self->running = NULL;

    // If `next` is in the FINISHED state, wake up all of its joiners (but
    // grant only one of them the join).
    if (next->state == NK_SCHOB_STATE_FINISHED) {
      pthread_spin_lock(&next->join_lock);
      if (next->joined) {
        next->joined->schob.state = NK_SCHOB_STATE_READY;
        nk_schob_enqueue(host, (nk_schob *)next->joined);
      }
      pthread_spin_unlock(&next->join_lock);
    }

    // If `next` is in the READY state, place it back on the runqueue.
    if (next->state == NK_SCHOB_STATE_READY) {
      pthread_mutex_lock(&host->runq_mutex);
      nk_schob_runq_push(&host->runq, next);
      pthread_mutex_unlock(&host->runq_mutex);
    }
  }
shutdown:

  return NULL;
}

static nk_status nk_hostthd_create(nk_hostthd **ret, nk_host *host) {
  nk_status status;

  status = NK_ERR_NOMEM;
  NK_AUTOPTR(nk_hostthd) h = NK_ALLOC(nk_hostthd);
  if (!h) {
    goto err;
  }

  h->host = host;
  status = NK_ERR_NOMEM;
  if (pthread_create(&h->pthread, NULL, &nk_hostthd_main, h)) {
    goto err;
  }

  pthread_mutex_lock(&host->hostthd_mutex);
  nk_hostthd_list_push(&host->hostthds, h);
  pthread_mutex_unlock(&host->hostthd_mutex);

  *ret = NK_AUTOPTR_STEAL(nk_hostthd, h);
  return NK_OK;

err:
  return status;
}

static void nk_hostthd_join(nk_hostthd *thd) {
  void *retval;
  pthread_join(thd->pthread, &retval);
  pthread_mutex_lock(&thd->host->hostthd_mutex);
  nk_hostthd_list_remove(thd);
  pthread_mutex_unlock(&thd->host->hostthd_mutex);
  NK_FREE(thd);
}

nk_status nk_host_create(nk_host **ret) {
  nk_status status;

  status = NK_ERR_NOMEM;
  NK_AUTOPTR(nk_host) h = NK_ALLOC(nk_host);
  if (!h) {
    goto err;
  }

  status = NK_ERR_NOMEM;
  if (pthread_mutex_init(&h->runq_mutex, NULL)) {
    goto err;
  }

  status = NK_ERR_NOMEM;
  if (pthread_cond_init(&h->runq_cond, NULL)) {
    goto err2;
  }

  status = NK_ERR_NOMEM;
  if (pthread_mutex_init(&h->hostthd_mutex, NULL)) {
    goto err3;
  }

  QUEUE_INIT(&h->runq);
  QUEUE_INIT(&h->hostthds);
  h->shutdown = 0;

  *ret = NK_AUTOPTR_STEAL(nk_host, h);
  return NK_OK;

err3:
  pthread_cond_destroy(&h->runq_cond);
err2:
  pthread_mutex_destroy(&h->runq_mutex);
err:
  return status;
}

void *nk_host_run(nk_host *host, int workers, nk_dpc_func main, void *data) {
  // Create workers. Create one more 'hidden' worker to run the main DPC.
  for (int i = 0; i < workers + 1; i++) {
    nk_hostthd *hostthd;
    nk_status status = nk_hostthd_create(&hostthd, host);
    if (status != NK_OK) {
      break;
    }
    if (i == 0) {
      status = nk_dpc_create_(hostthd, &host->main_dpc, main, data, NULL);
      if (status != NK_OK) {
        break;
      }
    }
  }

  // Host-thread join/destroy loop.
  while (1) {
    pthread_mutex_lock(&host->hostthd_mutex);
    if (nk_hostthd_list_empty(&host->hostthds)) {
      break;
    }
    nk_hostthd *h = nk_hostthd_list_shift(&host->hostthds);
    pthread_mutex_unlock(&host->hostthd_mutex);
    nk_hostthd_join(h);
  }

  void *ret = host->main_dpc->schob.retval;
  nk_dpc_destroy(host->main_dpc);
  return ret;
}

void nk_host_shutdown(nk_host *host) {
  pthread_mutex_lock(&host->runq_mutex);
  host->shutdown = 1;
  pthread_cond_broadcast(&host->runq_cond);
  pthread_mutex_unlock(&host->runq_mutex);
}

void nk_host_destroy(nk_host *host) {
  assert(host->shutdown);
  pthread_mutex_destroy(&host->hostthd_mutex);
  pthread_cond_destroy(&host->runq_cond);
  pthread_mutex_destroy(&host->runq_mutex);
  NK_FREE(host);
}

