#ifndef __NK_KERNEL_H__
#define __NK_KERNEL_H__

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef enum nk_status {
  NK_OK,
  NK_ERR_STATE,    // invalid state
  NK_ERR_PARAM,    // invalid parameter
  NK_ERR_NOMEM,    // no memory
  NK_ERROR_UNIMPL, // not implemented
} nk_status;

void nk_init();

#define NK_ALLOC(type) ((type *)calloc(sizeof(type), 1))
#define NK_ALLOCN(type, n) ((type *)calloc(sizeof(type), (n)))
#define NK_ALLOCBYTES(type, n) ((type *)calloc((n), 1))
#define NK_FREE(ptr) free(ptr)

static inline void __nk_free_cleanup(void *p) { NK_FREE(p); }
#define NK_AUTOPTR(type) __attribute__((cleanup(__nk_free_cleanup))) type *
#define NK_AUTOPTR_STEAL(type, p)                                              \
  ({                                                                           \
    type *ret = p;                                                             \
    p = NULL;                                                                  \
    ret;                                                                       \
  })

#endif // __NK_KERNEL_H__
