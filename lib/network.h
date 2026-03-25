#ifndef NETWORK_IO_MODULE_H
#define NETWORK_IO_MODULE_H

#include "http_parser.h"
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

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

struct Connection {
  char buffer[G_MAX_BUFFER_SIZE];
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
 * @brief Initialises a NetworkIO module.
 */
void init_network_io(struct NetworkIO *network_io_module, int port) {
  network_io_module->num_sockets = G_NUM_LISTENING_SOCKETS;

  assert(port >= 49152 && port <= 65535 &&
         "please use a port in the range [49152-65535].");
  network_io_module->port = port;
}

int set_non_blocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl(F_GETFL)");
    return -1;
  }

  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    perror("fcntl(F_SETFL)");
    return -1;
  }

  return 0;
}

/*
 * @brief Crashes if any step of the server socket setup fails.
 */
void start_listening(struct NetworkIO *network_io_module) {
  // This is a TCP socket: https://man7.org/linux/man-pages/man7/ip.7.html
  int listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  network_io_module->listen_socket = listen_socket;

  if (listen_socket == -1) {
    err(EXIT_FAILURE, "getting a listen_socket failed");
  }
  printf("Created listen socket.\n");

  if (set_non_blocking(listen_socket) == -1) {
    err(EXIT_FAILURE, "setting listen_socket to non-blocking failed");
  }
  printf("Set listen socket to non blocking.\n");

  int opt = 1;
  if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                 sizeof(opt)) == -1) {
    perror("setsockopt failed for listen_socket");
    exit(EXIT_FAILURE);
  }
  printf("Set socket options to re-use address and re-use port.\n");

  // Using designated initializers will automatically zero out paddings, so we
  // don't need to memset to 0.
  assert(network_io_module->port >= 49152 && network_io_module->port <= 65535 &&
         "please use a port in the range [49152-65535].");
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
