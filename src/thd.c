#include "thd.h"
#include "msg.h"
#include "sync.h"

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

nk_hostthd *nk_hostthd_self() {
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
                               uint32_t prio) {
  // All fields zeroed on entry.
  schob->state = NK_SCHOB_STATE_READY;
  schob->type = type;
  schob->prio = prio;
  return NK_OK;
}

static void nk_schob_destroy(nk_schob *schob) {
  // Nothing.
}

static void nk_schob_setdone(nk_host *host, nk_schob *schob) {
  schob->state = NK_SCHOB_STATE_ZOMBIE;
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
void nk_schob_enqueue(nk_host *host, nk_schob *schob, int new_schob) {
  pthread_mutex_lock(&host->runq_mutex);
  nk_schob_runq_push(&host->runq, schob);
  if (new_schob) {
    host->schob_count++;
  }
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

  f(t, f_data);
  nk_thd_exit();
}

nk_status nk_thd_create(nk_thd **ret, nk_thd_entrypoint entry, void *data,
                        const nk_thd_attrs *attrs) {
  nk_hostthd *host = nk_hostthd_self();
  assert(host != NULL);
  return nk_thd_create_ext(host->host, ret, entry, data, attrs);
}

nk_status nk_thd_create_ext(nk_host *host, nk_thd **ret,
                            nk_thd_entrypoint entry, void *data,
                            const nk_thd_attrs *attrs) {
  nk_status status;

  status = NK_ERR_NOMEM;
  nk_thd *t = nk_freelist_alloc(&host->thd_freelist);
  if (!t) {
    goto err;
  }

  if (pthread_spin_init(&t->running_lock, PTHREAD_PROCESS_PRIVATE)) {
    goto err;
  }

  status = NK_ERR_NOMEM;
  size_t stacksize = attrs ? attrs->stacksize : NK_THD_STACKSIZE_DEFAULT;
  status = allocstack(stacksize, &t->stack, &t->stacktop, &t->stacklen);
  if (status != NK_OK) {
    goto err;
  }

  status = nk_schob_init(&t->schob, NK_SCHOB_TYPE_THD,
                         attrs ? attrs->prio : NK_PRIO_DEFAULT);
  if (status != NK_OK) {
    goto err;
  }

  t->stacktop = nk_arch_create_ctx(t->stacktop, nk_thd_entry, /* data1 = */ t,
                                   /* data2 = */ entry, /* data3 = */ data);

  nk_schob_enqueue(host, (nk_schob *)t, /* new_schob = */ 1);

  *ret = t;
  return NK_OK;

err:
  if (t) {
    nk_freelist_free(&host->thd_freelist, t);
  }
  return status;
}

static void nk_thd_destroy(nk_thd *t) {
  nk_hostthd *hostthd = nk_hostthd_self();
  assert(hostthd != NULL);
  nk_host *host = hostthd->host;
  freestack(t->stacklen, t->stack);
  pthread_spin_destroy(&t->running_lock);
  nk_schob_destroy(&t->schob);
  nk_freelist_free(&host->thd_freelist, t);
}

void nk_thd_yield_ext(nk_thd_yield_reason r) {
  nk_hostthd *host = nk_hostthd_self();
  assert(host != NULL);
  nk_thd *self = nk_thd_self();
  assert(self != NULL);
  // Context-switch back to host thread, which will schedule the next item.
  nk_arch_switch_ctx(&self->stacktop, host->hoststack, r);
}

void nk_thd_yield() { nk_thd_yield_ext(NK_THD_YIELD_REASON_READY); }

void nk_thd_exit() {
  nk_hostthd *host = nk_hostthd_self();
  assert(host != NULL);
  nk_thd *self = nk_thd_self();
  assert(self != NULL);
  nk_schob_setdone(host->host, (nk_schob *)self);
  // Should never return.
  nk_arch_switch_ctx(&self->stacktop, host->hoststack,
                     NK_THD_YIELD_REASON_ZOMBIE);
  while (1) {
    assert(0);
  }
}

// ------------- dpc -----------

nk_status nk_dpc_create(nk_dpc **ret, nk_dpc_func func, void *data,
                        const nk_dpc_attrs *attrs) {
  nk_hostthd *host = nk_hostthd_self();
  assert(host != NULL);
  return nk_dpc_create_ext(host->host, ret, func, data, attrs);
}

nk_status nk_dpc_create_ext(nk_host *host, nk_dpc **ret, nk_dpc_func func,
                            void *data, const nk_dpc_attrs *attrs) {
  nk_status status;

  status = NK_ERR_NOMEM;
  nk_dpc *d = nk_freelist_alloc(&host->dpc_freelist);
  if (!d) {
    goto err;
  }

  d->func = func;
  d->data = data;

  status = nk_schob_init(&d->schob, NK_SCHOB_TYPE_DPC,
                         attrs ? attrs->prio : NK_PRIO_DEFAULT);
  if (status != NK_OK) {
    goto err;
  }

  nk_schob_enqueue(host, (nk_schob *)d, /* new_schob = */ 1);

  *ret = d;
  return NK_OK;

err:
  if (d) {
    nk_freelist_free(&host->dpc_freelist, d);
  }
  return status;
}

static void nk_dpc_destroy(nk_dpc *dpc) {
  nk_hostthd *hostthd = nk_hostthd_self();
  assert(hostthd != NULL);
  nk_host *host = hostthd->host;
  nk_schob_destroy(&dpc->schob);
  nk_freelist_free(&host->dpc_freelist, dpc);
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

      if (host->shutdown || host->schob_count == 0) {
        pthread_mutex_unlock(&host->runq_mutex);
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

    int destroyed = 0;
    int insert_into_runq = 0;

    switch (next->type) {
    case NK_SCHOB_TYPE_DPC: {
      nk_dpc *dpc = (nk_dpc *)next;
      dpc->func(dpc->data);
      nk_schob_setdone(host, next);
      // If `next` is in the ZOMBIE state, clean it up immediately.
      if (dpc->schob.state == NK_SCHOB_STATE_ZOMBIE) {
        nk_dpc_destroy(dpc);
        destroyed = 1;
      }
      break;
    }
    case NK_SCHOB_TYPE_THD: {
      nk_thd *thd = (nk_thd *)next;
      pthread_spin_lock(&thd->running_lock);
      nk_thd_yield_reason reason =
          nk_arch_switch_ctx(&self->hoststack, thd->stacktop, 0);
      pthread_spin_unlock(&thd->running_lock);
      switch (reason) {
      case NK_THD_YIELD_REASON_READY:
        // insert into runqueue below.
        insert_into_runq = 1;
        break;
      case NK_THD_YIELD_REASON_ZOMBIE:
        // kill immediately.
        nk_thd_destroy(thd);
        destroyed = 1;
        break;
      case NK_THD_YIELD_REASON_WAITING:
        // Do nothing -- yielding code will have added thd to other queue
        // already.
        break;
      }
      break;
    }
    }
    self->running = NULL;

    // If we destroyed a thread or DPC, decrement the total schob count.
    if (destroyed) {
      pthread_mutex_lock(&host->runq_mutex);
      host->schob_count--;
      if (host->schob_count == 0) {
        pthread_cond_broadcast(&host->runq_cond);
      }
      pthread_mutex_unlock(&host->runq_mutex);
    }
    // If `next` yielded ready to run again, place it back on the runqueue.
    else if (insert_into_runq) {
      next->state = NK_SCHOB_STATE_READY;
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
  nk_hostthd *h = nk_freelist_alloc(&host->hostthd_freelist);
  if (!h) {
    goto err;
  }

  h->host = host;
  status = NK_ERR_NOMEM;
  if (pthread_create(&h->pthread, NULL, &nk_hostthd_main, h)) {
    goto err;
  }

  pthread_mutex_lock(&host->runq_mutex);
  nk_hostthd_list_push(&host->hostthds, h);
  host->hostthd_count++;
  pthread_mutex_unlock(&host->runq_mutex);

  *ret = h;
  return NK_OK;

err:
  if (h) {
    nk_freelist_free(&host->hostthd_freelist, h);
  }
  return status;
}

static void nk_hostthd_join(nk_hostthd *thd) {
  void *retval;
  nk_host *host = thd->host;
  pthread_join(thd->pthread, &retval);
  pthread_mutex_lock(&thd->host->runq_mutex);
  nk_hostthd_list_remove(thd);
  thd->host->hostthd_count--;
  pthread_mutex_unlock(&thd->host->runq_mutex);
  nk_freelist_free(&host->hostthd_freelist, thd);
}

DEFINE_SIMPLE_FREELIST_TYPE(nk_thd, 10000);
DEFINE_SIMPLE_FREELIST_TYPE(nk_dpc, 10000);
DEFINE_SIMPLE_FREELIST_TYPE(nk_hostthd, 10000);

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

  QUEUE_INIT(&h->runq);
  QUEUE_INIT(&h->hostthds);

  if (nk_freelist_init(&h->thd_freelist, &nk_thd_freelist_attrs, NULL) !=
      NK_OK) {
    goto err3;
  }
  if (nk_freelist_init(&h->dpc_freelist, &nk_dpc_freelist_attrs, NULL) !=
      NK_OK) {
    goto err4;
  }
  if (nk_freelist_init(&h->hostthd_freelist, &nk_hostthd_freelist_attrs,
                       NULL) != NK_OK) {
    goto err5;
  }
  if (nk_msg_init_freelists(h) != NK_OK) {
    goto err6;
  }
  if (nk_sync_init_freelists(h) != NK_OK) {
    goto err7;
  }

  *ret = NK_AUTOPTR_STEAL(nk_host, h);
  return NK_OK;

err7:
  nk_msg_destroy_freelists(h);
err6:
  nk_freelist_destroy(&h->hostthd_freelist);
err5:
  nk_freelist_destroy(&h->dpc_freelist);
err4:
  nk_freelist_destroy(&h->thd_freelist);
err3:
  pthread_cond_destroy(&h->runq_cond);
err2:
  pthread_mutex_destroy(&h->runq_mutex);
err:
  return status;
}

void nk_host_run(nk_host *host, int workers, nk_dpc_func main, void *data) {
  // Create workers. Create one more 'hidden' worker to run the main DPC.
  for (int i = 0; i < workers + 1; i++) {
    nk_hostthd *hostthd;
    nk_status status = nk_hostthd_create(&hostthd, host);
    if (status != NK_OK) {
      nk_host_shutdown(host);
      break;
    }
    if (i == 0 && main != NULL) {
      nk_dpc *main_dpc;
      status = nk_dpc_create_ext(host, &main_dpc, main, data, NULL);
      if (status != NK_OK) {
        nk_host_shutdown(host);
        break;
      }
    }
  }

  // Host-thread join/destroy loop.
  while (1) {
    pthread_mutex_lock(&host->runq_mutex);
    if (nk_hostthd_list_empty(&host->hostthds)) {
      pthread_mutex_unlock(&host->runq_mutex);
      break;
    }
    nk_hostthd *h = nk_hostthd_list_shift(&host->hostthds);
    pthread_mutex_unlock(&host->runq_mutex);
    nk_hostthd_join(h);
  }

  // Destroy any remaining thds/DPCs on runq.
  for (nk_schob *s = nk_schob_runq_begin(&host->runq),
                *e = nk_schob_runq_end(&host->runq), *next = NULL;
       s != e; s = next) {
    next = nk_schob_runq_next(s);
    switch (s->type) {
    case NK_SCHOB_TYPE_THD:
      nk_thd_destroy((nk_thd *)s);
      break;
    case NK_SCHOB_TYPE_DPC:
      nk_dpc_destroy((nk_dpc *)s);
      break;
    }
  }
}

void nk_host_shutdown(nk_host *host) {
  pthread_mutex_lock(&host->runq_mutex);
  host->shutdown = 1;
  pthread_cond_broadcast(&host->runq_cond);
  pthread_mutex_unlock(&host->runq_mutex);
}

void nk_host_destroy(nk_host *host) {
  assert(host->schob_count == 0);
  pthread_cond_destroy(&host->runq_cond);
  pthread_mutex_destroy(&host->runq_mutex);
  nk_freelist_destroy(&host->thd_freelist);
  nk_freelist_destroy(&host->dpc_freelist);
  nk_freelist_destroy(&host->hostthd_freelist);
  nk_msg_destroy_freelists(host);
  nk_sync_destroy_freelists(host);
  NK_FREE(host);
}
