#ifndef __NK_MSG_H__
#define __NK_MSG_H__

#include "kernel.h"
#include "queue.h"
#include "databuf.h"

typedef enum {
    NK_PORT_DPC,
    NK_PORT_THD,
} nk_port_type;

typedef struct nk_port {
    queue_head inbox;
    nk_port_type type;
} nk_port;

typedef struct nk_msg {
  nk_por *src;
  nk_port *dest;
  queue_entry mailboxq;
  nk_databuf data;
} nk_msg;

nk_status nk_msg_new(nk_msg **ret);
void nk_msg_free(nk_msg *msg);

nk_status nk_msg_send(nk_msg *msg); // takes ownership.
nk_status nk_msg_recv(nk_msg **ret);

#endif // __NK_MSG_H__
