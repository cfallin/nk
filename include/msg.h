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

#ifndef __NK_MSG_H__
#define __NK_MSG_H__

#include "kernel.h"
#include "queue.h"
#include "thd.h"
#include "alloc.h"

#include <pthread.h>

typedef enum {
  NK_PORT_DPC, // port spawns a DPC on every incoming message.
  NK_PORT_THD, // port queues messages and waits for threads to recv them.
} nk_port_type;

typedef struct nk_port {
  nk_host *host;
  pthread_spinlock_t lock;
  queue_head msgs; // message(s) waiting to be received.
  queue_head thds; // thread(s) waiting to receive.
  nk_port_type type;
  nk_dpc_func dpc_func;
  void *dpc_data;
} nk_port;

typedef struct nk_msg {
  nk_host *host;
  nk_port *src;
  nk_port *dest;
  queue_entry port; // entry in port list.
  void *dpc_data;   // DPC data arg when msg is passed to DPC. Will be filled in
                    // by the message-receive code when spawning the DPC.
  void *data1, *data2; // message args. Meaning is user-defined.
} nk_msg;

QUEUE_DEFINE(nk_msg, port);

/**
 * Allocate a new message.
 */
nk_status nk_msg_create(nk_host *h, nk_msg **ret);

/**
 * Free a message.
 */
void nk_msg_destroy(nk_msg *msg);

/**
 * Create a new port. It may be either a thread port (on which threads can
 * block to receive messages) or a DPC port (which spawns a new DPC each time a
 * message is received).
 */

nk_status nk_port_create(nk_host *h, nk_port **ret, nk_port_type type);

/**
 * Destroy this port. Behavior is undefined if any receivers are blocked
 * (caller of this function must synchronize with all users of the port first).
 */
void nk_port_destroy(nk_port *port);

/**
 * Set the parameters for the DPC that will be created when this port receives
 * a message. Only valid for DPC ports.
 */
void nk_port_set_dpc(nk_port *port, nk_dpc_func func, void *data);

/**
 * Send a message to a port. Never blocks. However, must be called from within
 * the context of a thread or DPC running on the receiving thread/DPC's host
 * instance. Note that `from` may be NULL.
 */
nk_status nk_msg_send(nk_port *port, nk_port *from, void *data1, void *data2);

/**
 * Receive a message from a port, blocking until one is received. The port must
 * be a thread port, not a DPC port. User receives ownership of the message and
 * must call nk_msg_destroy() when done with it.
 */
nk_status nk_msg_recv(nk_port *port, nk_msg **ret);

// Internal only.
nk_status nk_msg_init_freelists(nk_host *h);
void nk_msg_destroy_freelists(nk_host *h);

#endif // __NK_MSG_H__
