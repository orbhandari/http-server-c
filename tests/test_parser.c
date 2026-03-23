#include "../lib/http_parser.h"
#include "testing_utils.h"
#include <stddef.h>
#include <string.h>


void test_parse_simple_request_with_valid_get_and_uri() {
  printf("Running test_parse_simple_request_with_valid.\n");
  struct HttpParser http_parser;
  char request[] = "GET /a/b/c";
  struct HttpSimpleRequest simple_request;

  bool success = parse_simple_request(&http_parser, &simple_request, request);

  assert(success);
  assert(http_parser.state == PARSING_FINISHED);
  assert(strcmp(simple_request.request_uri, "/a/b/c") == 0);

  printf("http_parser.state: %d\n", http_parser.state);
  printf("simple_request.request_uri: %s\n", simple_request.request_uri);

  print_test_passed();
}

void test_parse_simple_request_with_invalid_get() {
  printf("Running test_parse_simple_request_with_invalid_get.\n");
  struct HttpParser http_parser;
  char request[] = "NOGET /a/b/c";
  struct HttpSimpleRequest simple_request;
    
  printf("This should show an error:\n");
  bool success = parse_simple_request(&http_parser, &simple_request, request);

  assert(!success);

  print_test_passed();
}

void test_parse_simple_request_with_invalid_uri() {
  printf("Running test_parse_simple_request_with_invalid_uri.\n");
  struct HttpParser http_parser;

  char long_uri[G_MAX_URI_LEN * 2 + 1]; // PLus one for '\0'
  for (size_t i = 0; i < G_MAX_URI_LEN * 2; ++i) {
    long_uri[i] = 'a';
  }

  long_uri[G_MAX_URI_LEN * 2] = '\0';

  char request[4 + G_MAX_URI_LEN * 2 + 1] = "GET ";
  strcat(request, long_uri);

  printf("Expected size of entire request: %lu\n", strlen(request));
  printf("Expected size of URI: %lu\n", strlen(request) - 4);

  struct HttpSimpleRequest simple_request;

  printf("This should show an error:\n");
  bool success = parse_simple_request(&http_parser, &simple_request, request);

  assert(!success);

  print_test_passed();
}

int main(int argc, char *argv[]) {
  printf("\n----- TESTS -----\n\n");

  test_parse_simple_request_with_valid_get_and_uri();
  printf("\n");

  test_parse_simple_request_with_invalid_get();
  printf("\n");

  test_parse_simple_request_with_invalid_uri();
  printf("\n");

  return 0;
}
