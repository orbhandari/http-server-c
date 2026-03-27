#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include "http_definitions.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

enum HttpParseState {
  START,
  PARSING_GET,
  PARSING_REQUEST_URI,
  PARSING_FINISHED
};

/*
 * @brief HttpParser is modelled as a finite state machine.
 */
struct HttpParser {
  enum HttpParseState state; // Uninitialized "initial" state.
};

/*
 * @brief Simple-Request  = "GET" SP Request-URI CRLF
 */
struct HttpSimpleRequest {
  char request_uri[1024 +
                   1]; // request_uri is a null-terminated string, hence the +1.
                       // TODO: Change to pointer and heap-allocated C-string.
                       // This is memory-wasteful.
};

/*
 * @brief Initialises a HttpParser module.
 */
void init_http_parser(struct HttpParser *http_parser) {
  http_parser->state = START;
}

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
bool parse_simple_request(struct HttpParser *http_parser,
                          struct HttpSimpleRequest *http_simple_request,
                          char request[]) {
  char *token;

  char delimiters[] = {SP};
  token = strtok(request, delimiters);

  http_parser->state = PARSING_GET;

  // Technically, we can increment the enum by +1 so that we avoid making
  // mistakes of setting the wrong next state.
  while (token != NULL) {
    // Currently fails the parsing if there are extra tokens.
    if (http_parser->state >= PARSING_FINISHED) {
      return false;
    }

    unsigned long token_len = strlen(token);

    switch (http_parser->state) {
    case PARSING_GET:
      if (strcmp(token, "GET") != 0) {
        printf("[Error] GET token not found while parsing request into "
               "Simple-Request.\n");
        return false;
      }
      http_parser->state = PARSING_REQUEST_URI;
      break;
    case PARSING_REQUEST_URI:
      // TODO: Should validate request uri further here, such as needing it to start with /
      if (token_len > G_MAX_URI_LEN) {
        printf(
            "[Error] URI exceeded G_MAX_URI_LEN: %d. Current URI length: %lu",
            G_MAX_URI_LEN, token_len);
        return false;
      } else if (token_len <= 0) {
        // TODO: Also, if it is empty, technically we should default to root / as well
        printf("[Error] URI does not have positive length. Current URI length: "
               "%lu",
               token_len);
        return false;
      }
      strcpy(http_simple_request->request_uri, token);
      http_parser->state = PARSING_FINISHED;
      break;
    default:
      printf(
          "[Error] Unknown state while parsing request into Simple-Request.\n");
      return false;
    }

    token = strtok(NULL, delimiters);
  }

  return http_parser->state == PARSING_FINISHED;
}

#endif
