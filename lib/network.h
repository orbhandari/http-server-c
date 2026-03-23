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
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>

#define G_MAX_CLIENT_SOCKETS 1
#define G_NUM_LISTENING_SOCKETS 1 // Always 1 for TCP sockets
#define G_MAX_SOCKETS (G_MAX_CLIENT_SOCKETS + G_NUM_LISTENING_SOCKETS)

#define G_MAX_METHOD_LEN 6 // GET, POST, PUT, DELETE
#define G_SP_LEN 1

#define G_BUFFER_SCALING_FACTOR 2
#define G_MAX_BUFFER_SIZE                                                      \
  ((G_MAX_METHOD_LEN + G_SP_LEN + G_MAX_URI_LEN) * G_BUFFER_SCALING_FACTOR)

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

void run(struct NetworkIO *network_io_module) {
  // This is a TCP socket: https://man7.org/linux/man-pages/man7/ip.7.html
  int listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  if (listen_socket == -1) {
    err(EXIT_FAILURE, "getting a listen_socket failed");
  }

  if (set_non_blocking(listen_socket) == -1) {
    err(EXIT_FAILURE, "setting listen_socket to non-blocking failed");
  }

  int opt = 1;
  if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                 sizeof(opt)) == -1) {
    perror("setsockopt failed for listen_socket");
    exit(EXIT_FAILURE);
  }

  // Using designated initializers will automatically zero out paddings, so we
  // don't need to memset to 0.
  assert(network_io_module->port >= 49152 && network_io_module->port <= 65535 &&
         "please use a port in the range [49152-65535].");
  // TODO: Expand to public application and replace loopback address
  // restriction.
  struct sockaddr_in server_addr = {.sin_family = AF_INET,
                                    .sin_port = htons(network_io_module->port),
                                    .sin_addr = htonl(INADDR_LOOPBACK)};

  printf("note: currently hardcoded to using INADDR_LOOPBACK.\n");

  if (bind(listen_socket, (struct sockaddr *)&server_addr,
           sizeof(server_addr)) == -1) {
    err(EXIT_FAILURE, "binding the listen_socket failed");
  }

  if (listen(listen_socket, G_MAX_CLIENT_SOCKETS) == -1) {
    err(EXIT_FAILURE, "listen for listen_socket failed");
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
      err(EXIT_FAILURE, "epoll_wait failed");
    }

    for (int i = 0; i < num_ready_fds; ++i) {
      if (epoll_events[i].data.fd == listen_socket) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len;
        int client_connection_socket = accept(
            listen_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_connection_socket == -1) {
          err(EXIT_FAILURE, "failed to accept a client_connection_socket");
        }

        if (set_non_blocking(client_connection_socket) == -1) {
          err(EXIT_FAILURE,
              "failed to set a client_connection_socket as non-blocking");
        }

        // A client connection socket will be epolled via an edge-triggered
        // interface.
        epoll_event.events = EPOLLIN | EPOLLET;
        epoll_event.data.fd = client_connection_socket;

        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_connection_socket,
                      &epoll_event) == -1) {
          err(EXIT_FAILURE, "epoll_ctl failed for client_connection_socket");
        }

      } else {
        // TODO: remove this mock code:
        const int MAX_BUFFER_SIZE = 1024;
        char client_message[MAX_BUFFER_SIZE];

        char buffer[MAX_BUFFER_SIZE];
        // Non-blocking recv.
        while (true) {
          ssize_t num_bytes =
              recv(epoll_events[i].data.fd, buffer, MAX_BUFFER_SIZE, 0);

          printf("num_bytes from client: %ld\n", num_bytes);

          if (num_bytes > 0) {
            strcat(client_message, buffer);
          }

          if (errno == EAGAIN | errno == EWOULDBLOCK) {
            printf("drained recv buffer.\n");
            break;
          }
        }

        strcat(client_message, "\0"); // additional null termination for safety
        printf("[server] client message received: %s\n", client_message);

        // Non-blocking send.
        const char some_message[] = "Hello and received!";
        send(epoll_events[i].data.fd, some_message, sizeof(some_message), 0);

        // handle existing client connection
        // Do a recv() from an available file descriptor
        // Process the message
      }
    }
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
