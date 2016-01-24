#include "sync.h"
#include "thd.h"

#include <pthread.h>

nk_status nk_mutex_create(nk_host *host, nk_mutex **ret) {}
void nk_mutex_destroy(nk_mutex *m) {}
void nk_mutex_lock(nk_mutex *m) {}
void nk_mutex_unlock(nk_mutex *m) {}

nk_status nk_cond_create(nk_host *host, nk_cond **ret) {}
void nk_cond_destroy(nk_cond *c) {}
void nk_cond_wait(nk_cond *c, nk_mutex *m) {}
void nk_cond_signal(nk_cond *c) {}
void nk_cond_broadcast(nk_cond *c) {}

nk_status nk_barrier_create(nk_host *host, nk_barrier **ret, int count) {}
void nk_barrier_destroy(nk_barrier *b) {}
void nk_barrier_wait(nk_barrier *b) {}

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
