#ifndef NETWORK_IO_MODULE_H
#define NETWORK_IO_MODULE_H

#include "http_parser.h"
#include <err.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>

// Maybe put into parser module?
const int G_MAX_METHOD_LEN = 6; // GET, POST, PUT, DELETE
const int G_SP_LEN = 1;

const int G_MAX_CLIENT_SOCKETS = 1;
const int G_NUM_LISTENING_SOCKETS = 1; // Always 1 in TCP sockets.
const int G_MAX_SOCKETS = G_MAX_CLIENT_SOCKETS + G_NUM_LISTENING_SOCKETS;

const int G_BUFFER_SCALING_FACTOR = 2;
const int G_MAX_BUFFER_SIZE =
    (G_MAX_METHOD_LEN + G_SP_LEN + G_MAX_URI_LEN) * G_BUFFER_SCALING_FACTOR;

struct Connection {
  char buffer[G_MAX_BUFFER_SIZE];
  char offset; // This offset (suggested by Gemini) is used to avoid wasted
               // linear scans to find the first available CLRF.
};

struct NetworkIO {
  int port;
  int num_sockets; // Equivalent to saying num_fds, i.e. number of clients plus
                   // one listening socket.
                   // num_sockets should be at most G_MAX_SOCKETS.
                   // This invariant should be programmed in setters/getters.
  struct Connection connections[G_MAX_SOCKETS];
};

void run(struct NetworkIO *network_io_module) {
  // This is a TCP socket: https://man7.org/linux/man-pages/man7/ip.7.html
  int listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listen_socket == -1) {
    err(EXIT_FAILURE, "getting a listen_socket failed");
  }

  if (listen(listen_socket, G_MAX_CLIENT_SOCKETS) == -1) {
    err(EXIT_FAILURE, "listen for listen_socket failed");
  }

  // Using designated initializers will automatically zero out paddings, so we
  // don't need to memset to 0.
  assert(network_io_module->port >= 49152 && network_io_module->port <= 65535 &&
         "Please use a port in the range [49152-65535].");
  // TODO: Expand to public application and replace loopback address
  // restriction.
  struct sockaddr_in server_addr = {.sin_family = AF_INET,
                                    .sin_port = htons(network_io_module->port),
                                    .sin_addr = htons(INADDR_LOOPBACK)};

  printf("Note: currently hardcoded to using INADDR_LOOPBACK.\n");

  if (bind(listen_socket, (struct sockaddr *)&server_addr,
           sizeof(server_addr)) == -1) {
    err(EXIT_FAILURE, "binding the listen_socket failed.");
  }

  int epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    err(EXIT_FAILURE, "epoll_create1 failed");
  }

  // Reusable structure to help register the listen_socket and future
  // client_sockets.
  struct epoll_event epoll_event;
  struct epoll_event epoll_events[G_MAX_SOCKETS];

  // Register the listen_socket.
  epoll_event.events = EPOLLIN;
  epoll_event.data.fd = listen_socket;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_socket, &epoll_event) == -1) {
    err(EXIT_FAILURE, "epoll_ctl failed for listen_socket");
  }

  int num_ready_fds = 0;
  while (true) {
    num_ready_fds = epoll_wait(epoll_fd, epoll_events, G_MAX_SOCKETS, -1);

    if (num_ready_fds == -1) {
      err(EXIT_FAILURE,
          "epoll_wait failed when listen_socket is the only registered socket");
    }

    for (int i = 0; i < num_ready_fds; ++i) {
      if (epoll_events[i].data.fd == listen_socket) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len;
        int client_connection_socket = accept(
            listen_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_connection_socket == -1) {
          err(EXIT_FAILURE, "Failed to accept a client_connection_socket");
        }

        // set non blocking
        // define event: EPOLL IN and EPOLL ET?
        // register with epoll ctl

      } else {
        // handle existing client connection
      }
    }

    // Poll from the socket set of the network io module
    // Do a recv() from an available file descriptor
    // Process the message
  }
}

// TODO: Add connection, remove connection.
// Interesting fact about remove connection: normally, after a HTTP response,
// the TCP connection is closed by the server. Hence, there may be a "gap" in
// the connections array. However, the Linux Kernel assigns the *lowest
// available fd* to the client, hence it will re-take the gap spot in the array.
// No shifting is required. Of course, if it wasn't the case, I could have used
// doubly linked list + map to node. It is not required here.

#endif
