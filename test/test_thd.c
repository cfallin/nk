#include "test.h"
#include "kernel.h"
#include "thd.h"

static void *thd_create_main_dpc(void *arg) {
  int *flag = arg;
  *flag = 42;
  return (flag + 1);
}

NK_TEST(thd_create) {
  nk_host *host;
  nk_status status;

  status = nk_host_create(&host);
  NK_TEST_ASSERT(status == NK_OK);
  int dpcflag = 0;
  void *retval = nk_host_run(host, 4, thd_create_main_dpc, &dpcflag);
  NK_TEST_ASSERT(dpcflag == 42);
  NK_TEST_ASSERT(retval == (int *)&dpcflag + 1);
  nk_host_destroy(host);

  NK_TEST_OK();
}
