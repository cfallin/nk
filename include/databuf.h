#ifndef __NK_DATABUF_H__
#define __NK_DATABUF_H__

#include "kernel.h"

#define NK_DATABUF_SHORTBUF_SIZE 16

typedef struct nk_databuf {
  size_t len;
  size_t cap;
  union {
    uint8_t *data;
    uint8_t shortbuf[NK_DATABUF_SHORTBUF_SIZE];
  };
} nk_databuf;

#define NK_DATABUF_LEN(b) ((b)->len)
#define NK_DATABUF_CAP(b) ((b)->cap)
#define NK_DATABUF_ISLONG(b) ((b)->cap > NK_DATABUF_SHORTBUF_SIZE)
#define NK_DATABUF_ISALLOC(b) (NK_DATABUF_ISLONG(b) && (b)->data != (uint8_t*)((b) + 1))
#define NK_DATABUF_DATA(b) (NK_DATABUF_ISLONG(b) ? (b)->data : (b)->shortbuf)

nk_status nk_databuf_new(nk_databuf **ret, size_t capacity);
void nk_databuf_free(nk_databuf *buf);
nk_status nk_databuf_resize(nk_databuf *buf, size_t len, int fill);
nk_status nk_databuf_reserve(nk_databuf *buf, size_t capacity);
nk_status nk_databuf_clone(nk_databuf *buf, nk_databuf **ret, size_t capacity);

#endif // __NK_DATABUF_H__
