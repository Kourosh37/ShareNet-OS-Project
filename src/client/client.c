#include "client.h"

#include "chunk.h"
#include "config.h"
#include "file_utils.h"
#include "protocol.h"
#include "socket_utils.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "net_compat.h"

static int read_line(char *buffer, size_t size) {
    if (fgets(buffer, (int)size, stdin) == NULL) {
        buffer[0] = '\0';
        return 0;
    }

    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }
    return 1;
}

static int send_upload_metadata(int socket_fd, const char *filename, uint64_t file_size) {
    uint32_t filename_length = (uint32_t)strlen(filename);
    uint32_t payload_size = sizeof(uint32_t) + sizeof(uint64_t) + filename_length;
    uint32_t net_filename_length = htonl(filename_length);
    uint64_t net_file_size = htonll(file_size);

    if (send_message_header(socket_fd, MSG_REQUEST_UPLOAD, payload_size) != 0 ||
        send_all(socket_fd, &net_filename_length, sizeof(net_filename_length)) != 0 ||
        send_all(socket_fd, &net_file_size, sizeof(net_file_size)) != 0 ||
        send_all(socket_fd, filename, filename_length) != 0) {
        return -1;
    }
    return 0;
}

void start_client(const char *server_ip, int port) {
    if (ensure_directory_exists(CLIENT_FILES_DIR) != 0 ||
        ensure_directory_exists(DOWNLOADS_DIR) != 0) {
        fprintf(stderr, "Cannot create client directories\n");
        return;
    }

    while (1) {
        char input[32];
        show_menu();
        printf("Select an option: ");
        if (!read_line(input, sizeof(input))) {
            printf("\nInput closed. Exiting.\n");
            return;
        }

        int choice = atoi(input);
        switch (choice) {
            case 1:
                request_file_list(server_ip, port);
                break;
            case 2:
                upload_file(server_ip, port);
                break;
            case 3:
                download_file(server_ip, port);
                break;
            case 4:
                printf("Goodbye.\n");
                return;
            default:
                printf("Invalid option.\n");
                break;
        }
    }
}

void show_menu(void) {
    printf("\nShareNet Client\n");
    printf("1. Show file list\n");
    printf("2. Upload file\n");
    printf("3. Download file\n");
    printf("4. Exit\n");
}

void request_file_list(const char *server_ip, int port) {
    int socket_fd = connect_to_server(server_ip, port);
    if (socket_fd < 0) {
        return;
    }

    if (send_message_header(socket_fd, MSG_REQUEST_LIST, 0) != 0) {
        fprintf(stderr, "Could not send list request\n");
        close_socket(socket_fd);
        return;
    }

    MessageHeader header;
    if (recv_message_header(socket_fd, &header) != 0) {
        fprintf(stderr, "Could not receive server response\n");
        close_socket(socket_fd);
        return;
    }

    if (header.payload_size >= MAX_FILE_LIST_SIZE) {
        fprintf(stderr, "File list is too large\n");
        close_socket(socket_fd);
        return;
    }

    char list[MAX_FILE_LIST_SIZE];
    if (header.payload_size > 0 && recv_all(socket_fd, list, header.payload_size) != 0) {
        fprintf(stderr, "Could not receive file list\n");
        close_socket(socket_fd);
        return;
    }
    list[header.payload_size] = '\0';

    if (header.type == MSG_LIST_RESPONSE) {
        printf("\nServer files:\n%s", list);
    } else if (header.type == MSG_ERROR) {
        printf("Server error: %s\n", list);
    } else {
        printf("Unexpected server response.\n");
    }

    close_socket(socket_fd);
}

void upload_file(const char *server_ip, int port) {
    char path[1024];
    printf("Enter file path to upload: ");
    if (!read_line(path, sizeof(path))) {
        printf("\nInput closed.\n");
        return;
    }

    const char *filename = basename_from_path(path);
    if (!is_safe_filename(filename)) {
        printf("Unsafe or empty filename.\n");
        return;
    }

    long file_size = get_file_size(path);
    if (file_size < 0) {
        printf("File does not exist or is not a regular file.\n");
        return;
    }

    int socket_fd = connect_to_server(server_ip, port);
    if (socket_fd < 0) {
        return;
    }

    if (send_upload_metadata(socket_fd, filename, (uint64_t)file_size) != 0 ||
        send_file_chunks(socket_fd, path) != 0) {
        fprintf(stderr, "Upload failed while sending data\n");
        close_socket(socket_fd);
        return;
    }

    char response[BUFFER_SIZE];
    int type = recv_string_payload(socket_fd, response, sizeof(response));
    if (type == MSG_UPLOAD_RESPONSE) {
        printf("%s\n", response);
    } else if (type == MSG_ERROR) {
        printf("Server error: %s\n", response);
    } else {
        printf("Unexpected server response after upload.\n");
    }

    close_socket(socket_fd);
}

void download_file(const char *server_ip, int port) {
    char filename[MAX_FILENAME];
    printf("Enter filename to download: ");
    if (!read_line(filename, sizeof(filename))) {
        printf("\nInput closed.\n");
        return;
    }

    if (!is_safe_filename(filename)) {
        printf("Unsafe or empty filename.\n");
        return;
    }

    char output_path[1024];
    if (build_path(output_path, sizeof(output_path), DOWNLOADS_DIR, filename) != 0) {
        printf("Output path is too long.\n");
        return;
    }

    int socket_fd = connect_to_server(server_ip, port);
    if (socket_fd < 0) {
        return;
    }

    if (send_string_payload(socket_fd, MSG_DOWNLOAD_REQUEST, filename) != 0) {
        fprintf(stderr, "Could not send download request\n");
        close_socket(socket_fd);
        return;
    }

    MessageHeader header;
    if (recv_message_header(socket_fd, &header) != 0) {
        fprintf(stderr, "Could not receive download response\n");
        close_socket(socket_fd);
        return;
    }

    if (header.type == MSG_ERROR) {
        char error[BUFFER_SIZE];
        if (header.payload_size < sizeof(error) &&
            (header.payload_size == 0 || recv_all(socket_fd, error, header.payload_size) == 0)) {
            error[header.payload_size] = '\0';
            printf("Server error: %s\n", error);
        }
        close_socket(socket_fd);
        return;
    }

    if (header.type != MSG_CHUNK_DATA) {
        printf("Unexpected server response.\n");
        close_socket(socket_fd);
        return;
    }

    ChunkHeader chunk_header;
    if (header.payload_size < sizeof(chunk_header) ||
        recv_all(socket_fd, &chunk_header, sizeof(chunk_header)) != 0) {
        fprintf(stderr, "Invalid first chunk\n");
        close_socket(socket_fd);
        return;
    }

    chunk_header.chunk_index = ntohl(chunk_header.chunk_index);
    chunk_header.total_chunks = ntohl(chunk_header.total_chunks);
    chunk_header.chunk_size = ntohl(chunk_header.chunk_size);

    if (chunk_header.chunk_index != 0 || chunk_header.chunk_size > CHUNK_SIZE ||
        header.payload_size != sizeof(ChunkHeader) + chunk_header.chunk_size) {
        fprintf(stderr, "Invalid first chunk metadata\n");
        close_socket(socket_fd);
        return;
    }

    FILE *file = fopen(output_path, "wb");
    if (file == NULL) {
        perror("fopen");
        close_socket(socket_fd);
        return;
    }

    unsigned char buffer[CHUNK_SIZE];
    uint32_t expected_index = 0;

    while (1) {
        if (chunk_header.chunk_index != expected_index ||
            chunk_header.chunk_size > CHUNK_SIZE ||
            header.payload_size != sizeof(ChunkHeader) + chunk_header.chunk_size) {
            fprintf(stderr, "Invalid chunk metadata\n");
            fclose(file);
            close_socket(socket_fd);
            return;
        }

        if (chunk_header.chunk_size > 0 &&
            (recv_all(socket_fd, buffer, chunk_header.chunk_size) != 0 ||
             fwrite(buffer, 1, chunk_header.chunk_size, file) != chunk_header.chunk_size)) {
            fprintf(stderr, "Could not write chunk\n");
            fclose(file);
            close_socket(socket_fd);
            return;
        }

        expected_index++;

        if (recv_message_header(socket_fd, &header) != 0) {
            fprintf(stderr, "Could not receive next chunk header\n");
            fclose(file);
            close_socket(socket_fd);
            return;
        }

        if (header.type == MSG_DONE_TRANSFER) {
            break;
        }
        if (header.type != MSG_CHUNK_DATA || header.payload_size < sizeof(ChunkHeader) ||
            recv_all(socket_fd, &chunk_header, sizeof(chunk_header)) != 0) {
            fprintf(stderr, "Invalid chunk response\n");
            fclose(file);
            close_socket(socket_fd);
            return;
        }

        chunk_header.chunk_index = ntohl(chunk_header.chunk_index);
        chunk_header.total_chunks = ntohl(chunk_header.total_chunks);
        chunk_header.chunk_size = ntohl(chunk_header.chunk_size);
    }

    fclose(file);

    if (expected_index != chunk_header.total_chunks) {
        fprintf(stderr, "Download ended before all chunks were received\n");
        close_socket(socket_fd);
        return;
    }

    printf("Download completed: %s\n", output_path);
    close_socket(socket_fd);
}
