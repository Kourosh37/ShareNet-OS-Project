CC := gcc
CFLAGS := -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -Iinclude

COMMON_SRC := src/common/protocol.c src/common/socket_utils.c src/common/file_utils.c src/common/chunk.c
SERVER_SRC := src/server/server_main.c src/server/server.c
CLIENT_SRC := src/client/client_main.c src/client/client.c
SERVER_GUI_SRC := src/gui/server_gui.c src/server/server.c
CLIENT_GUI_SRC := src/gui/client_gui.c

SERVER_BIN := sharenet_server
CLIENT_BIN := sharenet_client
SERVER_GUI_BIN := sharenet_server_gui
CLIENT_GUI_BIN := sharenet_client_gui

RAYLIB_CFLAGS ?= $(shell pkg-config --cflags raylib 2>/dev/null)
RAYLIB_LIBS ?= $(shell pkg-config --libs raylib 2>/dev/null || printf "%s" "-lraylib -lm -ldl -lpthread -lGL -lrt -lX11")

.PHONY: all gui clean run-server run-client run-server-gui run-client-gui

all: $(SERVER_BIN) $(CLIENT_BIN)

gui: $(SERVER_GUI_BIN) $(CLIENT_GUI_BIN)

$(SERVER_BIN): $(COMMON_SRC) $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $^

$(CLIENT_BIN): $(COMMON_SRC) $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $^

$(SERVER_GUI_BIN): $(COMMON_SRC) $(SERVER_GUI_SRC)
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) -o $@ $^ $(RAYLIB_LIBS)

$(CLIENT_GUI_BIN): $(COMMON_SRC) $(CLIENT_GUI_SRC)
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) -o $@ $^ $(RAYLIB_LIBS)

run-server: $(SERVER_BIN)
	./$(SERVER_BIN)

run-client: $(CLIENT_BIN)
	./$(CLIENT_BIN)

run-server-gui: $(SERVER_GUI_BIN)
	./$(SERVER_GUI_BIN)

run-client-gui: $(CLIENT_GUI_BIN)
	./$(CLIENT_GUI_BIN)

clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN) $(SERVER_GUI_BIN) $(CLIENT_GUI_BIN) *.exe *.o *.out
