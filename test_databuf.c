#include "test.h"
#include "databuf.h"

NK_TEST(databuf_alloc) {
    nk_databuf* b;

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
    NK_TEST_ASSERT(NK_DATABUF_DATA(b) == b + 1);
    nk_databuf_free(b);

    NK_TEST_OK();
}

NK_TEST(databuf_reserve) {
    nk_databuf* b;

    NK_TEST_ASSERT(nk_databuf_new(&b, 0) == NK_OK);
    NK_TEST_ASSERT(NK_DATABUF_CAP(b) == 0);

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

    NK_TEST_OK();
}
