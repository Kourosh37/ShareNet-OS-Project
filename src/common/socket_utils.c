#include "socket_utils.h"

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "net_compat.h"

int init_socket_system(void) {
#ifdef _WIN32
    static int initialized = 0;
    if (!initialized) {
        WSADATA data;
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            fprintf(stderr, "WSAStartup failed\n");
            return -1;
        }
        initialized = 1;
    }
#endif
    return 0;
}

void cleanup_socket_system(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

static int socket_error_would_block(void) {
#ifdef _WIN32
    int error = WSAGetLastError();
    return error == WSAEWOULDBLOCK;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}

int create_server_socket(int port) {
    if (init_socket_system() != 0) {
        return -1;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close_socket(server_fd);
        return -1;
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons((uint16_t)port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind");
        close_socket(server_fd);
        return -1;
    }

    if (listen(server_fd, 8) < 0) {
        perror("listen");
        close_socket(server_fd);
        return -1;
    }

    return server_fd;
}

int accept_client(int server_fd) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (client_fd < 0) {
        if (!socket_error_would_block()) {
            perror("accept");
        }
    }
    return client_fd;
}

int connect_to_server(const char *ip, int port) {
    if (init_socket_system() != 0) {
        return -1;
    }

    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((uint16_t)port);

    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server IP: %s\n", ip);
        close_socket(socket_fd);
        return -1;
    }

    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close_socket(socket_fd);
        return -1;
    }

    return socket_fd;
}

int send_all(int socket_fd, const void *buffer, size_t length) {
    const char *ptr = (const char *)buffer;
    size_t sent_total = 0;

    while (sent_total < length) {
        ssize_t sent = send(socket_fd, ptr + sent_total, length - sent_total, 0);
        if (sent < 0) {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEINTR) {
                continue;
            }
#else
            if (errno == EINTR) {
                continue;
            }
#endif
            return -1;
        }
        if (sent == 0) {
            return -1;
        }
        sent_total += (size_t)sent;
    }

    return 0;
}

int recv_all(int socket_fd, void *buffer, size_t length) {
    char *ptr = (char *)buffer;
    size_t received_total = 0;

    while (received_total < length) {
        ssize_t received = recv(socket_fd, ptr + received_total, length - received_total, 0);
        if (received < 0) {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEINTR) {
                continue;
            }
#else
            if (errno == EINTR) {
                continue;
            }
#endif
            return -1;
        }
        if (received == 0) {
            return -1;
        }
        received_total += (size_t)received;
    }

    return 0;
}

int set_socket_nonblocking(int socket_fd, int nonblocking) {
#ifdef _WIN32
    u_long mode = nonblocking ? 1UL : 0UL;
    return ioctlsocket(socket_fd, FIONBIO, &mode) == 0 ? 0 : -1;
#else
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    if (nonblocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    return fcntl(socket_fd, F_SETFL, flags);
#endif
}

void close_socket(int socket_fd) {
    if (socket_fd >= 0) {
#ifdef _WIN32
        closesocket((SOCKET)socket_fd);
#else
        close(socket_fd);
#endif
    }
}
