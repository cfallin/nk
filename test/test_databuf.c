#include "test.h"
#include "databuf.h"

NK_TEST(databuf_alloc) {
  nk_databuf *b;

  NK_TEST_ASSERT_MSG(NK_DATABUF_SHORTBUF_SIZE == 16,
                     "Unit tests are written with assumption that short buffer "
                     "size is 16 bytes.");

  NK_TEST_ASSERT(nk_databuf_new(&b, 8) == NK_OK);
  NK_TEST_ASSERT(!NK_DATABUF_ISLONG(b));
  NK_TEST_ASSERT(!NK_DATABUF_ISALLOC(b));
  NK_TEST_ASSERT(NK_DATABUF_CAP(b) == 8);
  NK_TEST_ASSERT(NK_DATABUF_LEN(b) == 0);
  nk_databuf_free(b);

  NK_TEST_ASSERT(nk_databuf_new(&b, 32) == NK_OK);
  NK_TEST_ASSERT(NK_DATABUF_ISLONG(b));
  NK_TEST_ASSERT(!NK_DATABUF_ISALLOC(b));
  NK_TEST_ASSERT(NK_DATABUF_CAP(b) == 32);
  NK_TEST_ASSERT(NK_DATABUF_LEN(b) == 0);
  NK_TEST_ASSERT(NK_DATABUF_DATA(b) == (uint8_t *)(b + 1));
  nk_databuf_free(b);

  NK_TEST_OK();
}

NK_TEST(databuf_reserve) {
  nk_databuf *b;

  NK_TEST_ASSERT(nk_databuf_new(&b, 0) == NK_OK);
  NK_TEST_ASSERT(NK_DATABUF_CAP(b) == 0);
  NK_TEST_ASSERT(NK_DATABUF_LEN(b) == 0);

  NK_TEST_ASSERT(nk_databuf_reserve(b, 8) == NK_OK);
  NK_TEST_ASSERT(NK_DATABUF_CAP(b) == 8);
  NK_TEST_ASSERT(NK_DATABUF_LEN(b) == 0);
  NK_TEST_ASSERT(!NK_DATABUF_ISLONG(b));
  NK_TEST_ASSERT(!NK_DATABUF_ISALLOC(b));

  NK_TEST_ASSERT(nk_databuf_reserve(b, 32) == NK_OK);
  NK_TEST_ASSERT(NK_DATABUF_CAP(b) == 32);
  NK_TEST_ASSERT(NK_DATABUF_LEN(b) == 0);
  NK_TEST_ASSERT(NK_DATABUF_ISLONG(b));
  NK_TEST_ASSERT(NK_DATABUF_ISALLOC(b));

  NK_TEST_ASSERT(nk_databuf_reserve(b, 16) == NK_OK);
  NK_TEST_ASSERT(NK_DATABUF_CAP(b) == 32);
  NK_TEST_ASSERT(NK_DATABUF_LEN(b) == 0);

  nk_databuf_free(b);
  NK_TEST_OK();
}

NK_TEST(databuf_resize) {
  nk_databuf *b;

  NK_TEST_ASSERT(nk_databuf_new(&b, 0) == NK_OK);
  NK_TEST_ASSERT(NK_DATABUF_CAP(b) == 0);
  NK_TEST_ASSERT(NK_DATABUF_LEN(b) == 0);

  NK_TEST_ASSERT(nk_databuf_resize(b, 32, /* fill = */ 1) == NK_OK);
  NK_TEST_ASSERT(NK_DATABUF_CAP(b) == 32);
  NK_TEST_ASSERT(NK_DATABUF_LEN(b) == 32);
  for (int i = 0; i < NK_DATABUF_LEN(b); i++) {
    NK_TEST_ASSERT(NK_DATABUF_DATA(b)[i] == 0);
  }

  NK_TEST_ASSERT(nk_databuf_resize(b, 16, 1) == NK_OK);
  NK_TEST_ASSERT(NK_DATABUF_CAP(b) == 32);
  NK_TEST_ASSERT(NK_DATABUF_LEN(b) == 16);

  nk_databuf_free(b);
  NK_TEST_OK();
}

NK_TEST(databuf_clone) {
  nk_databuf *b;

  NK_TEST_ASSERT(nk_databuf_new(&b, 8) == NK_OK);
  NK_TEST_ASSERT(nk_databuf_resize(b, 8, 1) == NK_OK);
  NK_TEST_ASSERT(NK_DATABUF_CAP(b) == 8);
  NK_TEST_ASSERT(NK_DATABUF_LEN(b) == 8);
  NK_TEST_ASSERT(!NK_DATABUF_ISLONG(b));
  NK_TEST_ASSERT(!NK_DATABUF_ISALLOC(b));
  strncpy((char *)NK_DATABUF_DATA(b), "testdat", 8);
  NK_TEST_ASSERT(!strcmp((char *)NK_DATABUF_DATA(b), "testdat"));

  nk_databuf *b2;
  NK_TEST_ASSERT(nk_databuf_clone(b, &b2, 0) == NK_OK);
  NK_TEST_ASSERT(NK_DATABUF_CAP(b2) == 8);
  NK_TEST_ASSERT(NK_DATABUF_LEN(b2) == 8);
  NK_TEST_ASSERT(!NK_DATABUF_ISLONG(b2));
  NK_TEST_ASSERT(!NK_DATABUF_ISALLOC(b2));
  strncpy((char *)NK_DATABUF_DATA(b2), "testdat", 8);
  nk_databuf_free(b2);

  NK_TEST_ASSERT(nk_databuf_clone(b, &b2, 32) == NK_OK);
  NK_TEST_ASSERT(NK_DATABUF_CAP(b2) == 32);
  NK_TEST_ASSERT(NK_DATABUF_LEN(b2) == 8);
  NK_TEST_ASSERT(NK_DATABUF_ISLONG(b2));
  NK_TEST_ASSERT(!NK_DATABUF_ISALLOC(b2));
  strncpy((char *)NK_DATABUF_DATA(b2), "testdat", 8);
  nk_databuf_free(b2);

  NK_TEST_ASSERT(nk_databuf_resize(b, 128, 1) == NK_OK);
  NK_TEST_ASSERT(NK_DATABUF_ISLONG(b));
  NK_TEST_ASSERT(NK_DATABUF_ISALLOC(b));
  for (int i = 0; i < NK_DATABUF_LEN(b); i++) {
    NK_DATABUF_DATA(b)[i] = i;
  }
  NK_TEST_ASSERT(nk_databuf_clone(b, &b2, 256) == NK_OK);
  NK_TEST_ASSERT(NK_DATABUF_CAP(b2) == 256);
  NK_TEST_ASSERT(NK_DATABUF_LEN(b2) == 128);
  NK_TEST_ASSERT(NK_DATABUF_ISLONG(b2));
  NK_TEST_ASSERT(!NK_DATABUF_ISALLOC(b2));
  for (int i = 0; i < NK_DATABUF_LEN(b2); i++) {
    NK_TEST_ASSERT(NK_DATABUF_DATA(b)[i] == (i < NK_DATABUF_LEN(b) ? i : 0));
  }
  nk_databuf_free(b2);

  nk_databuf_free(b);
  NK_TEST_OK();
}
