#ifndef NETWORK_DEFINITIONS_H
#define NETWORK_DEFINITIONS_H

#include "http_definitions.h" // For G_MAX_URI_LEN

#define G_MAX_CLIENT_SOCKETS                                                   \
  1000 // This should always be greater than 5, since (usually) the first
       // available fd is 5 for clients. Also, note that going larger may
       // overload the available stack space in your system, as we currently use
       // stack memory.
#define G_NUM_LISTENING_SOCKETS 1 // Always 1 for TCP sockets
#define G_MAX_SOCKETS (G_MAX_CLIENT_SOCKETS + G_NUM_LISTENING_SOCKETS)

#define G_MAX_METHOD_LEN 6 // GET, POST, PUT, DELETE
#define G_SP_LEN 1

#define G_BUFFER_SCALING_FACTOR 2
#define G_MAX_BUFFER_SIZE                                                      \
  ((G_MAX_METHOD_LEN + G_SP_LEN + G_MAX_URI_LEN) * G_BUFFER_SCALING_FACTOR)

#endif
