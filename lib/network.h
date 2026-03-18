#ifndef NETWORK_IO_MODULE_H
#define NETWORK_IO_MODULE_H

#include "http_parser.h"
#include <string.h>

// Maybe put into parser module?
static const int G_MAX_METHOD_LEN = 6; // GET, POST, PUT, DELETE
static const int G_SP_LEN = 1;

static const int G_MAX_CLIENT_SOCKETS = 1;
static const int G_NUM_LISTENING_SOCKETS = 1; // Always 1 in TCP sockets.
static const int G_MAX_SOCKETS = G_MAX_CLIENT_SOCKETS + G_NUM_LISTENING_SOCKETS;

static const int G_BUFFER_SCALING_FACTOR = 2;
static const int G_MAX_BUFFER_SIZE =
    (G_MAX_METHOD_LEN + G_SP_LEN + G_MAX_URI_LEN) * G_BUFFER_SCALING_FACTOR;

struct Connection {
  char buffer[G_MAX_BUFFER_SIZE];
  char offset; // This offset (suggested by Gemini) is used to avoid wasted
                   // linear scans to find the first available CLRF.
};

struct NetworkIO {
  int num_sockets; // Equivalent to saying num_fds, i.e. number of clients plus
                   // one listening socket.
                   // num_sockets should be at most G_MAX_SOCKETS.
                   // This invariant should be programmed in setters/getters.
  struct Connection connections[G_MAX_SOCKETS];
};


// TODO: Add connection, remove connection.
// Interesting fact about remove connection: normally, after a HTTP response,
// the TCP connection is closed by the server. Hence, there may be a "gap" in
// the connections array. However, the Linux Kernel assigns the *lowest
// available fd* to the client, hence it will re-take the gap spot in the array.
// No shifting is required. Of course, if it wasn't the case, I could have used
// doubly linked list + map to node. It is not required here.

#endif
