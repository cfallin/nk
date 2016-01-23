#include "test.h"
#include "kernel.h"
#include "thd.h"

static void thd_main_dpc_main(void *arg) {
  int *flag = arg;
  *flag = 42;
}

NK_TEST(thd_main_dpc) {
  nk_host *host;
  nk_status status;
  status = nk_host_create(&host);
  NK_TEST_ASSERT(status == NK_OK);

  int dpcflag = 0;
  nk_host_run(host, 4, thd_main_dpc_main, &dpcflag);
  NK_TEST_ASSERT(dpcflag == 42);
  nk_host_destroy(host);

  NK_TEST_OK();
}

static void thd_two_threads_thdbody(nk_thd *self, void *arg) {
  int *flag = arg;
  *flag = 1;
}

struct thd_two_threads_arg {
  int flag0, flag1, flag2;
};

static void thd_two_threads_main(void *_arg) {
  struct thd_two_threads_arg *arg = _arg;

  nk_thd *thd1, *thd2;
  if (nk_thd_create(&thd1, thd_two_threads_thdbody, &arg->flag1, NULL) !=
      NK_OK) {
    return;
  }

  if (nk_thd_create(&thd2, thd_two_threads_thdbody, &arg->flag2, NULL) !=
      NK_OK) {
    return;
  }

  arg->flag0 = 1;
}

NK_TEST(thd_two_threads) {
  nk_host *host;
  nk_status status;
  status = nk_host_create(&host);
  NK_TEST_ASSERT(status == NK_OK);

  struct thd_two_threads_arg arg = {0, 0, 0};
  nk_host_run(host, 4, thd_two_threads_main, &arg);
  NK_TEST_ASSERT(arg.flag0 == 1);
  NK_TEST_ASSERT(arg.flag1 == 1);
  NK_TEST_ASSERT(arg.flag2 == 1);
  nk_host_destroy(host);

  NK_TEST_OK();
}
