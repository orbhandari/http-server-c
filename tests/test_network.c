#include "../lib/network.h"
#include "testing_utils.h"

#define TEST_PORT 60001

int main() {
  printf("Test server running on port %d", TEST_PORT);

  struct NetworkIO network_io_module = {.port = TEST_PORT};
  run(&network_io_module);

  return 0;
}
