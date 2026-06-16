#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

extern "C" {
#include "chunk.h"
#include "config.h"
#include "file_utils.h"
#include "protocol.h"
#include "socket_utils.h"
}

namespace sharenet {

inline std::string base_name(const std::string &path) {
    return std::filesystem::path(path).filename().string();
}

inline bool safe_name(const std::string &name) {
    return is_safe_filename(name.c_str()) != 0;
}

inline bool send_upload_metadata(int socket_fd, const std::string &filename, std::uint64_t file_size) {
    std::uint32_t filename_length = static_cast<std::uint32_t>(filename.size());
    std::uint32_t payload_size = sizeof(std::uint32_t) + sizeof(std::uint64_t) + filename_length;
    std::uint32_t net_filename_length = hton32(filename_length);
    std::uint64_t net_file_size = htonll(file_size);

    return send_message_header(socket_fd, MSG_REQUEST_UPLOAD, payload_size) == 0 &&
           send_all(socket_fd, &net_filename_length, sizeof(net_filename_length)) == 0 &&
           send_all(socket_fd, &net_file_size, sizeof(net_file_size)) == 0 &&
           send_all(socket_fd, filename.data(), filename.size()) == 0;
}

inline std::string receive_string_payload(int socket_fd, std::uint32_t size) {
    std::string text(size, '\0');
    if (size > 0 && recv_all(socket_fd, text.data(), size) != 0) {
        return {};
    }
    return text;
}

inline void close_if_valid(int socket_fd) {
    if (socket_fd >= 0) {
        close_socket(socket_fd);
    }
}

} // namespace sharenet
