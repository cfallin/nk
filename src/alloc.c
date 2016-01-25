#include "alloc.h"

#include <pthread.h>
#include <string.h>

static void *nk_freelist_alloc_default(const nk_freelist_attrs *attrs,
                                       void *cookie) {
  return calloc(attrs->node_size, 1);
}

static void nk_freelist_free_default(const nk_freelist_attrs *attrs,
                                     void *cookie, void *p) {
  free(p);
}

static void nk_freelist_zero_default(const nk_freelist_attrs *attrs,
                                     void *cookie, void *p) {
  memset(p, 0, attrs->node_size);
}

#define FREELIST_OBJ_FROM_NODE(f, node)                                        \
  ((void *)((char *)(node) - (f)->attrs.freelist_header_offset))

#define FREELIST_NODE_FROM_OBJ(f, obj)                                         \
  ((void *)((char *)(obj) + (f)->attrs.freelist_header_offset))

nk_status nk_freelist_init(nk_freelist *f, const nk_freelist_attrs *attrs,
                           void *cookie) {
  if (pthread_spin_init(&f->lock, PTHREAD_PROCESS_PRIVATE)) {
    return NK_ERR_NOMEM;
  }

  memcpy(&f->attrs, attrs, sizeof(nk_freelist_attrs));
  if (!f->attrs.alloc_func) {
    f->attrs.alloc_func = nk_freelist_alloc_default;
  }
  if (!f->attrs.free_func) {
    f->attrs.free_func = nk_freelist_free_default;
  }
  if (!f->attrs.zero_func) {
    f->attrs.zero_func = nk_freelist_zero_default;
  }
  f->cookie = cookie;

  f->count = 0;
  f->freelist_head = NULL;

  return NK_OK;
}

void nk_freelist_destroy(nk_freelist *f) {
  for (nk_freelist_node *n = f->freelist_head, *next = NULL; n; n = next) {
    next = n->next;
    f->attrs.free_func(&f->attrs, f->cookie, FREELIST_OBJ_FROM_NODE(f, n));
  }
  pthread_spin_destroy(&f->lock);
}

void *nk_freelist_alloc(nk_freelist *f) {
  pthread_spin_lock(&f->lock);
  if (f->count > 0) {
    nk_freelist_node *n = f->freelist_head;
    f->freelist_head = n->next;
    f->count--;
    pthread_spin_unlock(&f->lock);
    void *obj = FREELIST_OBJ_FROM_NODE(f, n);
    f->attrs.zero_func(&f->attrs, f->cookie, obj);
    return obj;
  } else {
    pthread_spin_unlock(&f->lock);
    return f->attrs.alloc_func(&f->attrs, f->cookie);
  }
}

void nk_freelist_free(nk_freelist *f, void *p) {
  size_t max_count = f->attrs.max_count;
  pthread_spin_lock(&f->lock);
  if (f->count >= max_count) {
    pthread_spin_unlock(&f->lock);
    f->attrs.free_func(&f->attrs, f->cookie, p);
  } else {
    nk_freelist_node *n = FREELIST_NODE_FROM_OBJ(f, p);
    n->next = f->freelist_head;
    f->freelist_head = n;
    f->count++;
    pthread_spin_unlock(&f->lock);
  }
}
