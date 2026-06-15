#include "chunk.h"

#include "config.h"
#include "protocol.h"
#include "socket_utils.h"

#include <stdio.h>
#include <stdlib.h>

#include "net_compat.h"

uint32_t calculate_total_chunks(long file_size, uint32_t chunk_size) {
    if (file_size <= 0) {
        return 0;
    }
    return (uint32_t)(((uint64_t)file_size + chunk_size - 1) / chunk_size);
}

static int send_chunk_header(int socket_fd, uint32_t index, uint32_t total, uint32_t size) {
    ChunkHeader header;
    header.chunk_index = htonl(index);
    header.total_chunks = htonl(total);
    header.chunk_size = htonl(size);
    return send_all(socket_fd, &header, sizeof(header));
}

static void decode_chunk_header(ChunkHeader *header) {
    header->chunk_index = ntohl(header->chunk_index);
    header->total_chunks = ntohl(header->total_chunks);
    header->chunk_size = ntohl(header->chunk_size);
}

int send_file_chunks(int socket_fd, const char *file_path) {
    FILE *file = fopen(file_path, "rb");
    if (file == NULL) {
        return -1;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }
    long file_size = ftell(file);
    if (file_size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }

    uint32_t total_chunks = calculate_total_chunks(file_size, CHUNK_SIZE);
    unsigned char buffer[CHUNK_SIZE];

    for (uint32_t index = 0; index < total_chunks; index++) {
        size_t read_count = fread(buffer, 1, sizeof(buffer), file);
        if (ferror(file)) {
            fclose(file);
            return -1;
        }

        if (send_message_header(socket_fd, MSG_CHUNK_DATA, sizeof(ChunkHeader) + (uint32_t)read_count) != 0 ||
            send_chunk_header(socket_fd, index, total_chunks, (uint32_t)read_count) != 0 ||
            send_all(socket_fd, buffer, read_count) != 0) {
            fclose(file);
            return -1;
        }
    }

    fclose(file);
    return send_message_header(socket_fd, MSG_DONE_TRANSFER, 0);
}

int receive_file_chunks(int socket_fd, const char *output_path, uint32_t expected_total_chunks) {
    FILE *file = fopen(output_path, "wb");
    if (file == NULL) {
        return -1;
    }

    unsigned char buffer[CHUNK_SIZE];
    uint32_t expected_index = 0;

    while (1) {
        MessageHeader message;
        if (recv_message_header(socket_fd, &message) != 0) {
            fclose(file);
            return -1;
        }

        if (message.type == MSG_DONE_TRANSFER) {
            break;
        }

        if (message.type != MSG_CHUNK_DATA || message.payload_size < sizeof(ChunkHeader)) {
            fclose(file);
            return -1;
        }

        ChunkHeader chunk;
        if (recv_all(socket_fd, &chunk, sizeof(chunk)) != 0) {
            fclose(file);
            return -1;
        }
        decode_chunk_header(&chunk);

        if (chunk.chunk_index != expected_index || chunk.chunk_size > CHUNK_SIZE) {
            fclose(file);
            return -1;
        }
        if (expected_total_chunks != 0 && chunk.total_chunks != expected_total_chunks) {
            fclose(file);
            return -1;
        }
        if (message.payload_size != sizeof(ChunkHeader) + chunk.chunk_size) {
            fclose(file);
            return -1;
        }

        if (chunk.chunk_size > 0 && recv_all(socket_fd, buffer, chunk.chunk_size) != 0) {
            fclose(file);
            return -1;
        }
        if (chunk.chunk_size > 0 && fwrite(buffer, 1, chunk.chunk_size, file) != chunk.chunk_size) {
            fclose(file);
            return -1;
        }

        expected_index++;
    }

    if (expected_total_chunks != 0 && expected_index != expected_total_chunks) {
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}
