set(SRCS src/kernel.c src/thd.c src/databuf.c)
set(TEST_SRCS test/test_main.c test/test.c test/test_databuf.c)
include_directories(include/)

cmake_minimum_required(VERSION 2.6)
project(nk)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11")
add_library(nk ${SRCS})

add_executable(nk_test ${SRCS} ${TEST_SRCS})
target_link_libraries(nk_test nk)
enable_testing()
add_test(nk_test nk_test)
