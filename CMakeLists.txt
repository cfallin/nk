set(SRCS kernel.c thd.c)
set(TEST_SRCS test.c)

cmake_minimum_required(VERSION 2.6)
project(nk)

set(CMAKE_C_CFLAGS "${CMAKE_C_FLAAGS} -std=c11 -pedantic")
add_library(nk ${SRCS})

add_executable(nk_test ${SRCS} ${TEST_SRCS})
target_link_libraries(nk_test nk)
enable_testing()
add_test(nk_test nk_test)
