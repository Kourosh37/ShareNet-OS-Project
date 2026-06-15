#ifndef SHARENET_FILE_UTILS_H
#define SHARENET_FILE_UTILS_H

#include <stddef.h>

int ensure_directory_exists(const char *dir);
int file_exists(const char *path);
long get_file_size(const char *path);
int build_path(char *dest, size_t dest_size, const char *dir, const char *filename);
int is_safe_filename(const char *filename);
int list_files_in_directory(const char *dir, char *output, size_t output_size);
const char *basename_from_path(const char *path);

#endif
