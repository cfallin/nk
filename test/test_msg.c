#include "msg.h"
#include "test.h"

struct msg_cross_messages_arg {
  nk_port *this_port, *other_port;
  int flag;
  int value;
};

static void msg_cross_messages_thd(nk_thd *self, void *_arg) {
  struct msg_cross_messages_arg *arg = _arg;
  if (nk_msg_send(arg->other_port, arg->this_port, &arg->flag, NULL) != NK_OK) {
    return;
  }
  nk_msg *m;
  if (nk_msg_recv(arg->this_port, &m) != NK_OK) {
    return;
  }
  if (m->src != arg->other_port || m->dest != arg->this_port) {
    return;
  }
  int *other_flag = m->data1;
  *other_flag = arg->value;
  nk_msg_destroy(m);
}

NK_TEST(msg_cross_messages) {
  nk_host *h;
  NK_TEST_ASSERT(nk_host_create(&h) == NK_OK);

  nk_thd *thd1, *thd2;
  struct msg_cross_messages_arg arg1, arg2;
  arg1.flag = arg2.flag = 0;
  arg1.value = 84;
  arg2.value = 42;

  NK_TEST_ASSERT(nk_port_create(&arg1.this_port, NK_PORT_THD) == NK_OK);
  NK_TEST_ASSERT(nk_port_create(&arg2.this_port, NK_PORT_THD) == NK_OK);
  arg1.other_port = arg2.this_port;
  arg2.other_port = arg1.this_port;

  NK_TEST_ASSERT(nk_thd_create_ext(h, &thd1, msg_cross_messages_thd, &arg1,
                                   NULL) == NK_OK);
  NK_TEST_ASSERT(nk_thd_create_ext(h, &thd2, msg_cross_messages_thd, &arg2,
                                   NULL) == NK_OK);
  nk_host_run(h, 2, NULL, NULL);
  nk_host_destroy(h);

  NK_TEST_ASSERT(arg1.flag == 42);
  NK_TEST_ASSERT(arg2.flag == 84);

  NK_TEST_OK();
}
