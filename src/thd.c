#include "thd.h"

#include <pthread.h>

// Global scheduler state.
struct nk_schedstate {
    queue_head runq;   // threads ready to run.
    queue_head waitq;  // threads waiting on messages.
};
static struct nk_schedstate sched;

// Host-thread state.
struct nk_hostthd {
    pthread_t pthread;
    queue_entry queue;
    nk_thd* running;
};

QUEUE_DEFINE(nk_hostthd, queue);

nk_status nk_thd_create(nk_thd **ret, nk_thd_entrypoint entry, void *data,
                        const nk_thd_attrs *attrs) {
  return NK_ERROR_UNIMPL;
}

nk_status nk_thd_set_attrs(nk_thd *thd, const nk_thd_attrs *attrs) {
    // TODO.
}

nk_status nk_thd_get_attrs(nk_thd *thd, nk_thd_attrs *attrs) {
}

void nk_thd_yield() {
}

void nk_thd_exit(void *retval) {
}

void *nk_thd_join(nk_thd *thd) {
}

nk_status nk_hostthd_create(nk_hostthd **ret) {
}

void nk_hostthd_run(nk_hostthd *thd) {
}

void nk_hostthd_destroy(nk_hostthd *thd) {
}
