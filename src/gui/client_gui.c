#include "chunk.h"
#include "config.h"
#include "file_utils.h"
#include "net_compat.h"
#include "protocol.h"
#include "socket_utils.h"

#include "raylib.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define LOG_LINES 10

typedef struct {
    Rectangle bounds;
    char *text;
    size_t capacity;
    int active;
} TextBox;

static int gui_button(Rectangle rect, const char *text, Color base, Color hover) {
    Vector2 mouse = GetMousePosition();
    int hot = CheckCollisionPointRec(mouse, rect);
    DrawRectangleRounded(rect, 0.12f, 8, hot ? hover : base);
    DrawText(text, (int)(rect.x + 16), (int)(rect.y + 12), 20, RAYWHITE);
    return hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

static void add_log(char logs[LOG_LINES][160], const char *message) {
    for (int i = 0; i < LOG_LINES - 1; i++) {
        strcpy(logs[i], logs[i + 1]);
    }
    snprintf(logs[LOG_LINES - 1], 160, "%s", message);
}

static void update_textbox(TextBox *box) {
    Vector2 mouse = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        box->active = CheckCollisionPointRec(mouse, box->bounds);
    }

    if (!box->active) {
        return;
    }

    int key;
    while ((key = GetCharPressed()) > 0) {
        size_t len = strlen(box->text);
        if (key >= 32 && key <= 126 && len + 1 < box->capacity) {
            box->text[len] = (char)key;
            box->text[len + 1] = '\0';
        }
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
        size_t len = strlen(box->text);
        if (len > 0) {
            box->text[len - 1] = '\0';
        }
    }
}

static void draw_textbox(TextBox *box, const char *label) {
    DrawText(label, (int)box->bounds.x, (int)box->bounds.y - 28, 18, (Color){194, 207, 224, 255});
    DrawRectangleRounded(box->bounds, 0.08f, 8, box->active ? (Color){39, 58, 84, 255} : (Color){29, 40, 56, 255});
    DrawRectangleLinesEx(box->bounds, 2.0f, box->active ? (Color){87, 150, 255, 255} : (Color){61, 77, 100, 255});
    DrawText(box->text, (int)box->bounds.x + 14, (int)box->bounds.y + 13, 19, RAYWHITE);
}

static int send_upload_metadata_gui(int socket_fd, const char *filename, uint64_t file_size) {
    uint32_t filename_length = (uint32_t)strlen(filename);
    uint32_t payload_size = sizeof(uint32_t) + sizeof(uint64_t) + filename_length;
    uint32_t net_filename_length = htonl(filename_length);
    uint64_t net_file_size = htonll(file_size);

    return send_message_header(socket_fd, MSG_REQUEST_UPLOAD, payload_size) == 0 &&
           send_all(socket_fd, &net_filename_length, sizeof(net_filename_length)) == 0 &&
           send_all(socket_fd, &net_file_size, sizeof(net_file_size)) == 0 &&
           send_all(socket_fd, filename, filename_length) == 0 ? 0 : -1;
}

static int gui_request_list(char *output, size_t output_size) {
    int socket_fd = connect_to_server(SERVER_IP, SERVER_PORT);
    if (socket_fd < 0) {
        snprintf(output, output_size, "Could not connect to server.");
        return -1;
    }

    MessageHeader header;
    if (send_message_header(socket_fd, MSG_REQUEST_LIST, 0) != 0 ||
        recv_message_header(socket_fd, &header) != 0 ||
        header.payload_size >= output_size ||
        (header.payload_size > 0 && recv_all(socket_fd, output, header.payload_size) != 0)) {
        close_socket(socket_fd);
        snprintf(output, output_size, "Could not receive file list.");
        return -1;
    }

    output[header.payload_size] = '\0';
    close_socket(socket_fd);
    return header.type == MSG_LIST_RESPONSE ? 0 : -1;
}

static int gui_upload_file(const char *path, char *status, size_t status_size) {
    const char *filename = basename_from_path(path);
    long file_size = get_file_size(path);

    if (!is_safe_filename(filename) || file_size < 0) {
        snprintf(status, status_size, "Invalid upload path or unsafe filename.");
        return -1;
    }

    int socket_fd = connect_to_server(SERVER_IP, SERVER_PORT);
    if (socket_fd < 0) {
        snprintf(status, status_size, "Could not connect to server.");
        return -1;
    }

    if (send_upload_metadata_gui(socket_fd, filename, (uint64_t)file_size) != 0 ||
        send_file_chunks(socket_fd, path) != 0) {
        close_socket(socket_fd);
        snprintf(status, status_size, "Upload failed while sending file.");
        return -1;
    }

    int type = recv_string_payload(socket_fd, status, status_size);
    close_socket(socket_fd);
    return type == MSG_UPLOAD_RESPONSE ? 0 : -1;
}

static int gui_download_file(const char *filename, char *status, size_t status_size) {
    if (!is_safe_filename(filename)) {
        snprintf(status, status_size, "Unsafe or empty filename.");
        return -1;
    }

    char output_path[1024];
    if (build_path(output_path, sizeof(output_path), DOWNLOADS_DIR, filename) != 0) {
        snprintf(status, status_size, "Output path is too long.");
        return -1;
    }

    int socket_fd = connect_to_server(SERVER_IP, SERVER_PORT);
    if (socket_fd < 0) {
        snprintf(status, status_size, "Could not connect to server.");
        return -1;
    }

    MessageHeader header;
    if (send_string_payload(socket_fd, MSG_DOWNLOAD_REQUEST, filename) != 0 ||
        recv_message_header(socket_fd, &header) != 0) {
        close_socket(socket_fd);
        snprintf(status, status_size, "Could not start download.");
        return -1;
    }

    if (header.type == MSG_ERROR) {
        if (header.payload_size < status_size &&
            (header.payload_size == 0 || recv_all(socket_fd, status, header.payload_size) == 0)) {
            status[header.payload_size] = '\0';
        } else {
            snprintf(status, status_size, "Server returned an error.");
        }
        close_socket(socket_fd);
        return -1;
    }

    FILE *file = fopen(output_path, "wb");
    if (file == NULL) {
        close_socket(socket_fd);
        snprintf(status, status_size, "Could not create output file.");
        return -1;
    }

    unsigned char buffer[CHUNK_SIZE];
    uint32_t expected_index = 0;
    uint32_t total_chunks = 0;

    while (header.type == MSG_CHUNK_DATA) {
        ChunkHeader chunk;
        if (header.payload_size < sizeof(chunk) || recv_all(socket_fd, &chunk, sizeof(chunk)) != 0) {
            fclose(file);
            close_socket(socket_fd);
            snprintf(status, status_size, "Invalid chunk header.");
            return -1;
        }

        chunk.chunk_index = ntohl(chunk.chunk_index);
        chunk.total_chunks = ntohl(chunk.total_chunks);
        chunk.chunk_size = ntohl(chunk.chunk_size);
        total_chunks = chunk.total_chunks;

        if (chunk.chunk_index != expected_index || chunk.chunk_size > CHUNK_SIZE ||
            header.payload_size != sizeof(ChunkHeader) + chunk.chunk_size) {
            fclose(file);
            close_socket(socket_fd);
            snprintf(status, status_size, "Invalid chunk order.");
            return -1;
        }

        if (chunk.chunk_size > 0 &&
            (recv_all(socket_fd, buffer, chunk.chunk_size) != 0 ||
             fwrite(buffer, 1, chunk.chunk_size, file) != chunk.chunk_size)) {
            fclose(file);
            close_socket(socket_fd);
            snprintf(status, status_size, "Could not write downloaded data.");
            return -1;
        }

        expected_index++;
        if (recv_message_header(socket_fd, &header) != 0) {
            fclose(file);
            close_socket(socket_fd);
            snprintf(status, status_size, "Connection closed before transfer completed.");
            return -1;
        }
    }

    fclose(file);
    close_socket(socket_fd);

    if (header.type != MSG_DONE_TRANSFER || expected_index != total_chunks) {
        snprintf(status, status_size, "Download ended unexpectedly.");
        return -1;
    }

    snprintf(status, status_size, "Downloaded to %s", output_path);
    return 0;
}

int main(void) {
    InitWindow(1040, 680, "ShareNet Client - Raylib");
    SetTargetFPS(60);

    ensure_directory_exists(CLIENT_FILES_DIR);
    ensure_directory_exists(DOWNLOADS_DIR);

    char upload_path[512] = "client_files/sample.txt";
    char download_name[MAX_FILENAME] = "sample.txt";
    char file_list[MAX_FILE_LIST_SIZE] = "Click List Files to load server files.\n";
    char logs[LOG_LINES][160] = {{0}};

    TextBox upload_box = {{40, 130, 610, 50}, upload_path, sizeof(upload_path), 0};
    TextBox download_box = {{40, 235, 610, 50}, download_name, sizeof(download_name), 0};
    add_log(logs, "Client GUI ready.");

    while (!WindowShouldClose()) {
        update_textbox(&upload_box);
        update_textbox(&download_box);

        BeginDrawing();
        ClearBackground((Color){17, 23, 31, 255});
        DrawText("ShareNet Client", 38, 28, 34, RAYWHITE);
        DrawText("Upload, list and download files over TCP", 40, 68, 18, (Color){171, 186, 204, 255});

        draw_textbox(&upload_box, "Upload path");
        draw_textbox(&download_box, "Download filename");

        if (gui_button((Rectangle){690, 130, 220, 50}, "Upload File", (Color){49, 107, 214, 255},
                       (Color){75, 132, 238, 255})) {
            char status[160];
            gui_upload_file(upload_path, status, sizeof(status));
            add_log(logs, status);
        }

        if (gui_button((Rectangle){690, 205, 220, 50}, "List Files", (Color){54, 139, 116, 255},
                       (Color){74, 166, 139, 255})) {
            if (gui_request_list(file_list, sizeof(file_list)) == 0) {
                add_log(logs, "File list refreshed.");
            } else {
                add_log(logs, file_list);
            }
        }

        if (gui_button((Rectangle){690, 280, 220, 50}, "Download File", (Color){180, 109, 46, 255},
                       (Color){206, 132, 65, 255})) {
            char status[160];
            gui_download_file(download_name, status, sizeof(status));
            add_log(logs, status);
        }

        DrawRectangleRounded((Rectangle){40, 340, 458, 292}, 0.04f, 8, (Color){27, 35, 48, 255});
        DrawText("Server Files", 64, 364, 24, RAYWHITE);
        DrawRectangle(64, 404, 360, 1, (Color){64, 78, 98, 255});
        DrawText(file_list, 64, 428, 18, (Color){219, 226, 235, 255});

        DrawRectangleRounded((Rectangle){532, 340, 468, 292}, 0.04f, 8, (Color){27, 35, 48, 255});
        DrawText("Activity Log", 556, 364, 24, RAYWHITE);
        DrawRectangle(556, 404, 360, 1, (Color){64, 78, 98, 255});
        for (int i = 0; i < LOG_LINES; i++) {
            if (logs[i][0] != '\0') {
                DrawText(logs[i], 556, 428 + i * 22, 16, (Color){219, 226, 235, 255});
            }
        }

        DrawText("Server: 127.0.0.1:5000", 40, 650, 16, (Color){139, 154, 174, 255});
        EndDrawing();
    }

    cleanup_socket_system();
    CloseWindow();
    return 0;
}
