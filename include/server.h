#ifndef SHARENET_SERVER_H
#define SHARENET_SERVER_H

#include "protocol.h"

void start_server(int port);
void handle_client(int client_fd);
void handle_list_request(int client_fd);
void handle_upload_request(int client_fd, MessageHeader *header);
void handle_download_request(int client_fd, MessageHeader *header);

#endif
