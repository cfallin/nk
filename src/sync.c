#include "sync.h"
#include "thd.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>

nk_status nk_mutex_create(nk_host *host, nk_mutex **ret) {
  nk_status status;

  status = NK_ERR_NOMEM;
  nk_mutex *m = nk_freelist_alloc(&host->mutex_freelist);
  if (!m) {
    goto err;
  }

  if (pthread_spin_init(&m->lock, PTHREAD_PROCESS_PRIVATE)) {
    goto err;
  }

  m->host = host;
  QUEUE_INIT(&m->waiters);

  *ret = m;
  return NK_OK;
err:
  if (m) {
    nk_freelist_free(&host->mutex_freelist, m);
  }
  return status;
}

void nk_mutex_destroy(nk_mutex *m) {
  assert(nk_schob_runq_empty(&m->waiters));
  pthread_spin_destroy(&m->lock);
  nk_freelist_free(&m->host->mutex_freelist, m);
}

void nk_mutex_lock(nk_mutex *m) {
  nk_thd *t = nk_thd_self();
  assert(t != NULL);

  while (1) {
    pthread_spin_lock(&m->lock);
    if (m->locked) {
      nk_schob_runq_push(&m->waiters, (nk_schob *)t);
      pthread_spin_unlock(&m->lock);
      nk_thd_yield_ext(NK_THD_YIELD_REASON_WAITING);
    } else {
      m->locked = 1;
      pthread_spin_unlock(&m->lock);
      break;
    }
  }
}

void nk_mutex_unlock(nk_mutex *m) {
  pthread_spin_lock(&m->lock);
  m->locked = 0;
  if (!nk_schob_runq_empty(&m->waiters)) {
    nk_thd *t = (nk_thd *)nk_schob_runq_shift(&m->waiters);
    pthread_spin_unlock(&m->lock);
    nk_schob_enqueue(m->host, (nk_schob *)t, /* new_schob = */ 0);
  } else {
    pthread_spin_unlock(&m->lock);
  }
}

nk_status nk_cond_create(nk_host *host, nk_cond **ret) {
  nk_status status;

  status = NK_ERR_NOMEM;
  nk_cond *c = nk_freelist_alloc(&host->cond_freelist);
  if (!c) {
    goto err;
  }

  if (pthread_spin_init(&c->lock, PTHREAD_PROCESS_PRIVATE)) {
    goto err;
  }

  c->host = host;
  QUEUE_INIT(&c->waiters);

  *ret = c;
  return NK_OK;
err:
  if (c) {
    nk_freelist_free(&host->cond_freelist, c);
  }
  return status;
}

void nk_cond_destroy(nk_cond *c) {
  assert(nk_schob_runq_empty(&c->waiters));
  pthread_spin_destroy(&c->lock);
  nk_freelist_free(&c->host->cond_freelist, c);
}

void nk_cond_wait(nk_cond *c, nk_mutex *m) {
  nk_thd *self = nk_thd_self();
  assert(self != NULL);
  // We enqueue ourselves first and *then* unlock the mutex.
  pthread_spin_lock(&c->lock);
  nk_schob_runq_push(&c->waiters, (nk_schob *)self);
  pthread_spin_unlock(&c->lock);
  // This gap does not create a race condition: even if some other thread
  // immediately signals the condition variable here, once we release the
  // spinlock, and moves us back to READY state on the runqueue, we yield with
  // reason WAITING which signals to the host thread scheduler that something
  // else owns our runnable status / is responsible for re-enqueueing us.
  nk_mutex_unlock(m);
  nk_thd_yield_ext(NK_THD_YIELD_REASON_WAITING);
  nk_mutex_lock(m);
}

void nk_cond_signal(nk_cond *c) {
  pthread_spin_lock(&c->lock);
  if (!nk_schob_runq_empty(&c->waiters)) {
    nk_thd *t = (nk_thd *)nk_schob_runq_shift(&c->waiters);
    nk_schob_enqueue(c->host, (nk_schob *)t, /* new_schob = */ 0);
  }
  pthread_spin_unlock(&c->lock);
}

void nk_cond_broadcast(nk_cond *c) {
  pthread_spin_lock(&c->lock);
  queue_head to_run;
  QUEUE_INIT(&to_run);
  while (!nk_schob_runq_empty(&c->waiters)) {
    nk_thd *t = (nk_thd *)nk_schob_runq_shift(&c->waiters);
    nk_schob_runq_push(&to_run, (nk_schob *)t);
  }
  pthread_spin_unlock(&c->lock);
  while (!nk_schob_runq_empty(&to_run)) {
    nk_thd *t = (nk_thd *)nk_schob_runq_shift(&to_run);
    nk_schob_enqueue(c->host, (nk_schob *)t, /* new_schob = */ 0);
  }
}

nk_status nk_barrier_create(nk_host *host, nk_barrier **ret, int limit) {
  nk_status status;

  status = NK_ERR_NOMEM;
  nk_barrier *b = nk_freelist_alloc(&host->barrier_freelist);
  if (!b) {
    goto err;
  }

  if (pthread_spin_init(&b->lock, PTHREAD_PROCESS_PRIVATE)) {
    goto err;
  }

  b->host = host;
  b->count = 0;
  b->limit = limit;
  QUEUE_INIT(&b->waiters);

  *ret = b;
  return NK_OK;
err:
  if (b) {
    nk_freelist_free(&host->barrier_freelist, b);
  }
  return status;
}

void nk_barrier_destroy(nk_barrier *b) {
  assert(nk_schob_runq_empty(&b->waiters));
  pthread_spin_destroy(&b->lock);
  nk_freelist_free(&b->host->barrier_freelist, b);
}

void nk_barrier_wait(nk_barrier *b) {
  nk_thd *self = nk_thd_self();
  assert(self != NULL);
  pthread_spin_lock(&b->lock);
  b->count++;
  assert(b->count <= b->limit);
  if (b->count == b->limit) {
    b->count = 0;
    queue_head to_run;
    QUEUE_INIT(&to_run);
    while (!nk_schob_runq_empty(&b->waiters)) {
      nk_thd *t = (nk_thd *)nk_schob_runq_shift(&b->waiters);
      nk_schob_runq_push(&to_run, (nk_schob *)t);
    }
    pthread_spin_unlock(&b->lock);
    while (!nk_schob_runq_empty(&to_run)) {
      nk_thd *t = (nk_thd *)nk_schob_runq_shift(&to_run);
      nk_schob_enqueue(b->host, (nk_schob *)t, /* new_schob = */ 0);
    }
  } else {
    nk_schob_runq_push(&b->waiters, (nk_schob *)self);
    pthread_spin_unlock(&b->lock);
    nk_thd_yield_ext(NK_THD_YIELD_REASON_WAITING);
  }
}

DEFINE_SIMPLE_FREELIST_TYPE(nk_mutex, 10000);
DEFINE_SIMPLE_FREELIST_TYPE(nk_cond, 10000);
DEFINE_SIMPLE_FREELIST_TYPE(nk_barrier, 10000);

nk_status nk_sync_init_freelists(nk_host *h) {
  nk_status status;
  if ((status = nk_freelist_init(&h->mutex_freelist, &nk_mutex_freelist_attrs,
                                 NULL)) != NK_OK) {
    return status;
  }
  if ((status = nk_freelist_init(&h->cond_freelist, &nk_cond_freelist_attrs,
                                 NULL)) != NK_OK) {
    nk_freelist_destroy(&h->mutex_freelist);
    return status;
  }
  if ((status = nk_freelist_init(&h->barrier_freelist,
                                 &nk_barrier_freelist_attrs, NULL)) != NK_OK) {
    nk_freelist_destroy(&h->cond_freelist);
    nk_freelist_destroy(&h->mutex_freelist);
    return status;
  }
  return NK_OK;
}

void nk_sync_destroy_freelists(nk_host *h) {
  nk_freelist_destroy(&h->mutex_freelist);
  nk_freelist_destroy(&h->cond_freelist);
  nk_freelist_destroy(&h->barrier_freelist);
}
