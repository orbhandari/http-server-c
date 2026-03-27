#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "data.h"
#include "data_definitions.h"
#include "http_builder.h"
#include "http_helpers.h"
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
  // TODO: Perhaps allow the caller to set the memory, and we only initialize it (similar the init_module calls we have here)
  // Apparently, this allows the caller to put it in BSS segment as a global static variable, avoiding our explicit malloc
  http_server = malloc(sizeof(*http_server));

  // Init does NOT mean it starts listening. The HTTP server is STILL in a idle
  // state.
  init_network_io(&http_server->network_io_module, port);

  init_http_parser(&http_server->http_parser_module);

  init_data_io(&http_server->data_io_module);

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

bool process_connection_buffer(struct HttpServer *http_server,
                               struct Connection *connection,
                               struct HttpSimpleRequest *http_simple_request) {
  printf("Processing connection buffer: %.*s\n", connection->buffer_size,
         connection->buffer);
  printf("Size: %d\n", connection->buffer_size);
  printf("Starting check from check_offset: %d\n", connection->check_offset);

  memset(http_simple_request, 0, sizeof(struct HttpSimpleRequest));

  assert(connection->check_offset >= 0 &&
         "check_offset should always be greater than 0");

  int i = connection->check_offset;
  for (; i < connection->buffer_size - 1; ++i) {
    // We define the message boundary as "CLRF" for HTTP/0.9.
    if (connection->buffer[i] == '\r' && connection->buffer[i + 1] == '\n') {
      printf("Message boundary CLRF found!\n");

      int request_len = i;
      printf("Request length excluding 2 bytes from CLRF: %d\n", request_len);

      // This must be null-terminated.
      // TODO: request_len is variable, hence we should prevent stack overflow here!
      char request_buffer[request_len + 1];

      memcpy(request_buffer, connection->buffer, request_len);

      request_buffer[request_len] = '\0';
      printf("Request buffer extracted: %s\n", request_buffer);

      // Check if we can correctly shift from the part after '\r\n'
      // next_message_start_idx is same as the request_buffer plus two bytes
      // for CLRF.
      const int next_message_start_idx =
          request_len + 2; // TODO make "2" a non-magic number for clarity

      assert(next_message_start_idx <= G_MAX_BUFFER_SIZE - 1 &&
             "unreachable case: a single HTTP message occupied "
             "the entire connection buffer");

      // This is a faster way to "circular shift" our array/pop the first
      // message out.
      const int next_message_size =
          connection->buffer_size - next_message_start_idx;

      memmove(&connection->buffer[0],
              &connection->buffer[next_message_start_idx], next_message_size);

      // Make sure the invalid data is truly "unmeaningful" by resetting
      // them to 0.
      memset(&connection->buffer[next_message_size], 0,
             connection->buffer_size - next_message_size);

      connection->buffer_size -= next_message_start_idx;
      connection->write_offset -= next_message_start_idx;

      printf("Connection buffer after shifting: %.*s\n",
             connection->buffer_size, connection->buffer);
      printf("Size after shifting: %d\n", connection->buffer_size);

      // Whenever we "popped" the buffer, we will restart from 0.
      connection->check_offset = 0;

      // TODO: return this to main loop
      if (parse_simple_request(&http_server->http_parser_module,
                               http_simple_request, request_buffer)) {
        printf("http_simple_request parsed: %s\n",
               http_simple_request->request_uri);
        return true;
      } else {
        printf("http_simple_request parsing rejected.\n");
        return false;
      }
    }
  }

  connection->check_offset = i;
  printf("No complete potential HTTP message found.\n");
  return false;
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

    // TODO: Change to less aggressive error handling here.
    if (num_ready_fds == -1) {
      err(EXIT_FAILURE, "epoll_wait failed");
    }

    for (int i = 0; i < num_ready_fds; ++i) {
      if (epoll_events[i].data.fd == listen_socket) {
        struct sockaddr_in client_addr;

        socklen_t client_addr_len;

        int client_connection_socket = accept(
            listen_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        // TODO: Change to less aggressive error handling here.
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

        // TODO: Change to less aggressive error handling here.
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_connection_socket,
                      &epoll_event) == -1) {
          err(EXIT_FAILURE, "epoll_ctl failed for client_connection_socket");
        }
        printf("Registered new client to epoll instance.\n");

        struct Connection client_connection = {.buffer = {},
                                               .buffer_size = 0,
                                               .check_offset = 0,
                                               .write_offset = 0};

        // TODO: Handle the case where client_connection_socket assigned is greater than G_MAX_CLIENT_SOCKETS, which may occur if some other files are open.
        // e.g. G_MAX_CLIENT_SOCKETS = 1000, but latest client_connection_socket is 1002.
        // This is different from the above rejection, as that is only concerned with client count.
        assert(client_connection_socket < G_MAX_CLIENT_SOCKETS && "client_connection_socket exceeds G_MAX_CLIENT_SOCKETS, this is a BUG");
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

        bool message_available = false;
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

            message_available = true;
          } else if (num_bytes == -1) {
              if (errno == EAGAIN || errno == EWOULDBLOCK) {
                  printf("Socket drained for client.\n");
                  break;
              } else {
                  perror("recv failed");

                  assert(false && "UNIMPLEMENTED PATH");
                  // TODO: cleanup (reduce num_sockets, clear connection buffer, close socket)

                  message_available = false;
                  break;
              }
          } else if (num_bytes == 0) {
            // TODO: refactor as cleanup client function so that above error case can also use it
            http_server->network_io_module.num_sockets -= 1;

            assert(http_server->network_io_module.num_sockets >= 1);

            struct Connection empty_connection = {.buffer = {},
                                                  .buffer_size = 0,
                                                  .write_offset = 0,
                                                  .check_offset = 0};

            *client_connection = empty_connection;

            close(client_connection_socket); // Ensures we always start from the
                                             // minimum available fd.

            message_available = false;

            printf("Orderly shutdown of connection by a client during socket "
                   "draining.\n");
            break;
          }


        }

        // TODO: While true check here. This is a design tradeoff.
        if (message_available) {
          struct HttpSimpleRequest http_simple_request = {0};
          char data_buffer[G_MAX_FILE_READ_SIZE] = {0};

          while (process_connection_buffer(http_server, client_connection,
                                           &http_simple_request)) {
            print_http_simple_request(&http_simple_request);

            memset(data_buffer, 0,
                   sizeof(data_buffer)); // Ensure a clean data buffer
            size_t num_bytes =
                get_file_data(data_buffer, http_simple_request.request_uri);

            if (num_bytes != -1) {
              printf("Sending file data to client...\n");
              send(client_connection_socket, data_buffer, num_bytes, MSG_NOSIGNAL);
            } else {
              fprintf(stderr, "Failed to get file data.\n");
            }
          }
        }

        // TODO: Close client connection and remove from connection pool!
      }
    }
  }
}

#endif
