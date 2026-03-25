#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "data.h"
#include "http_builder.h"
#include "http_parser.h"
#include "network.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

struct HttpServer {
  struct NetworkIO network_io_module;
  struct HttpParser http_parser_module;
  struct DataIO data_io_module;
  struct HttpBuilder http_builder_module;
};

/*
 * @brief Since the HttpServer is hardcoded to use localhost,
 * only port is required to be passed in.
 *
 * @param port A free port in the range: [ x - x ]
 */
struct HttpServer *get_http_server(int port) {
  struct HttpServer *http_server;
  http_server = malloc(sizeof(*http_server));

  init_network_io(&http_server->network_io_module, port);
  // TODO more inits here...

  return http_server;
}

void process_connection_buffer(struct Connection *connection) {
  for (int i = connection->check_offset; i < G_MAX_BUFFER_SIZE - 1; ++i) {
    if (connection->buffer[i] == '\r' && connection->buffer[i + 1] == '\n') {
      // complete!
      // We want from 0 to i-1 (remove CLRF)

      // make sure that i-1 >= 0.
      // Then null terminate.
      // Then memmove the rest of the array to overwrite 0 to i + 1.
    }
  }
}

void free_http_server(struct HttpServer *http_server) {
  free(http_server);
  http_server = NULL;
}

void run_http_server(struct HttpServer *http_server) {
  start_listening(&http_server->network_io_module);

  int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
  if (epoll_fd == -1) {
    err(EXIT_FAILURE, "epoll_create1 failed");
  }
  printf("epoll instance created.\n");

  // Reusable structure to help register the listen_socket and future
  // client_sockets.
  struct epoll_event epoll_event;
  struct epoll_event epoll_events[G_MAX_SOCKETS];

  // Register the listen_socket.
  int listen_socket = http_server->network_io_module.listen_socket;
  epoll_event.events = EPOLLIN;
  epoll_event.data.fd = listen_socket;

  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_socket, &epoll_event) == -1) {
    err(EXIT_FAILURE, "epoll_ctl failed for listen_socket");
  }
  printf("Registed listen socket to epoll instance.\n");

  while (true) {
    int num_ready_fds = epoll_wait(epoll_fd, epoll_events, G_MAX_SOCKETS, -1);
    printf("Event received. Processing batch of ready fds...\n");

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

        if (http_server->network_io_module.num_sockets == G_MAX_SOCKETS) {
          perror("rejecting new client as G_MAX_SOCKETS reached");
          close(client_connection_socket);
          continue;
        }

        http_server->network_io_module.num_sockets += 1;
        printf(
            "Accepted new client connection: fd %d. Total client count: %d\n",
            client_connection_socket,
            http_server->network_io_module.num_sockets - 1);

        if (set_non_blocking(client_connection_socket) == -1) {
          err(EXIT_FAILURE,
              "failed to set a client_connection_socket as non-blocking");
        }
        printf("Set new client connection as non-blocking.\n");

        // A client connection socket will be epolled via an edge-triggered
        // interface.
        epoll_event.events = EPOLLIN | EPOLLET;
        epoll_event.data.fd = client_connection_socket;

        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_connection_socket,
                      &epoll_event) == -1) {
          err(EXIT_FAILURE, "epoll_ctl failed for client_connection_socket");
        }
        printf("Registered new client to epoll instance.\n");

        struct Connection client_connection = {
            .buffer = {}, .check_offset = 0, .write_offset = 0};

        http_server->network_io_module.connections[client_connection_socket] =
            client_connection;
        printf("Added new client Connection metadata to NetworkIO module.\n");

      } else {
        int client_connection_socket = epoll_events[i].data.fd;
        struct Connection *client_connection =
            &http_server->network_io_module
                 .connections[client_connection_socket];
        char
            recv_buffer[G_MAX_BUFFER_SIZE]; // Does not assume null-termination.

        while (true) {
          int num_bytes =
              recv(client_connection_socket, recv_buffer, G_MAX_BUFFER_SIZE, 0);

          // TODO: Clean up the ugly offset logic!
          if (num_bytes > 0) {
            printf("Number of bytes received from fd %d: %d\n",
                   client_connection_socket, num_bytes);
            if (num_bytes > G_MAX_BUFFER_SIZE ||
                client_connection->write_offset + num_bytes >=
                    G_MAX_BUFFER_SIZE) {
              // TODO: Refactor as reject_client and properly respond with HTTP
              // failure.
              const char rejection_message[] =
                  "Either your message is too long, or the server can't keep "
                  "up.\n";
              send(client_connection_socket, rejection_message,
                   strlen(rejection_message), 0);
              printf("Rejected client due to buffer overflow.\n");
              break;
            }

            // Append to per-client application buffer
            // TODO: Refactor as a "append" function. This is way too ugly.
            for (int i = 0; i < num_bytes; ++i) {
              client_connection->buffer[client_connection->write_offset + i] =
                  recv_buffer[i];
            }
            printf("Appended recv_buffer to connection buffer.\n");

            // TODO: Refactor as a "append" function. This is way too ugly.
            client_connection->write_offset += num_bytes;

            printf("Updated write offset to %d.\n",
                   client_connection->write_offset);

          } else if (num_bytes == -1) {
            perror(
                "some error may have occurred during recv from client socket");
            break;

          } else if (num_bytes == 0) {
            http_server->network_io_module.num_sockets -= 1;

            assert(http_server->network_io_module.num_sockets >= 1);

            struct Connection empty_connection = {
                .buffer = {}, .write_offset = 0, .check_offset = 0};

            *client_connection = empty_connection;

            close(client_connection_socket); // Ensures we always start from the
                                             // minimum available fd.

            printf("Orderly shutdown of connection by a client during socket "
                   "draining.\n");
            break;
          }

          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            printf("Socket drained for client.\n");
            break;
          }
        }
      }
    }
  }
}

#endif
