#ifndef SHARENET_SOCKET_UTILS_H
#define SHARENET_SOCKET_UTILS_H

#include <stddef.h>

int init_socket_system(void);
void cleanup_socket_system(void);
int create_server_socket(int port);
int create_server_socket_at(const char *ip, int port);
int accept_client(int server_fd);
int connect_to_server(const char *ip, int port);
int connect_to_server_timeout(const char *ip, int port, int timeout_ms);
int send_all(int socket_fd, const void *buffer, size_t length);
int recv_all(int socket_fd, void *buffer, size_t length);
int set_socket_nonblocking(int socket_fd, int nonblocking);
int set_socket_timeouts(int socket_fd, int timeout_ms);
void close_socket(int socket_fd);

#endif
