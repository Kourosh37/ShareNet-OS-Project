#include "protocol.h"

#include "socket_utils.h"

#include <stdio.h>
#include <string.h>

#include "net_compat.h"

uint64_t htonll(uint64_t value) {
    uint32_t high = htonl((uint32_t)(value >> 32));
    uint32_t low = htonl((uint32_t)(value & 0xffffffffULL));
    return ((uint64_t)low << 32) | high;
}

uint64_t ntohll(uint64_t value) {
    uint32_t high = ntohl((uint32_t)(value >> 32));
    uint32_t low = ntohl((uint32_t)(value & 0xffffffffULL));
    return ((uint64_t)low << 32) | high;
}

int send_message_header(int socket_fd, uint32_t type, uint32_t payload_size) {
    MessageHeader header;
    header.type = htonl(type);
    header.payload_size = htonl(payload_size);
    return send_all(socket_fd, &header, sizeof(header));
}

int recv_message_header(int socket_fd, MessageHeader *header) {
    if (recv_all(socket_fd, header, sizeof(*header)) != 0) {
        return -1;
    }

    header->type = ntohl(header->type);
    header->payload_size = ntohl(header->payload_size);
    return 0;
}

int send_error_message(int socket_fd, const char *message) {
    return send_string_payload(socket_fd, MSG_ERROR, message);
}

int recv_string_payload(int socket_fd, char *buffer, size_t buffer_size) {
    MessageHeader header;
    if (recv_message_header(socket_fd, &header) != 0) {
        return -1;
    }

    if (header.payload_size >= buffer_size) {
        return -1;
    }

    if (header.payload_size > 0 && recv_all(socket_fd, buffer, header.payload_size) != 0) {
        return -1;
    }

    buffer[header.payload_size] = '\0';
    return (int)header.type;
}

int send_string_payload(int socket_fd, uint32_t type, const char *text) {
    uint32_t length = (uint32_t)strlen(text);
    if (send_message_header(socket_fd, type, length) != 0) {
        return -1;
    }
    if (length == 0) {
        return 0;
    }
    return send_all(socket_fd, text, length);
}
