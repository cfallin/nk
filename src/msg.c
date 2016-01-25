#include "msg.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>

nk_status nk_msg_create(nk_host *h, nk_msg **ret) {
  nk_status status;

  status = NK_ERR_NOMEM;
  nk_msg *m = nk_freelist_alloc(&h->msg_freelist);
  if (!m) {
    goto err;
  }

  QUEUE_INIT(&m->port);
  m->host = h;

  *ret = m;
  return NK_OK;
err:
  if (m) {
    nk_freelist_free(&h->msg_freelist, m);
  }
  return status;
}

void nk_msg_destroy(nk_msg *msg) {
  nk_freelist_free(&msg->host->msg_freelist, msg);
}

nk_status nk_port_create(nk_host *h, nk_port **ret, nk_port_type type) {
  nk_status status;

  status = NK_ERR_NOMEM;
  nk_port *p = nk_freelist_alloc(&h->port_freelist);
  if (!p) {
    goto err;
  }

  status = NK_ERR_NOMEM;
  if (pthread_spin_init(&p->lock, PTHREAD_PROCESS_PRIVATE)) {
    goto err;
  }

  QUEUE_INIT(&p->msgs);
  QUEUE_INIT(&p->thds);

  p->type = type;
  p->host = h;

  *ret = p;
  return NK_OK;
err:
  nk_freelist_free(&h->port_freelist, p);
  return status;
}

void nk_port_destroy(nk_port *port) {
  if (!port) {
    return;
  }
  assert(nk_msg_port_empty(&port->msgs));
  assert(nk_schob_runq_empty(&port->thds));
  pthread_spin_destroy(&port->lock);
  nk_freelist_free(&port->host->port_freelist, port);
}

void nk_port_set_dpc(nk_port *port, nk_dpc_func func, void *data) {
  assert(port->type == NK_PORT_DPC);
  port->dpc_func = func;
  port->dpc_data = data;
}

nk_status nk_msg_send(nk_port *port, nk_port *from, void *data1, void *data2) {
  nk_hostthd *hostthd = nk_hostthd_self();
  assert(hostthd != NULL);
  nk_host *host = hostthd->host;
  nk_msg *msg;
  nk_status status = nk_msg_create(port->host, &msg);
  if (status != NK_OK) {
    return status;
  }

  msg->data1 = data1;
  msg->data2 = data2;
  msg->src = from;
  msg->dest = port;

  if (port->type == NK_PORT_DPC) {
    if (port->dpc_func) {
      nk_dpc *new_dpc;
      msg->dpc_data = port->dpc_data;
      return nk_dpc_create(&new_dpc, port->dpc_func, msg, NULL);
    } else {
      return NK_ERR_NORECV;
    }
  } else if (port->type == NK_PORT_THD) {
    pthread_spin_lock(&port->lock);
    if (!nk_schob_runq_empty(&port->thds)) {
      // There's at least one thread waiting to receive: deliver right away.
      nk_thd *t = (nk_thd *)nk_schob_runq_shift(&port->thds);
      pthread_spin_unlock(&port->lock);
      t->recvslot = msg;
      nk_schob_enqueue(host, (nk_schob *)t, /* new_schob = */ 0);
      return NK_OK;
    } else {
      // No threads are waiting to receive: enqueue the message.
      nk_msg_port_push(&port->msgs, msg);
      pthread_spin_unlock(&port->lock);
      return NK_OK;
    }
  } else {
    assert(0);
    return NK_OK;
  }
}

nk_status nk_msg_recv(nk_port *port, nk_msg **ret) {
  assert(port->type == NK_PORT_THD);
  nk_thd *self = nk_thd_self();
  pthread_spin_lock(&port->lock);
  if (!nk_msg_port_empty(&port->msgs)) {
    nk_msg *m = nk_msg_port_shift(&port->msgs);
    pthread_spin_unlock(&port->lock);
    *ret = m;
    return NK_OK;
  } else {
    nk_schob_runq_push(&port->thds, (nk_schob *)self);
    pthread_spin_unlock(&port->lock);
    // Note that this gap between unlock and yield is nevertheless safe from
    // race conditions: we indicate via the yield to the host thread scheduler
    // that we're waiting, so the host thread scheduler will not place us back
    // on the runqueue as it would for an ordinary yield. Rather, the port
    // itself logically owns this thread now. It could be the case that some
    // other thread concurrently delivers a message and places us back on the
    // runqueue before we even reach this yield, but that's OK, because then
    // we'll simply wake up when next scheduled off the runqueue. (Note that
    // the thread-running lock prevents another host thread from jumping to our
    // context before we leave it here.)
    nk_thd_yield_ext(NK_THD_YIELD_REASON_WAITING);
    assert(self->recvslot);
    *ret = self->recvslot;
    self->recvslot = NULL;
    return NK_OK;
  }
}

DEFINE_SIMPLE_FREELIST_TYPE(nk_msg, 10000);
DEFINE_SIMPLE_FREELIST_TYPE(nk_port, 10000);

nk_status nk_msg_init_freelists(nk_host *h) {
  nk_status status;
  if ((status = nk_freelist_init(&h->msg_freelist, &nk_msg_freelist_attrs,
                                 NULL)) != NK_OK) {
    return status;
  }
  if ((status = nk_freelist_init(&h->port_freelist, &nk_port_freelist_attrs,
                                 NULL)) != NK_OK) {
    nk_freelist_destroy(&h->msg_freelist);
    return status;
  }
  return NK_OK;
}

void nk_msg_destroy_freelists(nk_host *h) {
  nk_freelist_destroy(&h->msg_freelist);
  nk_freelist_destroy(&h->port_freelist);
}
