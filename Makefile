CC := gcc
CFLAGS := -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -Iinclude

COMMON_SRC := src/common/protocol.c src/common/socket_utils.c src/common/file_utils.c src/common/chunk.c
SERVER_SRC := src/server/server_main.c src/server/server.c
CLIENT_SRC := src/client/client_main.c src/client/client.c

SERVER_BIN := sharenet_server
CLIENT_BIN := sharenet_client

.PHONY: all fltk clean run-server run-client

all: $(SERVER_BIN) $(CLIENT_BIN)

fltk:
	cmake -S src/fltk -B build/fltk
	cmake --build build/fltk

$(SERVER_BIN): $(COMMON_SRC) $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $^

$(CLIENT_BIN): $(COMMON_SRC) $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $^

run-server: $(SERVER_BIN)
	./$(SERVER_BIN)

run-client: $(CLIENT_BIN)
	./$(CLIENT_BIN)

clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN) *.exe *.o *.out
	rm -rf build/fltk
