#include "config.h"
#include "file_utils.h"
#include "server.h"
#include "socket_utils.h"

#include "raylib.h"

#include <stdio.h>
#include <string.h>

#define LOG_LINES 12

static int gui_button(Rectangle rect, const char *text, Color base, Color hover) {
    Vector2 mouse = GetMousePosition();
    int hot = CheckCollisionPointRec(mouse, rect);
    DrawRectangleRounded(rect, 0.12f, 8, hot ? hover : base);
    DrawText(text, (int)(rect.x + 18), (int)(rect.y + 12), 20, RAYWHITE);
    return hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

static void add_log(char logs[LOG_LINES][128], const char *message) {
    for (int i = 0; i < LOG_LINES - 1; i++) {
        strcpy(logs[i], logs[i + 1]);
    }
    snprintf(logs[LOG_LINES - 1], 128, "%s", message);
}

int main(void) {
    const int width = 980;
    const int height = 640;
    InitWindow(width, height, "ShareNet Server - Raylib");
    SetTargetFPS(60);

    char logs[LOG_LINES][128] = {{0}};
    char file_list[MAX_FILE_LIST_SIZE] = "Click Refresh Files to load server_files.\n";
    int server_fd = -1;
    int listening = 0;

    ensure_directory_exists(SERVER_FILES_DIR);
    add_log(logs, "Server GUI ready.");

    while (!WindowShouldClose()) {
        if (listening && server_fd >= 0) {
            int client_fd = accept_client(server_fd);
            if (client_fd >= 0) {
                add_log(logs, "Client connected. Processing request...");
                handle_client(client_fd);
                close_socket(client_fd);
                add_log(logs, "Client request completed.");
                list_files_in_directory(SERVER_FILES_DIR, file_list, sizeof(file_list));
            }
        }

        BeginDrawing();
        ClearBackground((Color){18, 24, 33, 255});

        DrawText("ShareNet Server", 32, 26, 34, RAYWHITE);
        DrawText("Single-threaded TCP file server", 34, 66, 18, (Color){171, 186, 204, 255});

        Color status = listening ? (Color){38, 184, 111, 255} : (Color){211, 75, 75, 255};
        DrawCircle(820, 44, 9, status);
        DrawText(listening ? "Listening on port 5000" : "Stopped", 840, 34, 20, RAYWHITE);

        if (!listening) {
            if (gui_button((Rectangle){34, 112, 180, 48}, "Start Server", (Color){49, 107, 214, 255},
                           (Color){72, 130, 238, 255})) {
                server_fd = create_server_socket(SERVER_PORT);
                if (server_fd >= 0 && set_socket_nonblocking(server_fd, 1) == 0) {
                    listening = 1;
                    add_log(logs, "Listening socket opened.");
                } else {
                    if (server_fd >= 0) {
                        close_socket(server_fd);
                        server_fd = -1;
                    }
                    add_log(logs, "Failed to start server.");
                }
            }
        } else {
            if (gui_button((Rectangle){34, 112, 180, 48}, "Stop Server", (Color){184, 63, 79, 255},
                           (Color){211, 83, 100, 255})) {
                close_socket(server_fd);
                server_fd = -1;
                listening = 0;
                add_log(logs, "Server stopped.");
            }
        }

        if (gui_button((Rectangle){232, 112, 190, 48}, "Refresh Files", (Color){54, 139, 116, 255},
                       (Color){73, 166, 139, 255})) {
            list_files_in_directory(SERVER_FILES_DIR, file_list, sizeof(file_list));
            add_log(logs, "File list refreshed.");
        }

        DrawRectangleRounded((Rectangle){34, 190, 430, 398}, 0.04f, 8, (Color){27, 35, 48, 255});
        DrawText("Server Files", 58, 214, 24, RAYWHITE);
        DrawRectangle((int)58, 254, 360, 1, (Color){64, 78, 98, 255});
        DrawText(file_list, 58, 278, 18, (Color){219, 226, 235, 255});

        DrawRectangleRounded((Rectangle){498, 190, 448, 398}, 0.04f, 8, (Color){27, 35, 48, 255});
        DrawText("Activity Log", 522, 214, 24, RAYWHITE);
        DrawRectangle(522, 254, 360, 1, (Color){64, 78, 98, 255});
        for (int i = 0; i < LOG_LINES; i++) {
            if (logs[i][0] != '\0') {
                DrawText(logs[i], 522, 278 + i * 24, 17, (Color){219, 226, 235, 255});
            }
        }

        DrawText("This GUI uses non-blocking accept, then handles one client at a time.", 34, 608, 16,
                 (Color){139, 154, 174, 255});
        EndDrawing();
    }

    if (server_fd >= 0) {
        close_socket(server_fd);
    }
    cleanup_socket_system();
    CloseWindow();
    return 0;
}
