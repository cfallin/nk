#include "kernel.h"
#include "databuf.h"

nk_status nk_databuf_new(nk_databuf **ret, size_t capacity) {
  NK_AUTOPTR(nk_databuf) b = NULL;
  if (capacity > NK_DATABUF_SHORTBUF_SIZE) {
    b = NK_ALLOCBYTES(nk_databuf, sizeof(nk_databuf) + capacity);
    if (!b)
      goto err;
    b->data = (uint8_t *)(b + 1);
  } else {
    b = NK_ALLOC(nk_databuf);
    if (!b)
      goto err;
  }
  b->cap = capacity;
  b->len = 0;
  *ret = NK_AUTOPTR_STEAL(nk_databuf, b);
  return NK_OK;

err:
  return NK_ERR_NOMEM;
}

void nk_databuf_free(nk_databuf *buf) {
  if (!buf) {
    return;
  }
  if (NK_DATABUF_ISALLOC(buf)) {
    NK_FREE(buf->data);
  }
  NK_FREE(buf);
}

nk_status nk_databuf_resize(nk_databuf *buf, size_t len, int fill) {
  if (!buf) {
    return NK_ERR_PARAM;
  }
  if (len > NK_DATABUF_CAP(buf)) {
    nk_status err = nk_databuf_reserve(buf, len);
    if (err != NK_OK) {
      return err;
    }
  }
  if ((len > buf->len) && fill) {
    memset(NK_DATABUF_DATA(buf) + buf->len, (len - buf->len), 0);
  }
  buf->len = len;
  return NK_OK;
}

nk_status nk_databuf_reserve(nk_databuf *buf, size_t capacity) {
  if (!buf) {
    return NK_ERR_PARAM;
  }
  if (capacity <= buf->cap) {
    return NK_OK;
  }
  if (capacity <= NK_DATABUF_SHORTBUF_SIZE) {
    buf->cap = capacity;
    return NK_OK;
  }

  uint8_t *newbuf = NK_ALLOCBYTES(void, capacity);
  if (!newbuf) {
    return NK_ERR_NOMEM;
  }
  memcpy(newbuf, NK_DATABUF_DATA(buf), buf->len);
  if (NK_DATABUF_ISALLOC(buf)) {
    NK_FREE(buf->data);
  }
  buf->data = newbuf;
  buf->cap = capacity;
  return NK_OK;
}

nk_status nk_databuf_clone(nk_databuf *buf, nk_databuf **ret, size_t capacity) {
  nk_status err;
  if (!buf) {
    *ret = NULL;
    return NK_OK;
  }

  if (capacity < NK_DATABUF_LEN(buf)) {
    capacity = NK_DATABUF_LEN(buf);
  }
  nk_databuf *b;
  if ((err = nk_databuf_new(&b, capacity)) != NK_OK) {
    return err;
  }

  memcpy(NK_DATABUF_DATA(b), NK_DATABUF_DATA(buf), NK_DATABUF_LEN(buf));
  b->len = buf->len;
  *ret = b;
  return NK_OK;
}
