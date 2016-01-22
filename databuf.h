#ifndef __NK_DATABUF_H__
#define __NK_DATABUF_H__

#include "kernel.h"

#define NK_DATABUF_SHORTBUF_SIZE 24

typedef struct nk_databuf {
  size_t len;
  union {
    void *data;
    char shortbuf[NK_DATABUF_SHORTBUF_SIZE];
  };
} nk_databuf;

#define NK_DATABUF_LEN(b) ((b)->len)
#define NK_DATABUF_DATA(b)                                                     \
  (NK_DATABUF_LEN(b) > NK_DATABUF_SHORTBUF_SIZE ? b->data : &b->shortbuf)

nk_status nk_databuf_new(nk_databuf **ret, size_t prealloc);
void nk_databuf_free(nk_databuf *buf);
nk_status nk_databuf_resize(nk_databuf *buf, size_t size);
nk_status nk_databuf_clone(nk_databuf *buf, nk_databuf **ret);

#endif // __NK_DATABUF_H__
