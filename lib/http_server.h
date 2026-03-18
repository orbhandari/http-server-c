#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "network.h"
#include "http_parser.h"
#include "http_builder.h"
#include "data.h"
#include <stddef.h>
#include <stdbool.h>

struct HttpServer {
    struct NetworkIO network_io_module;
    struct HttpParser http_parser_module;
    struct HttpBuilder http_builder_module;
    struct DataIO data_io_module;
};

inline struct HttpServer* get_http_server(int port) {
    // malloc an http server.
    // construct and init modules
    // starts the http server 
    // starting means to start listening for connections on the listen socket,
    // and accepting connections from clients.
    return NULL;
}

inline bool process_message(char message[]) {
    // Call the HTTP parser module
    // If parse failed, reject the message with the proper HTTP response and status code using HTTP builder module and network IO module.
    // If parse succeed, continue on.

    // Take the parsed message and pass it on to the data IO module.
    // Wait for the data.
    // If data get failed, reject the message with the proper HTTP response and status code using HTTP builder module and network IO module.
    // If data get succeed, continue on.
    
    // Take the data and send the HTTP response along with the data using HTTP builder module and network IO module.
    return false;
}

inline void run(struct HttpServer* http_server) {
    while (true) {
        // Poll from the socket set of the network io module
        // Do a recv() from an available file descriptor
        // Process the message
    }
}

#endif
