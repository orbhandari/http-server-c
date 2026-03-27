#ifndef HTTP_HELPERS_H
#define HTTP_HELPERS_H 

#include "http_parser.h"
void print_http_simple_request(struct HttpSimpleRequest* http_simple_request) {
   printf("Request URI: %s\n", http_simple_request->request_uri);
}

#endif // !HTTP_HELPERS_H
