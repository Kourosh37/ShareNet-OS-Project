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
    return create_server_socket_at(NULL, port);
}

int create_server_socket_at(const char *ip, int port) {
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
    address.sin_port = htons((uint16_t)port);
    if (ip == NULL || ip[0] == '\0' || strcmp(ip, "0.0.0.0") == 0) {
        address.sin_addr.s_addr = INADDR_ANY;
    } else if (inet_pton(AF_INET, ip, &address.sin_addr) <= 0) {
        fprintf(stderr, "Invalid bind IP: %s\n", ip);
        close_socket(server_fd);
        return -1;
    }

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
    return connect_to_server_timeout(ip, port, 5000);
}

int connect_to_server_timeout(const char *ip, int port, int timeout_ms) {
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

    if (set_socket_nonblocking(socket_fd, 1) != 0) {
        close_socket(socket_fd);
        return -1;
    }

    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
#ifdef _WIN32
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK && error != WSAEINPROGRESS && error != WSAEINVAL) {
            perror("connect");
            close_socket(socket_fd);
            return -1;
        }
#else
        if (errno != EINPROGRESS) {
            perror("connect");
            close_socket(socket_fd);
            return -1;
        }
#endif

        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(socket_fd, &write_fds);

        struct timeval timeout;
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;

        int select_result = select(socket_fd + 1, NULL, &write_fds, NULL, &timeout);
        if (select_result <= 0) {
            fprintf(stderr, "connect timeout or select failure\n");
            close_socket(socket_fd);
            return -1;
        }

        int socket_error = 0;
        socklen_t error_size = sizeof(socket_error);
        if (getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, (char *)&socket_error, &error_size) != 0 ||
            socket_error != 0) {
            fprintf(stderr, "connect failed\n");
            close_socket(socket_fd);
            return -1;
        }
    }

    if (set_socket_nonblocking(socket_fd, 0) != 0 || set_socket_timeouts(socket_fd, timeout_ms) != 0) {
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

int set_socket_timeouts(int socket_fd, int timeout_ms) {
#ifdef _WIN32
    DWORD timeout = (DWORD)timeout_ms;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout)) != 0 ||
        setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout)) != 0) {
        return -1;
    }
#else
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0 ||
        setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0) {
        return -1;
    }
#endif
    return 0;
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
