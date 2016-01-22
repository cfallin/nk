#ifndef __NK_TEST_H__
#define __NK_TEST_H__

#include <stdio.h>

// Run all registered test suites.
int nk_run_tests();

typedef int (*nk_test_func)(FILE *__test_out);
void nk_test_register(const char *name, nk_test_func func);
void nk_test_error_report(FILE *out, const char *fmt, ...);

// Define a test suite.
#define NK_TEST(name)                                                          \
  static int __test_##name(FILE *__test_out);                                  \
  static __attribute__((constructor)) void __test_##name##_register() {        \
    nk_test_register(#name, &__test_##name);                                   \
  }                                                                            \
  static int __test_##name(FILE *__test_out)

#define NK_TEST_ASSERT_IMPL(exit_on_fail, cond, error)                         \
  do {                                                                         \
    int __testcond = (cond);                                                   \
    if (!__testcond) {                                                         \
      if (exit_on_fail) {                                                      \
        nk_test_error_report(__test_out, "%s\n", (error));                     \
        return 1;                                                              \
      } else {                                                                 \
        nk_test_error_report(__test_out, "%s\n", (error));                     \
      }                                                                        \
    }                                                                          \
  } while (0)
#define NK_TEST_ASSERT_IMPL_FMT(exit_on_fail, cond, errorfmt, ...)             \
  do {                                                                         \
    int __testcond = (cond);                                                   \
    if (!__testcond) {                                                         \
      nk_test_error_report(__test_out, errorfmt "\n", __VA_ARGS__);            \
      if (exit_on_fail) {                                                      \
        return 1;                                                              \
      }                                                                        \
    }                                                                          \
  } while (0)

// Test a condition, printing the given condition and failing the suite if not
// true.
#define NK_TEST_ASSERT(cond)                                                   \
  NK_TEST_ASSERT_IMPL(1, cond, "ASSERT FAILED: " #cond)
// Test a condition, printing the given error message and failing the test suite
// if not true.
#define NK_TEST_ASSERT_MSG(cond, error) NK_TEST_ASSERT_IMPL(1, cond, error)
// Test a condition, printing the given error message formatted with the given
// args and failing the test suite if not true.
#define NK_TEST_ASSERT_FMT(cond, errorfmt, ...)                                \
  NK_TEST_ASSERT_IMPL_FMT(1, cond, errorfmt, __VA_ARGS__)
// Test a condition, printing the given condition if not true, but continuing
// the suite.
#define NK_TEST_EXPECT(cond)                                                   \
  NK_TEST_ASSERT_IMPL(0, cond, "EXPECT FAILED: " #cond)
// Test a condition, printing the given error message if not true, but
// continuing the suite.
#define NK_TEST_EXPECT_MSG(cond, error) NK_TEST_ASSERT_IMPL(0, cond, error)
// Test a condition, printing the given error message formatted with the given
// args if not true, but continuing the suite.
#define NK_TEST_EXPECT_FMT(cond, errorfmt, ...)                                \
  NK_TEST_ASSERT_IMPL_FMT(0, cond, errorfmt, __VA_ARGS__)
// End the test suite.
#define NK_TEST_OK() return 0

#endif // __NK_TEST_H__
