#ifndef __NK_MSG_H__
#define __NK_MSG_H__

#include "kernel.h"
#include "queue.h"
#include "thd.h"

#include <pthread.h>

typedef enum {
  NK_PORT_DPC,  // port spawns a DPC on every incoming message.
  NK_PORT_THD,  // port queues messages and waits for threads to recv them.
} nk_port_type;

typedef struct nk_port {
  pthread_spinlock_t lock;
  queue_head msgs; // message(s) waiting to be received.
  queue_head thds; // thread(s) waiting to receive.
  nk_port_type type;
  nk_dpc_func dpc_func;
  void *dpc_data;
} nk_port;

typedef struct nk_msg {
  nk_port *src;
  nk_port *dest;
  queue_entry port;    // entry in port list
  void *dpc_data;      // DPC data arg when msg is passed to DPC.
  void *data1, *data2; // message args.
} nk_msg;

QUEUE_DEFINE(nk_msg, port);

nk_status nk_msg_create(nk_msg **ret);
void nk_msg_destroy(nk_msg *msg);

nk_status nk_port_create(nk_port **ret, nk_port_type type);
void nk_port_destroy(nk_port *port);
void nk_port_set_dpc(nk_port *port, nk_dpc_func func, void *data);

nk_status nk_msg_send(nk_port *port, nk_msg *msg); // takes ownership of msg.
nk_status nk_msg_recv(nk_port *port, nk_msg **ret);

#endif // __NK_MSG_H__
