set(SRCS kernel.c thd.c databuf.c)
set(TEST_SRCS test_main.c test.c test_databuf.c)

cmake_minimum_required(VERSION 2.6)
project(nk)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11")
add_library(nk ${SRCS})

add_executable(nk_test ${SRCS} ${TEST_SRCS})
target_link_libraries(nk_test nk)
enable_testing()
add_test(nk_test nk_test)
