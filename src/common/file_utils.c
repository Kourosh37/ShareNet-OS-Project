#include "file_utils.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#endif

static int make_directory(const char *dir) {
#ifdef _WIN32
    return _mkdir(dir);
#else
    return mkdir(dir, 0755);
#endif
}

int ensure_directory_exists(const char *dir) {
    struct stat st;
    if (stat(dir, &st) == 0) {
        return S_ISDIR(st.st_mode) ? 0 : -1;
    }

    if (make_directory(dir) != 0 && errno != EEXIST) {
        perror("mkdir");
        return -1;
    }

    return 0;
}

int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

long get_file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
        return -1;
    }
    return (long)st.st_size;
}

int build_path(char *dest, size_t dest_size, const char *dir, const char *filename) {
    int written = snprintf(dest, dest_size, "%s/%s", dir, filename);
    if (written < 0 || (size_t)written >= dest_size) {
        return -1;
    }
    return 0;
}

int is_safe_filename(const char *filename) {
    if (filename == NULL || filename[0] == '\0') {
        return 0;
    }
    if (strstr(filename, "..") != NULL) {
        return 0;
    }
    if (strchr(filename, '/') != NULL || strchr(filename, '\\') != NULL) {
        return 0;
    }
    return 1;
}

int list_files_in_directory(const char *dir, char *output, size_t output_size) {
    DIR *directory = opendir(dir);
    if (directory == NULL) {
        snprintf(output, output_size, "Cannot open directory: %s\n", dir);
        return -1;
    }

    size_t used = 0;
    int count = 0;
    output[0] = '\0';

    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        char path[1024];
        if (build_path(path, sizeof(path), dir, entry->d_name) != 0) {
            continue;
        }

        struct stat st;
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
            continue;
        }

        char time_buf[64];
        struct tm *tm_info = localtime(&st.st_mtime);
        if (tm_info != NULL) {
            strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
        } else {
            snprintf(time_buf, sizeof(time_buf), "unknown-time");
        }

        int written = snprintf(output + used, output_size - used, "%s | %ld bytes | %s\n",
                               entry->d_name, (long)st.st_size, time_buf);
        if (written < 0 || (size_t)written >= output_size - used) {
            break;
        }

        used += (size_t)written;
        count++;
    }

    closedir(directory);

    if (count == 0) {
        snprintf(output, output_size, "No files are available on the server.\n");
    }

    return 0;
}

const char *basename_from_path(const char *path) {
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    const char *base = path;

    if (slash != NULL && slash + 1 > base) {
        base = slash + 1;
    }
    if (backslash != NULL && backslash + 1 > base) {
        base = backslash + 1;
    }

    return base;
}
