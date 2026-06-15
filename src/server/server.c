#include "server.h"

#include "chunk.h"
#include "config.h"
#include "file_utils.h"
#include "net_compat.h"
#include "protocol.h"
#include "socket_utils.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int receive_upload_metadata(int client_fd, MessageHeader *header, char *filename,
                                   size_t filename_size, uint64_t *file_size) {
    if (header->payload_size < sizeof(uint32_t) + sizeof(uint64_t)) {
        return -1;
    }

    uint32_t net_filename_length;
    uint64_t net_file_size;
    if (recv_all(client_fd, &net_filename_length, sizeof(net_filename_length)) != 0 ||
        recv_all(client_fd, &net_file_size, sizeof(net_file_size)) != 0) {
        return -1;
    }

    uint32_t filename_length = ntohl(net_filename_length);
    *file_size = ntohll(net_file_size);

    if (filename_length == 0 || filename_length >= filename_size ||
        header->payload_size != sizeof(uint32_t) + sizeof(uint64_t) + filename_length) {
        return -1;
    }

    if (recv_all(client_fd, filename, filename_length) != 0) {
        return -1;
    }
    filename[filename_length] = '\0';
    return 0;
}

void start_server(int port) {
    if (ensure_directory_exists(SERVER_FILES_DIR) != 0) {
        fprintf(stderr, "Cannot create %s\n", SERVER_FILES_DIR);
        return;
    }

    int server_fd = create_server_socket(port);
    if (server_fd < 0) {
        return;
    }

    printf("ShareNet server is listening on port %d\n", port);

    while (1) {
        int client_fd = accept_client(server_fd);
        if (client_fd < 0) {
            continue;
        }

        handle_client(client_fd);
        close_socket(client_fd);
    }
}

void handle_client(int client_fd) {
    MessageHeader header;
    if (recv_message_header(client_fd, &header) != 0) {
        return;
    }

    switch (header.type) {
        case MSG_REQUEST_LIST:
            handle_list_request(client_fd);
            break;
        case MSG_REQUEST_UPLOAD:
            handle_upload_request(client_fd, &header);
            break;
        case MSG_DOWNLOAD_REQUEST:
            handle_download_request(client_fd, &header);
            break;
        case MSG_EXIT:
            break;
        default:
            send_error_message(client_fd, "Unknown request type");
            break;
    }
}

void handle_list_request(int client_fd) {
    char output[MAX_FILE_LIST_SIZE];
    list_files_in_directory(SERVER_FILES_DIR, output, sizeof(output));
    send_string_payload(client_fd, MSG_LIST_RESPONSE, output);
}

void handle_upload_request(int client_fd, MessageHeader *header) {
    char filename[MAX_FILENAME];
    uint64_t file_size = 0;

    if (receive_upload_metadata(client_fd, header, filename, sizeof(filename), &file_size) != 0) {
        send_error_message(client_fd, "Invalid upload metadata");
        return;
    }

    if (!is_safe_filename(filename)) {
        send_error_message(client_fd, "Unsafe filename rejected");
        return;
    }

    char output_path[1024];
    if (build_path(output_path, sizeof(output_path), SERVER_FILES_DIR, filename) != 0) {
        send_error_message(client_fd, "Server path is too long");
        return;
    }

    uint32_t expected_chunks = calculate_total_chunks((long)file_size, CHUNK_SIZE);
    if (receive_file_chunks(client_fd, output_path, expected_chunks) != 0) {
        send_error_message(client_fd, "Upload failed while receiving chunks");
        return;
    }

    send_string_payload(client_fd, MSG_UPLOAD_RESPONSE, "Upload completed successfully");
}

void handle_download_request(int client_fd, MessageHeader *header) {
    if (header->payload_size == 0 || header->payload_size >= MAX_FILENAME) {
        send_error_message(client_fd, "Invalid filename length");
        return;
    }

    char filename[MAX_FILENAME];
    if (recv_all(client_fd, filename, header->payload_size) != 0) {
        return;
    }
    filename[header->payload_size] = '\0';

    if (!is_safe_filename(filename)) {
        send_error_message(client_fd, "Unsafe filename rejected");
        return;
    }

    char path[1024];
    if (build_path(path, sizeof(path), SERVER_FILES_DIR, filename) != 0) {
        send_error_message(client_fd, "Server path is too long");
        return;
    }

    if (!file_exists(path)) {
        send_error_message(client_fd, "File not found on server");
        return;
    }

    if (send_file_chunks(client_fd, path) != 0) {
        send_error_message(client_fd, "Download failed while sending chunks");
    }
}
