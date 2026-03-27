#ifndef NETWORK_IO_MODULE_H
#define NETWORK_IO_MODULE_H

#include "network_definitions.h"
#include "network_helpers.h"
#include <assert.h>
#include <err.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

/*
 * @brief A connection metadata object to track a client.
 */
struct Connection {
  char buffer[G_MAX_BUFFER_SIZE];
  int buffer_size;
  int check_offset; // This offset (suggested by Gemini) is used to avoid wasted
                    // linear scans to find the first available CLRF.
  int write_offset;
};

/*
 * @brief It is highly recommended to 0-initialise this to make sure the member
 * initialised contains the correct state.
 */
struct NetworkIO {
  int listen_socket;
  int port;
  int num_sockets; // Equivalent to saying num_fds, i.e. number of clients plus
                   // one listening socket.
                   // num_sockets should be at most G_MAX_SOCKETS.
  struct Connection connections[G_MAX_CLIENT_SOCKETS];
};

/*
 * @brief Initialises a NetworkIO module, and crashes if any step of the server
 * socket setup fails.
 *
 * @param network_io_module Pointer to caller-created NetworkIO object.
 * @param port A free port in the range: [49152-65535].
 */
void init_network_io(struct NetworkIO *network_io_module, int port) {
  // 0-initialise for safety.
  memset(network_io_module, 0, sizeof(struct NetworkIO));

  // This is a TCP server socket:
  // https://man7.org/linux/man-pages/man7/ip.7.html
  network_io_module->listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  if (network_io_module->listen_socket == -1) {
    err(EXIT_FAILURE, "getting a listen_socket failed");
  }
  printf("Created listen socket.\n");

  if (set_non_blocking(network_io_module->listen_socket) == -1) {
    err(EXIT_FAILURE, "setting listen_socket to non-blocking failed");
  }
  printf("Set listen socket to non blocking.\n");

  int opt = 1;
  if (setsockopt(network_io_module->listen_socket, SOL_SOCKET,
                 SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) == -1) {
    perror("setsockopt failed for listen_socket");
    exit(EXIT_FAILURE);
  }
  printf("Set socket options to re-use address and re-use port.\n");

  network_io_module->num_sockets = G_NUM_LISTENING_SOCKETS;

  assert(port >= 49152 && port <= 65535 &&
         "please use a port in the range [49152-65535].");
  network_io_module->port = port;
}

/*
 * @brief Please ensure the NetworkIO object is properly initialised via
 * init_network_io.
 *
 * @param network_io_module Pointer to caller-created NetworkIO object.
 */
void start_listening(struct NetworkIO *network_io_module) {
  int listen_socket = network_io_module->listen_socket;

  // Using designated initializers will automatically zero out paddings, so we
  // don't need to memset to 0.
  // TODO: Expand to public application and replace loopback address
  // restriction.
  struct sockaddr_in server_addr = {.sin_family = AF_INET,
                                    .sin_port = htons(network_io_module->port),
                                    .sin_addr = htonl(INADDR_LOOPBACK)};

  printf("Note: currently hardcoded to using loopback address.\n");

  if (bind(listen_socket, (struct sockaddr *)&server_addr,
           sizeof(server_addr)) == -1) {
    err(EXIT_FAILURE, "binding the listen_socket failed");
  }
  printf("Binding listen socket to loopback address.\n");

  if (listen(listen_socket, G_MAX_CLIENT_SOCKETS) == -1) {
    err(EXIT_FAILURE, "listen for listen_socket failed");
  }
  printf("Listen mode set for listen socket.\n");
}

#endif
