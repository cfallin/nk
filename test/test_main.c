#include "kernel.h"
#include "test.h"

int main(int argc, char **argv) {
  nk_init();
  return nk_run_tests();
}
