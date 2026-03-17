#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <assert.h>
#include <cstdio>
#include <cstring>
#include <stdbool.h>

static const char SP = ' ';

static const int G_REQUEST_LINE_NUM_TOKENS = 2; // Defined by HTTP/0.9 RFC.
static const int G_MAX_URI_LEN =
    1024; // Max URI length is not specified in RFC, so it seems we have freedom
          // to choose it.
          // TODO: Choose a common length, and consider any possible tradeoffs.

enum HttpParseState { PARSING_GET, PARSING_REQUEST_URI, PARSING_FINISHED };

// HttpParser is modelled as a finite state machine.
struct HttpParser {
  HttpParseState state; // Unitialized "initial" state.
};

// Simple-Request  = "GET" SP Request-URI CRLF
struct HttpSimpleRequest {
  char request_uri[G_MAX_URI_LEN +
                   1]; // request_uri is a null-terminated string, hence the +1.
                       // TODO: Change to pointer and heap-allocated C-string.
                       // This is memory-wasteful.
};

/*
 * @param http_parser Pointer to a HttpParser instance. The function will modify
 * its state. The final state implies the state at which parsing failed.
 * @param http_simple_request Caller-created HttpSimpleRequest. The function
 * will fill it with values if successful.
 * @param request Request to be parsed. MUST be a null-terminated string to
 * avoid UB. For parsing to succeed, it must further be clear of CLRF, and
 * satisfy URI length must not exceed G_MAX_URI_LEN.
 *
 * @return Boolean value indicating success (1) or failure (0). Upon failure,
 * http_simple_request may still contain partial values, and should not be used
 * as not all values are initialised. Doing so leads to undefined behaviour.
 */
inline bool parse_simple_request(HttpParser *http_parser,
                                 struct HttpSimpleRequest *http_simple_request,
                                 char request[]) {
  char *token;

  char delimiters[] = {SP};
  token = strtok(request, delimiters);

  http_parser->state = HttpParseState::PARSING_GET;

  // Technically, we can increment the enum by +1 so that we avoid making
  // mistakes of setting the wrong next state.
  while (token != NULL) {
    if (http_parser->state >= PARSING_FINISHED) {
      return false;
    }

    switch (http_parser->state) {
    case PARSING_GET:
      if (strcmp(token, "GET")) {
        return false;
      }
      http_parser->state = PARSING_REQUEST_URI;
      break;
    case PARSING_REQUEST_URI:
      if (strlen(token) > G_MAX_URI_LEN) {
        return false;
      }
      strcpy(http_simple_request->request_uri, token);
      http_parser->state = PARSING_FINISHED;
      break;
    default:
      printf("Unknown http_parser->state while parsing request into "
             "Simple-Request.\n");
      return false;
    }

    token = strtok(NULL, " ");
  }

  return http_parser->state == PARSING_FINISHED;
}

#endif
