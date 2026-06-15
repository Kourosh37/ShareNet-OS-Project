#ifndef SHARENET_PROTOCOL_H
#define SHARENET_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    MSG_REQUEST_LIST = 1,
    MSG_LIST_RESPONSE,
    MSG_REQUEST_UPLOAD,
    MSG_UPLOAD_RESPONSE,
    MSG_DOWNLOAD_REQUEST,
    MSG_CHUNK_DATA,
    MSG_DONE_TRANSFER,
    MSG_ERROR,
    MSG_EXIT
} MessageType;

typedef struct {
    uint32_t type;
    uint32_t payload_size;
} MessageHeader;

uint64_t htonll(uint64_t value);
uint64_t ntohll(uint64_t value);

int send_message_header(int socket_fd, uint32_t type, uint32_t payload_size);
int recv_message_header(int socket_fd, MessageHeader *header);
int send_error_message(int socket_fd, const char *message);
int recv_string_payload(int socket_fd, char *buffer, size_t buffer_size);
int send_string_payload(int socket_fd, uint32_t type, const char *text);

#endif
