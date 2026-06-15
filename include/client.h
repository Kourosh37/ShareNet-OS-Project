#ifndef SHARENET_CLIENT_H
#define SHARENET_CLIENT_H

void start_client(const char *server_ip, int port);
void show_menu(void);
void request_file_list(const char *server_ip, int port);
void upload_file(const char *server_ip, int port);
void download_file(const char *server_ip, int port);

#endif
