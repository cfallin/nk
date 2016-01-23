#include "thd.h"

#include <pthread.h>
#include <sys/mman.h>

// Global scheduler state.
struct nk_schedstate {
  queue_head runq;  // threads ready to run.
  queue_head waitq; // threads waiting on messages.
};
static struct nk_schedstate sched;

// Host-thread state.
struct nk_hostthd {
  pthread_t pthread;
  queue_entry queue;
  nk_thd *running;
};

QUEUE_DEFINE(nk_hostthd, queue);

static nk_status allocstack(size_t size, void **stack, void **top,
                            size_t *alloced) {
  // Round up to next page size.
  size = (size + 4095) & ~4095UL;
  // Add one more page as a guard page.
  size += 4096;

  void *p = mmap(NULL, size, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK);
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

nk_status nk_thd_create(nk_thd **ret, nk_thd_entrypoint entry, void *data,
                        const nk_thd_attrs *attrs) {
  nk_status status;

  status = NK_ERR_NOMEM;
  NK_AUTOPTR(nk_thd) t = NK_ALLOC(nk_thd);
  if (!t) {
    goto err;
  }

  status = NK_ERR_NOMEM;
  status =
      allocstack(attrs->stacksize, &thd->stack, &thd->stacktop, &thd->stacklen);

err:
  return status;
}

nk_status nk_thd_set_attrs(nk_thd *thd, const nk_thd_attrs *attrs) {
  // TODO.
}

nk_status nk_thd_get_attrs(nk_thd *thd, nk_thd_attrs *attrs) {}

void nk_thd_yield() {}

void nk_thd_exit(void *retval) {}

void *nk_thd_join(nk_thd *thd) {}

nk_status nk_hostthd_create(nk_hostthd **ret) {}

void nk_hostthd_run(nk_hostthd *thd) {}

void nk_hostthd_destroy(nk_hostthd *thd) {}
