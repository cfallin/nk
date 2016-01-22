#ifndef __NK_KERNEL_H__
#define __NK_KERNEL_H__

#include <stdint.h>

typedef enum nk_status {
  NK_OK,
  NK_ERR_STATE, // invalid state
  NK_ERR_PARAM, // invalid parameter
  NK_ERR_NOMEM, // no memory
  NK_ERROR_UNIMPL,  // not implemented
} nk_status;

void nk_init();

#endif // __NK_KERNEL_H__
