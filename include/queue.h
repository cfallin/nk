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

#ifndef __NK_QUEUE_H__
#define __NK_QUEUE_H__

#include <stddef.h>

typedef struct queue_entry queue_entry;
struct queue_entry {
  queue_entry *prev;
  queue_entry *next;
};

typedef struct queue_entry queue_head;

#define QUEUE_INIT(head)                                                       \
  do {                                                                         \
    (head)->next = (head);                                                     \
    (head)->prev = (head);                                                     \
  } while (0)
#define QUEUE_INSERT_ENTRY_AFTER(pos, entry)                                   \
  do {                                                                         \
    queue_entry *e = (entry);                                                  \
    e->prev = (pos);                                                           \
    e->next = (pos)->next;                                                     \
    e->prev->next = e;                                                         \
    e->next->prev = e;                                                         \
  } while (0)

#define QUEUE_REMOVE_ENTRY(entry)                                              \
  do {                                                                         \
    queue_entry *e = (entry);                                                  \
    e->prev->next = e->next;                                                   \
    e->next->prev = e->prev;                                                   \
  } while (0)

#define QUEUE_OBJ_FROM_ENTRY(type, field, entry)                               \
  ((type *)(((char *)(entry)) - offsetof(type, field)))
#define QUEUE_ENTRY_FROM_OBJ(type, field, obj)                                 \
  ((queue_entry *)(((char *)(obj)) + offsetof(type, field)))

#define QUEUE_DEFINE(type, field)                                              \
  static void type##_##field##_remove(type *obj) {                             \
    QUEUE_REMOVE_ENTRY(QUEUE_ENTRY_FROM_OBJ(type, field, obj));                \
  }                                                                            \
  static void type##_##field##_insert_after(type *pos, type *obj) {            \
    QUEUE_INSERT_ENTRY_AFTER(QUEUE_ENTRY_FROM_OBJ(type, field, pos),           \
                             QUEUE_ENTRY_FROM_OBJ(type, field, obj));          \
  }                                                                            \
  static void type##_##field##_unshift(queue_head *head, type *obj) {          \
    QUEUE_INSERT_ENTRY_AFTER(head->prev,                                       \
                             QUEUE_ENTRY_FROM_OBJ(type, field, obj));          \
  }                                                                            \
  static void type##_##field##_push(queue_head *head, type *obj) {             \
    QUEUE_INSERT_ENTRY_AFTER(head->next,                                       \
                             QUEUE_ENTRY_FROM_OBJ(type, field, obj));          \
  }                                                                            \
  static int type##_##field##_empty(queue_head *head) {                        \
    return (head->next == head) ? 1 : 0;                                       \
  }                                                                            \
  static type *type##_##field##_shift(queue_head *head) {                      \
    if (type##_##field##_empty(head)) {                                        \
      return NULL;                                                             \
    } else {                                                                   \
      type *ret = QUEUE_OBJ_FROM_ENTRY(type, field, head->next);               \
      QUEUE_REMOVE_ENTRY(head->next);                                          \
      return ret;                                                              \
    }                                                                          \
  }                                                                            \
  static type *type##_##field##_pop(queue_head *head) {                        \
    if (type##_##field##_empty(head)) {                                        \
      return NULL;                                                             \
    } else {                                                                   \
      type *ret = QUEUE_OBJ_FROM_ENTRY(type, field, head->prev);               \
      QUEUE_REMOVE_ENTRY(head->prev);                                          \
      return ret;                                                              \
    }                                                                          \
  }                                                                            \
  static type *type##_##field##_begin(queue_head *head) {                      \
    return QUEUE_OBJ_FROM_ENTRY(type, field, head->next);                      \
  }                                                                            \
  static type *type##_##field##_end(queue_head *head) {                        \
    return QUEUE_OBJ_FROM_ENTRY(type, field, head->prev);                      \
  }                                                                            \
  static type *type##_##field##_next(type *obj) {                              \
    return QUEUE_OBJ_FROM_ENTRY(type, field,                                   \
                                QUEUE_ENTRY_FROM_OBJ(type, field, obj)->next); \
  }                                                                            \
  static type *type##_##field##_prev(type *obj) {                              \
    return QUEUE_OBJ_FROM_ENTRY(type, field,                                   \
                                QUEUE_ENTRY_FROM_OBJ(type, field, obj)->prev); \
  }

#endif // __NK_QUEUE_H__
