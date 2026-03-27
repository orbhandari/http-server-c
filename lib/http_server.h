#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "data.h"
#include "http_builder.h"
#include "http_parser.h"
#include "network.h"
#include "network_definitions.h"
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

/*
 * @brief The HttpServer will follow the pipeline:
 * network IO (receiving)
 * -> HTTP request parsing
 * -> data IO
 * -> HTTP response building
 * -> network IO (sending)
 */
struct HttpServer {
  struct NetworkIO network_io_module;
  struct HttpParser http_parser_module;
  struct DataIO data_io_module;
  struct HttpBuilder http_builder_module;
};

/*
 * @brief It is recommended to use this instead of creating your own HttpServer
 * object to ensure modules are properly initialised. Since the HttpServer is
 * hardcoded to use localhost, only port is required to be passed in.
 *
 * @param port A free port in the range: [49152-65535]
 *
 * @returns Dynamically allocated HttpServer. Remember to call
 * free_http_server for cleanup.
 */
struct HttpServer *get_http_server(int port) {
  struct HttpServer *http_server;
  http_server = malloc(sizeof(*http_server));

  // Init does NOT mean it starts listening. The HTTP server is STILL in a idle
  // state.
  init_network_io(&http_server->network_io_module, port);

  init_http_parser(&http_server->http_parser_module);

  // TODO more inits here...

  return http_server;
}

/*
 * @brief Cleanup function for an HttpServer object.
 *
 * @param Pointer to the caller-created HttpServer object.
 */
void free_http_server(struct HttpServer *http_server) {
  // TODO: free any other heap allocated objects here...

  free(http_server);
  http_server = NULL;
}

void process_connection_buffer(struct HttpServer *http_server,
                               struct Connection *connection) {
  printf("Processing connection buffer: %.*s\n", connection->buffer_size,
         connection->buffer);
  printf("Size: %d\n", connection->buffer_size);

  printf("Starting check from check_offset: %d\n", connection->check_offset);

  int i = connection->check_offset;
  for (; i < connection->buffer_size - 1; ++i) {
    // We define the message boundary as "CLRF" for HTTP/0.9.
    printf("Checking: %c%c\n", connection->buffer[i],
           connection->buffer[i + 1]);
    if (connection->buffer[i] == '\r' && connection->buffer[i + 1] == '\n') {
      printf("CLRF found!\n");

      int request_len = i;
      printf("Request length excluding 2 bytes from CLRF: %d\n", request_len);

      if (request_len >= 0) {
        // This must be null-terminated.
        char request_buffer[request_len + 1];

        memcpy(request_buffer, connection->buffer, request_len);

        request_buffer[request_len] = '\0';
        printf("Request buffer extracted: %s\n", request_buffer);

        // Check if we can correctly shift from the part after '\r\n'
        // next_message_start_idx is same as the request_buffer plus two bytes
        // for CLRF.
        const int next_message_start_idx = request_len + 2;

        if (next_message_start_idx <= G_MAX_BUFFER_SIZE - 1) {
          // This is a faster way to "circular shift" our array/pop the first
          // message out.
          memmove(&connection->buffer[0],
                  &connection->buffer[next_message_start_idx],
                  connection->buffer_size - next_message_start_idx + 1);

          // Make sure the invalid data is truly "unmeaningful" by resetting
          // them to 0.
          memset(&connection->buffer[next_message_start_idx], 0,
                 connection->buffer_size - next_message_start_idx + 1);

          connection->buffer_size -= next_message_start_idx;
        } else {
          assert(false && "unreachable case: a single HTTP message occupied "
                          "the entire connection buffer");
          // memset(&connection->buffer, 0, G_MAX_BUFFER_SIZE);
        }

        printf("Connection buffer after shifting: %.*s\n",
               connection->buffer_size, connection->buffer);
        printf("Size after shifting: %d\n", connection->buffer_size);

        // Whenever we "popped" the buffer, we will restart from 0.
        connection->check_offset = 0;

        struct HttpSimpleRequest http_simple_request = {0};

        // TODO: return this to main loop
        if (parse_simple_request(&http_server->http_parser_module,
                                 &http_simple_request, request_buffer)) {
          printf("http_simple_request parsed: %s\n",
                 http_simple_request.request_uri);
        } else {
          printf("http_simple_request parsing rejected.\n");
        }
      }

      return;
    }
  }

  connection->check_offset = i;
  printf("No complete potential HTTP message found.\n");
}

/*
 * @brief After getting an HttpServer via get_http_server, call this to start
 * listening and kickstart the processing pipeline. Note that this is a blocking
 * function call.
 *
 * @param http_server Pointer to the caller-created HttpServer object.
 */
// TODO: Refactor this shit
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

        struct Connection client_connection = {.buffer = {},
                                               .buffer_size = 0,
                                               .check_offset = 0,
                                               .write_offset = 0};

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
            client_connection->buffer_size += num_bytes;

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

            struct Connection empty_connection = {.buffer = {},
                                                  .buffer_size = 0,
                                                  .write_offset = 0,
                                                  .check_offset = 0};

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

        process_connection_buffer(http_server, client_connection);
      }
    }
  }
}

#endif
