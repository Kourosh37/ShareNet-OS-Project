#ifndef SHARENET_CHUNK_H
#define SHARENET_CHUNK_H

#include <stdint.h>

typedef struct {
    uint32_t chunk_index;
    uint32_t total_chunks;
    uint32_t chunk_size;
} ChunkHeader;

uint32_t calculate_total_chunks(long file_size, uint32_t chunk_size);
int send_file_chunks(int socket_fd, const char *file_path);
int receive_file_chunks(int socket_fd, const char *output_path, uint32_t expected_total_chunks);

#endif
