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

#include "test.h"
#include "kernel.h"
#include "thd.h"

static void thd_dpc_main(void *arg) {
  int *flag = arg;
  *flag = 42;
}

NK_TEST(thd_dpc) {
  nk_host *host;
  nk_status status;
  status = nk_host_create(&host);
  NK_TEST_ASSERT(status == NK_OK);

  int dpcflag = 0;
  nk_dpc *dpc;
  nk_dpc_create_ext(host, &dpc, &thd_dpc_main, &dpcflag);
  nk_host_run(host, 4);
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
  if (nk_thd_create(&thd1, thd_two_threads_thdbody, &arg->flag1) != NK_OK) {
    return;
  }

  if (nk_thd_create(&thd2, thd_two_threads_thdbody, &arg->flag2) != NK_OK) {
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
  nk_dpc *dpc;
  nk_dpc_create_ext(host, &dpc, &thd_two_threads_main, &arg);
  nk_host_run(host, 4);
  NK_TEST_ASSERT(arg.flag0 == 1);
  NK_TEST_ASSERT(arg.flag1 == 1);
  NK_TEST_ASSERT(arg.flag2 == 1);
  nk_host_destroy(host);

  NK_TEST_OK();
}
