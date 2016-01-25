#include "test.h"
#include "sync.h"

struct sync_mutex_arg {
  nk_mutex *m;
  int value;
};

static void sync_mutex_thd(nk_thd *self, void *_arg) {
  struct sync_mutex_arg *arg = _arg;
  for (int i = 0; i < 1000; i++) {
    nk_mutex_lock(arg->m);
    int v = arg->value;
    v++;
    nk_thd_yield();
    arg->value = v;
    nk_mutex_unlock(arg->m);
  }
}

NK_TEST(sync_mutex) {
  nk_host *h;
  NK_TEST_ASSERT(nk_host_create(&h) == NK_OK);
  nk_thd *thds[10];
  struct sync_mutex_arg arg;
  arg.value = 0;
  NK_TEST_ASSERT(nk_mutex_create(h, &arg.m) == NK_OK);
  for (int i = 0; i < 10; i++) {
    NK_TEST_ASSERT(
        nk_thd_create_ext(h, &thds[i], &sync_mutex_thd, &arg, NULL) == NK_OK);
  }
  nk_host_run(h, 10, NULL, NULL);

  NK_TEST_ASSERT(arg.value == 10000);
  NK_TEST_OK();
}

struct sync_cond_channel {
  nk_mutex *m;
  nk_cond *c;
  int count;
  int quit_token;
};

static void sync_cond_channel_send(struct sync_cond_channel *c, int count,
                                   int quit) {
  nk_mutex_lock(c->m);
  int oldval = c->count;
  c->count += count;
  int oldquit = c->quit_token;
  c->quit_token |= quit;
  if (c->count != oldval || c->quit_token != oldquit) {
    nk_cond_signal(c->c);
  }
  nk_mutex_unlock(c->m);
}

static void sync_cond_channel_recv(struct sync_cond_channel *c, int *count,
                                   int *quit) {
  nk_mutex_lock(c->m);
  while (c->count == 0 && c->quit_token == 0) {
    nk_cond_wait(c->c, c->m);
  }
  *count = c->count;
  *quit = c->quit_token;
  c->count = 0;
  c->quit_token = 0;
  nk_mutex_unlock(c->m);
}

static struct sync_cond_channel *sync_cond_channel_new(nk_host *h) {
  struct sync_cond_channel *c = NK_ALLOC(struct sync_cond_channel);
  nk_mutex_create(h, &c->m);
  nk_cond_create(h, &c->c);
  c->count = 0;
  c->quit_token = 0;
}

static void sync_cond_channel_destroy(struct sync_cond_channel *c) {
  nk_mutex_destroy(c->m);
  nk_cond_destroy(c->c);
  NK_FREE(c);
}

struct sync_cond_arg {
  struct sync_cond_channel *inbound, *outbound;
  int final_count;
};

static void sync_cond_thd(nk_thd *self, void *_arg) {
  struct sync_cond_arg *arg = _arg;

  int originator_count = 0;
  while (1) {
    // Inbound process.
    int count = 0, quit = 0;
    if (!arg->inbound) {
      if (originator_count++ < 1000) {
        count = 1;
        quit = 0;
      } else {
        count = 0;
        quit = 1;
      }
    } else {
      sync_cond_channel_recv(arg->inbound, &count, &quit);
    }

    // Outbound process.
    if (!arg->outbound) {
      arg->final_count += count;
    } else {
      for (int i = 0; i < count; i++) {
        sync_cond_channel_send(arg->outbound, 1, 0);
      }
      if (quit) {
        sync_cond_channel_send(arg->outbound, 0, 1);
      }
    }
    if (quit) {
      break;
    }
  }
}

NK_TEST(sync_cond) {
  nk_host *h;
  NK_TEST_ASSERT(nk_host_create(&h) == NK_OK);

  static const int kThdCount = 100;

  struct sync_cond_arg arg[kThdCount];
  nk_thd *thds[kThdCount];
  for (int i = 0; i < kThdCount; i++) {
    memset(&arg[i], 0, sizeof(struct sync_cond_arg));
    if (i != (kThdCount - 1)) {
      arg[i].outbound = sync_cond_channel_new(h);
    }
    if (i != 0) {
      arg[i].inbound = arg[i - 1].outbound;
    }

    nk_thd_create_ext(h, &thds[i], &sync_cond_thd, &arg[i], NULL);
  }

  nk_host_run(h, 1, NULL, NULL);

  NK_TEST_ASSERT(arg[kThdCount - 1].final_count == 1000);

  for (int i = 0; i < kThdCount; i++) {
    if (arg[i].outbound) {
      sync_cond_channel_destroy(arg[i].outbound);
    }
  }

  nk_host_destroy(h);

  NK_TEST_OK();
}

struct sync_barrier_arg {
  nk_barrier *b1;
  nk_barrier *b2;
  nk_mutex *m;
  int done_count;
  int iters;
  int thdcount;
  int ok_iters;
};

static void sync_barrier_thd1(nk_thd *self, void *_arg) {
  struct sync_barrier_arg *arg = _arg;

  for (int i = 0; i < arg->iters; i++) {
    nk_barrier_wait(arg->b1);
    nk_mutex_lock(arg->m);
    arg->done_count++;
    nk_mutex_unlock(arg->m);
    nk_barrier_wait(arg->b2);
  }
}

static void sync_barrier_thd2(nk_thd *self, void *_arg) {
  struct sync_barrier_arg *arg = _arg;

  for (int i = 0; i < arg->iters; i++) {
    nk_barrier_wait(arg->b1);
    nk_barrier_wait(arg->b2);
    if (arg->done_count == arg->thdcount) {
      arg->ok_iters++;
    }
    arg->done_count = 0;
  }
}

NK_TEST(sync_barrier) {
  nk_host *h;
  NK_TEST_ASSERT(nk_host_create(&h) == NK_OK);
  static const int kThdCount = 100;
  static const int kIterCount = 100;

  struct sync_barrier_arg arg;
  nk_thd *thds[kThdCount];
  nk_thd *masterthd;
  memset(&arg, 0, sizeof(struct sync_barrier_arg));
  arg.thdcount = kThdCount;
  arg.iters = kIterCount;
  nk_mutex_create(h, &arg.m);
  nk_barrier_create(h, &arg.b1, kThdCount + 1);
  nk_barrier_create(h, &arg.b2, kThdCount + 1);
  for (int i = 0; i < kThdCount; i++) {
    nk_thd_create_ext(h, &thds[i], &sync_barrier_thd1, &arg, NULL);
  }
  nk_thd_create_ext(h, &masterthd, &sync_barrier_thd2, &arg, NULL);

  nk_host_run(h, 100, NULL, NULL);

  NK_TEST_ASSERT(arg.ok_iters == kIterCount);

  nk_barrier_destroy(arg.b2);
  nk_barrier_destroy(arg.b1);
  nk_mutex_destroy(arg.m);
  nk_host_destroy(h);

  NK_TEST_OK();
}
