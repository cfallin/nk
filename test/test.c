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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

typedef struct test_entry test_entry;
struct test_entry {
  test_entry *next;
  const char *name;
  nk_test_func func;
};

static test_entry *test_head = NULL;
static test_entry *test_tail = NULL;

void nk_test_register(const char *name, nk_test_func func) {
  test_entry *e = calloc(sizeof(test_entry), 1);
  e->name = name;
  e->func = func;
  e->next = NULL;

  if (!test_head) {
    test_head = test_tail = e;
  } else {
    test_tail->next = e;
    test_tail = e;
  }
}

int nk_run_tests() {
  for (test_entry *e = test_head; e; e = e->next) {
    fprintf(stderr, "Running test: %s\n", e->name);
    if (e->func(stderr)) {
      fprintf(stderr, "FAILED: %s\n", e->name);
      return 1;
    }
    fprintf(stderr, "...done.\n");
  }
  fprintf(stderr, "All tests passed.\n");
  return 0;
}

void nk_test_error_report(FILE *f, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(f, fmt, args);
  va_end(args);
}
