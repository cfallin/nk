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

#define NK_TEST_MACRO_STRING2(s) #s
#define NK_TEST_MACRO_STRING(s) NK_TEST_MACRO_STRING2(s)

// Test a condition, printing the given condition and failing the suite if not
// true.
#define NK_TEST_ASSERT(cond)                                                   \
  NK_TEST_ASSERT_IMPL(1, cond, "ASSERT FAILED: " __FILE__                      \
                               ":" NK_TEST_MACRO_STRING(__LINE__) ": " #cond)
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
  NK_TEST_ASSERT_IMPL(0, cond, "EXPECT FAILED: " __FILE__                      \
                               ":" NK_TEST_MACRO_STRING(__LINE__) ": " #cond)
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
