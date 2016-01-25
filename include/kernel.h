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

#ifndef __NK_KERNEL_H__
#define __NK_KERNEL_H__

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef enum nk_status {
  NK_OK,
  NK_ERR_STATE,  // invalid state
  NK_ERR_PARAM,  // invalid parameter
  NK_ERR_NOMEM,  // no memory
  NK_ERR_UNIMPL, // not implemented
  NK_ERR_NORECV, // no DPC receiver set up on port
} nk_status;

#define NK_ALLOC(type) ((type *)calloc(sizeof(type), 1))
#define NK_ALLOCN(type, n) ((type *)calloc(sizeof(type), (n)))
#define NK_ALLOCBYTES(type, n) ((type *)calloc((n), 1))
#define NK_FREE(ptr) free(ptr)

static inline void __nk_free_cleanup(void *p) { NK_FREE(*(void **)p); }
#define NK_AUTOPTR(type) __attribute__((cleanup(__nk_free_cleanup))) type *
#define NK_AUTOPTR_STEAL(type, p)                                              \
  ({                                                                           \
    type *ret = p;                                                             \
    p = NULL;                                                                  \
    ret;                                                                       \
  })

#endif // __NK_KERNEL_H__
