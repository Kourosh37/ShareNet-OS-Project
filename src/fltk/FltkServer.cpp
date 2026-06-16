#include "FltkProtocol.hpp"

#include <FL/Fl.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct ServerEvent {
    class ServerWindow *window;
    std::string text;
    bool refresh;
};

void server_awake_cb(void *data);

class ServerWindow {
public:
    ServerWindow() {
        fs::create_directories("server_files");
        window = new Fl_Window(1000, 680, "ShareNet Server - FLTK");
        window->color(fl_rgb_color(245, 247, 250));

        bind_ip = new Fl_Input(88, 24, 170, 30, "Bind IP");
        bind_ip->value("0.0.0.0");
        port = new Fl_Input(318, 24, 90, 30, "Port");
        port->value("5000");
        start_btn = new Fl_Button(430, 24, 110, 30, "Start");
        refresh_btn = new Fl_Button(560, 24, 110, 30, "Refresh");
        delete_btn = new Fl_Button(690, 24, 120, 30, "Delete");
        copy_btn = new Fl_Button(830, 24, 130, 30, "Copy");
        files = new Fl_Browser(24, 88, 470, 500, "Server files");
        log_buffer = new Fl_Text_Buffer();
        log_view = new Fl_Text_Display(520, 88, 456, 500, "Activity");
        log_view->buffer(log_buffer);
        status = new Fl_Box(24, 620, 952, 24, "Stopped");
        status->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        start_btn->callback([](Fl_Widget *, void *p) { static_cast<ServerWindow *>(p)->toggle(); }, this);
        refresh_btn->callback([](Fl_Widget *, void *p) { static_cast<ServerWindow *>(p)->refresh_files(); }, this);
        delete_btn->callback([](Fl_Widget *, void *p) { static_cast<ServerWindow *>(p)->delete_selected(); }, this);
        copy_btn->callback([](Fl_Widget *, void *p) { static_cast<ServerWindow *>(p)->copy_selected(); }, this);

        refresh_files();
        window->resizable(log_view);
        window->end();
    }

    ~ServerWindow() { stop(); }
    void show() { window->show(); }

    void post(std::string text, bool refresh = false) {
        Fl::awake(server_awake_cb, new ServerEvent{this, std::move(text), refresh});
    }

    void apply(ServerEvent *event) {
        if (!event->text.empty()) append_log(event->text);
        if (event->refresh) refresh_files();
    }

private:
    Fl_Window *window{};
    Fl_Input *bind_ip{};
    Fl_Input *port{};
    Fl_Button *start_btn{};
    Fl_Button *refresh_btn{};
    Fl_Button *delete_btn{};
    Fl_Button *copy_btn{};
    Fl_Browser *files{};
    Fl_Text_Buffer *log_buffer{};
    Fl_Text_Display *log_view{};
    Fl_Box *status{};
    std::atomic_bool running{false};
    int server_fd{-1};

    void append_log(const std::string &text) {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        char time_buf[32]{};
        std::strftime(time_buf, sizeof(time_buf), "%H:%M:%S", std::localtime(&now));
        log_buffer->append((std::string(time_buf) + "  " + text + "\n").c_str());
        log_view->scroll(log_buffer->count_lines(0, log_buffer->length()), 0);
    }

    void refresh_files() {
        files->clear();
        for (const auto &entry : fs::directory_iterator("server_files")) {
            if (entry.is_regular_file()) {
                files->add((entry.path().filename().string() + " | " + std::to_string(entry.file_size()) + " bytes").c_str());
            }
        }
    }

    std::string selected_name() {
        int idx = files->value();
        if (idx <= 0) return {};
        std::string row = files->text(idx);
        return row.substr(0, row.find(" | "));
    }

    void delete_selected() {
        std::string name = selected_name();
        if (name.empty()) return;
        if (fl_choice(("Delete " + name + "?").c_str(), "Cancel", "Delete", nullptr) == 1) {
            fs::remove(fs::path("server_files") / name);
            refresh_files();
            append_log("Deleted " + name);
        }
    }

    void copy_selected() {
        std::string name = selected_name();
        if (name.empty()) return;
        const char *dst = fl_file_chooser("Copy server file to", "*", name.c_str());
        if (!dst) return;
        fs::copy_file(fs::path("server_files") / name, dst, fs::copy_options::overwrite_existing);
        append_log("Copied " + name);
    }

    void toggle() {
        if (running) {
            stop();
            return;
        }
        server_fd = create_server_socket_at(bind_ip->value(), std::atoi(port->value()));
        if (server_fd < 0) {
            append_log("Listen failed");
            return;
        }
        running = true;
        start_btn->label("Stop");
        status->label(("Listening on " + std::string(bind_ip->value()) + ":" + port->value()).c_str());
        append_log(status->label());
        std::thread([this] { accept_loop(); }).detach();
    }

    void stop() {
        running = false;
        if (server_fd >= 0) {
            close_socket(server_fd);
            server_fd = -1;
        }
        start_btn->label("Start");
        status->label("Stopped");
        append_log("Server stopped");
    }

    void accept_loop() {
        while (running) {
            int client = accept_client(server_fd);
            if (client >= 0) {
                std::thread([this, client] { handle_client_socket(client); }).detach();
            }
        }
    }

    std::string file_list_text() {
        char out[MAX_FILE_LIST_SIZE];
        list_files_in_directory("server_files", out, sizeof(out));
        return out;
    }

    void handle_client_socket(int client) {
        MessageHeader header{};
        if (recv_message_header(client, &header) != 0) { sharenet::close_if_valid(client); return; }
        if (header.type == MSG_REQUEST_LIST) {
            send_string_payload(client, MSG_LIST_RESPONSE, file_list_text().c_str());
        } else if (header.type == MSG_DOWNLOAD_REQUEST) {
            std::string name = sharenet::receive_string_payload(client, header.payload_size);
            if (!sharenet::safe_name(name)) send_error_message(client, "Unsafe filename rejected");
            else {
                auto path = (fs::path("server_files") / name).string();
                if (!fs::is_regular_file(path) || send_file_chunks(client, path.c_str()) != 0) {
                    send_error_message(client, "Download failed");
                } else {
                    post("Sent " + name);
                }
            }
        } else if (header.type == MSG_REQUEST_UPLOAD) {
            handle_upload(client, header);
        } else {
            send_error_message(client, "Unknown request type");
        }
        sharenet::close_if_valid(client);
    }

    void handle_upload(int client, const MessageHeader &header) {
        if (header.payload_size < sizeof(std::uint32_t) + sizeof(std::uint64_t)) {
            send_error_message(client, "Invalid upload metadata");
            return;
        }
        std::uint32_t net_len{};
        std::uint64_t net_size{};
        if (recv_all(client, &net_len, sizeof(net_len)) != 0 || recv_all(client, &net_size, sizeof(net_size)) != 0) return;
        std::uint32_t len = ntoh32(net_len);
        std::uint64_t size = ntohll(net_size);
        std::string name(len, '\0');
        if (len == 0 || len >= MAX_FILENAME || recv_all(client, name.data(), len) != 0 || !sharenet::safe_name(name)) {
            send_error_message(client, "Invalid filename");
            return;
        }
        std::uint32_t expected = calculate_total_chunks(static_cast<long>(size), CHUNK_SIZE);
        auto path = (fs::path("server_files") / name).string();
        if (receive_file_chunks(client, path.c_str(), expected) != 0) {
            send_error_message(client, "Upload failed");
            return;
        }
        send_string_payload(client, MSG_UPLOAD_RESPONSE, "Upload completed successfully");
        post("Received " + name, true);
    }
};

void server_awake_cb(void *data) {
    auto *event = static_cast<ServerEvent *>(data);
    event->window->apply(event);
    delete event;
}

} // namespace

int main(int argc, char **argv) {
    init_socket_system();
    Fl::lock();
    ServerWindow app;
    app.show();
    int rc = Fl::run();
    cleanup_socket_system();
    return rc;
}
