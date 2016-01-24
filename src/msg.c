#include "msg.h"

#include <assert.h>
#include <pthread.h>

nk_status nk_msg_create(nk_msg **ret) {
  nk_status status;

  status = NK_ERR_NOMEM;
  NK_AUTOPTR(nk_msg) m = NK_ALLOC(nk_msg);
  if (!m) {
    goto err;
  }

  QUEUE_INIT(&m->port);

  *ret = NK_AUTOPTR_STEAL(nk_msg, m);
  return NK_OK;
err:
  return status;
}

void nk_msg_destroy(nk_msg *msg) { NK_FREE(msg); }

nk_status nk_port_create(nk_port **ret, nk_port_type type) {
  nk_status status;

  status = NK_ERR_NOMEM;
  NK_AUTOPTR(nk_port) p = NK_ALLOC(nk_port);
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

  *ret = NK_AUTOPTR_STEAL(nk_port, p);
  return NK_OK;
err:
  return status;
}

void nk_port_destroy(nk_port *port) {
  if (!port) {
    return;
  }
  assert(nk_msg_port_empty(&port->msgs));
  assert(nk_schob_runq_empty(&port->thds));
  pthread_spin_destroy(&port->lock);
  NK_FREE(port);
}

void nk_port_set_dpc(nk_port *port, nk_dpc_func func, void *data) {
  assert(port->type == NK_PORT_DPC);
  port->dpc_func = func;
  port->dpc_data = data;
}

nk_status nk_msg_send(nk_port *port, nk_port *from, void *data1, void *data2) {
  nk_msg *msg;
  nk_status status = nk_msg_create(&msg);
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
      nk_hostthd *hostthd = nk_hostthd_self();
      assert(hostthd != NULL);
      t->recvslot = msg;
      t->schob.state = NK_SCHOB_STATE_READY;
      nk_schob_enqueue(hostthd->host, (nk_schob *)t, /* new_schob = */ 0);
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
    self->schob.state = NK_SCHOB_STATE_WAITING;
    nk_schob_runq_push(&port->thds, (nk_schob *)self);
    pthread_spin_unlock(&port->lock);
    nk_thd_yield();
    assert(self->recvslot);
    *ret = self->recvslot;
    self->recvslot = NULL;
    return NK_OK;
  }
}
