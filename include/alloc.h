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

#ifndef __NK_ALLOC_H__
#define __NK_ALLOC_H__

#include "kernel.h"

#include <pthread.h>

typedef struct nk_freelist_node nk_freelist_node;
typedef struct nk_freelist_attrs nk_freelist_attrs;
typedef struct nk_freelist nk_freelist;

struct nk_freelist_node {
  nk_freelist_node *next;
};

typedef void *(*nk_freelist_alloc_func)(const nk_freelist_attrs *attrs,
                                        void *cookie);
typedef void (*nk_freelist_free_func)(const nk_freelist_attrs *attrs,
                                      void *cookie, void *p);
typedef void (*nk_freelist_zero_func)(const nk_freelist_attrs *attrs,
                                      void *cookie, void *p);

struct nk_freelist_attrs {
  size_t node_size; // used only by default alloc/free/zero funcs.
  size_t max_count;
  // allows freelist obj header to be offset to avoid e.g. guard pages at first
  // page of thread stacks.
  size_t freelist_header_offset;
  nk_freelist_alloc_func alloc_func;
  nk_freelist_free_func free_func;
  nk_freelist_zero_func zero_func;
};

struct nk_freelist {
  nk_freelist_attrs attrs;
  void *cookie;

  pthread_spinlock_t lock;
  size_t count;
  nk_freelist_node *freelist_head;
};

nk_status nk_freelist_init(nk_freelist *f, const nk_freelist_attrs *attrs,
                           void *cookie);
void nk_freelist_destroy(nk_freelist *f);
void *nk_freelist_alloc(nk_freelist *f);
void nk_freelist_free(nk_freelist *f, void *p);

#define DEFINE_SIMPLE_FREELIST_TYPE(type, count)                               \
  static nk_freelist_attrs type##_freelist_attrs = {                           \
      .node_size = sizeof(type),                                               \
      .max_count = count,                                                      \
      .freelist_header_offset = 0,                                             \
      .alloc_func = NULL,                                                      \
      .free_func = NULL,                                                       \
      .zero_func = NULL,                                                       \
  }

#endif // __NK_ALLOC_H__
