#include "../lib/http_server.h"
#include "testing_utils.h"

#define TEST_PORT 60001

int main() {
  printf("Test server running on port %d\n", TEST_PORT);

  struct HttpServer *http_server = get_http_server(TEST_PORT);
  run_http_server(http_server);

  return 0;
}
