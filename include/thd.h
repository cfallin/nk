#ifndef __NK_THD_H__
#define __NK_THD_H__

#include "kernel.h"
#include "queue.h"
#include "databuf.h"

typedef enum nk_thd_state {
  NK_THD_READY,
  NK_THD_RUNNING,
  NK_THD_WAIT_MSG,
  NK_THD_FINISHED,
} nk_thd_state;

typedef struct nk_thd_attrs {
  // Priority: higher is preferred.
  uint32_t prio;
  // Stack size.
  uint32_t stacksize;
} nk_thd_attrs;

#define NK_THD_ATTR_INIT                                                       \
  { NK_THD_PRIO_DEFAULT, NK_THD_STACKSIZE_DEFAULT }

#define NK_THD_PRIO_MIN 0
#define NK_THD_PRIO_DEFAULT 0x80000000
#define NK_THD_PRIO_MAX 0xffffffff

#define NK_THD_STACKSIZE_MIN 4096
#define NK_THD_STACKSIZE_DEFAULT 8192
#define NK_THD_STACKSIZE_MAX (4*1024*1024)

typedef struct nk_thd {
  void *stack;
  void *stacktop;
  size_t stacklen;
  void *retval;
  nk_thd_state state;
  nk_thd_attrs attrs;
  queue_entry thdq;
  queue_head mailbox; // queue of messages sent to us
} nk_thd;

QUEUE_DEFINE(nk_thd, thdq);

typedef void *(*nk_thd_entrypoint)(nk_thd *self, void *data);

nk_status nk_thd_create(nk_thd **ret, nk_thd_entrypoint entry, void *data,
                        const nk_thd_attrs *attrs);
nk_status nk_thd_set_attrs(nk_thd *thd, const nk_thd_attrs *attrs);
nk_status nk_thd_get_attrs(nk_thd *thd, nk_thd_attrs *attrs);
void nk_thd_yield();
void nk_thd_exit(void *retval);
void *nk_thd_join(nk_thd *thd);

struct nk_hostthd;
typedef struct nk_hostthd nk_hostthd;

nk_status nk_hostthd_create(nk_hostthd **ret);
void nk_hostthd_run(nk_hostthd *thd);
void nk_hostthd_destroy(nk_hostthd *thd);

#endif // __NK_THD_H__
