/*
 * Copyright (c) 2016, Chris Fallin <cfallin@c1f.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef __NK_SYNC_H__
#define __NK_SYNC_H__

#include "kernel.h"
#include "thd.h"

#include <pthread.h>

typedef struct nk_mutex {
  nk_host *host;
  pthread_spinlock_t lock;
  int locked;
  queue_head waiters;
} nk_mutex;

nk_status nk_mutex_create(nk_host *host, nk_mutex **ret);
void nk_mutex_destroy(nk_mutex *m);
void nk_mutex_lock(nk_mutex *m);
void nk_mutex_unlock(nk_mutex *m);

typedef struct nk_cond {
  nk_host *host;
  pthread_spinlock_t lock;
  queue_head waiters;
} nk_cond;

nk_status nk_cond_create(nk_host *host, nk_cond **ret);
void nk_cond_destroy(nk_cond *c);
void nk_cond_wait(nk_cond *c, nk_mutex *m);
void nk_cond_signal(nk_cond *c);
void nk_cond_broadcast(nk_cond *c);

typedef struct nk_barrier {
  nk_host *host;
  pthread_spinlock_t lock;
  int count;
  int limit;
  queue_head waiters;
} nk_barrier;

nk_status nk_barrier_create(nk_host *host, nk_barrier **ret, int limit);
void nk_barrier_destroy(nk_barrier *b);
void nk_barrier_wait(nk_barrier *b);

// Internal only.
nk_status nk_sync_init_freelists(nk_host *h);
void nk_sync_destroy_freelists(nk_host *h);

#endif // __NK_SYNC_H__
