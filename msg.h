#ifndef __NK_MSG_H__
#define __NK_MSG_H__

#include "kernel.h"
#include "queue.h"
#include "databuf.h"

typedef struct nk_msg {
  nk_thd *src;
  nk_thd *dest;
  queue_entry mailboxq;
  nk_databuf data;
};

nk_status nk_msg_new(nk_msg **ret);
void nk_msg_free(nk_msg *msg);

nk_status nk_msg_send(nk_msg *msg); // takes ownership.
nk_status nk_msg_recv(nk_msg **ret);

#endif // __NK_MSG_H__
