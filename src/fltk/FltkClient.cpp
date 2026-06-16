#include "FltkProtocol.hpp"

#include <FL/Fl.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Progress.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

namespace {

struct UiEvent {
    class ClientWindow *window;
    std::string text;
    int progress;
    bool done;
    bool list;
    std::vector<std::string> rows;
};

void awake_cb(void *data);

class ClientWindow {
public:
    ClientWindow() {
        fs::create_directories("downloads");
        fs::create_directories("client_files");

        window = new Fl_Window(1060, 700, "ShareNet Client - FLTK");
        window->color(fl_rgb_color(245, 247, 250));

        ip = new Fl_Input(90, 24, 170, 30, "Server");
        ip->value("127.0.0.1");
        port = new Fl_Input(320, 24, 90, 30, "Port");
        port->value("5000");
        list_btn = new Fl_Button(430, 24, 110, 30, "List");
        upload_path = new Fl_Input(120, 72, 650, 30, "Upload");
        upload_path->value(fs::absolute("client_files/sample.txt").string().c_str());
        browse_btn = new Fl_Button(790, 72, 90, 30, "Browse");
        upload_btn = new Fl_Button(900, 72, 120, 30, "Upload");
        download_dir = new Fl_Input(120, 120, 650, 30, "Downloads");
        download_dir->value(fs::absolute("downloads").string().c_str());
        folder_btn = new Fl_Button(790, 120, 90, 30, "Folder");
        download_btn = new Fl_Button(900, 120, 120, 30, "Download");

        files = new Fl_Browser(24, 176, 500, 390, "Server files");
        log_buffer = new Fl_Text_Buffer();
        log_view = new Fl_Text_Display(548, 176, 488, 390, "Activity");
        log_view->buffer(log_buffer);
        progress = new Fl_Progress(24, 608, 1012, 24);
        progress->minimum(0);
        progress->maximum(100);
        progress->value(0);
        progress->selection_color(fl_rgb_color(37, 99, 235));
        status = new Fl_Box(24, 584, 1012, 20, "Ready");
        status->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        list_btn->callback([](Fl_Widget *, void *p) { static_cast<ClientWindow *>(p)->list_files(); }, this);
        browse_btn->callback([](Fl_Widget *, void *p) { static_cast<ClientWindow *>(p)->choose_upload(); }, this);
        folder_btn->callback([](Fl_Widget *, void *p) { static_cast<ClientWindow *>(p)->choose_folder(); }, this);
        upload_btn->callback([](Fl_Widget *, void *p) { static_cast<ClientWindow *>(p)->upload_file(); }, this);
        download_btn->callback([](Fl_Widget *, void *p) { static_cast<ClientWindow *>(p)->download_selected(); }, this);

        load_config();
        window->resizable(log_view);
        window->end();
    }

    void show() { window->show(); }

    void post(std::string text, int pct = -1, bool done = false) {
        Fl::awake(awake_cb, new UiEvent{this, std::move(text), pct, done, false, {}});
    }

    void post_list(std::vector<std::string> rows) {
        Fl::awake(awake_cb, new UiEvent{this, "File list loaded", 100, true, true, std::move(rows)});
    }

    void apply(UiEvent *event) {
        if (event->progress >= 0) progress->value(event->progress);
        if (!event->text.empty()) {
            status->label(event->text.c_str());
            append_log(event->text);
        }
        if (event->list) {
            files->clear();
            for (const auto &row : event->rows) files->add(row.c_str());
        }
        if (event->done) set_busy(false);
        window->redraw();
    }

private:
    Fl_Window *window{};
    Fl_Input *ip{};
    Fl_Input *port{};
    Fl_Input *upload_path{};
    Fl_Input *download_dir{};
    Fl_Button *list_btn{};
    Fl_Button *browse_btn{};
    Fl_Button *folder_btn{};
    Fl_Button *upload_btn{};
    Fl_Button *download_btn{};
    Fl_Browser *files{};
    Fl_Text_Buffer *log_buffer{};
    Fl_Text_Display *log_view{};
    Fl_Progress *progress{};
    Fl_Box *status{};
    std::atomic_bool busy{false};

    int server_port() const { return std::atoi(port->value()); }

    void set_busy(bool value) {
        busy = value;
        list_btn->deactivate();
        upload_btn->deactivate();
        download_btn->deactivate();
        browse_btn->deactivate();
        folder_btn->deactivate();
        if (!value) {
            list_btn->activate();
            upload_btn->activate();
            download_btn->activate();
            browse_btn->activate();
            folder_btn->activate();
        }
    }

    void append_log(const std::string &text) {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        char time_buf[32]{};
        std::strftime(time_buf, sizeof(time_buf), "%H:%M:%S", std::localtime(&now));
        log_buffer->append((std::string(time_buf) + "  " + text + "\n").c_str());
        log_view->scroll(log_buffer->count_lines(0, log_buffer->length()), 0);
    }

    void choose_upload() {
        const char *path = fl_file_chooser("Select file to upload", "*", upload_path->value());
        if (path) {
            upload_path->value(fs::absolute(path).string().c_str());
            save_config();
        }
    }

    void choose_folder() {
        const char *path = fl_dir_chooser("Select download folder", download_dir->value());
        if (path) {
            fs::create_directories(path);
            download_dir->value(fs::absolute(path).string().c_str());
            save_config();
        }
    }

    void list_files() {
        if (busy.exchange(true)) return;
        save_config();
        progress->value(0);
        std::string host = ip->value();
        int p = server_port();
        std::thread([this, host, p] {
            int fd = connect_to_server_timeout(host.c_str(), p, 5000);
            if (fd < 0) { post("Could not connect to server", 0, true); return; }
            if (send_message_header(fd, MSG_REQUEST_LIST, 0) != 0) {
                sharenet::close_if_valid(fd); post("Could not send list request", 0, true); return;
            }
            MessageHeader header{};
            if (recv_message_header(fd, &header) != 0 || header.payload_size >= MAX_FILE_LIST_SIZE) {
                sharenet::close_if_valid(fd); post("Invalid list response", 0, true); return;
            }
            std::string text = sharenet::receive_string_payload(fd, header.payload_size);
            sharenet::close_if_valid(fd);
            std::vector<std::string> rows;
            std::istringstream in(text);
            for (std::string line; std::getline(in, line);) {
                if (!line.empty() && line.rfind("No files", 0) != 0) rows.push_back(line);
            }
            post_list(rows);
        }).detach();
    }

    void upload_file() {
        if (busy.exchange(true)) return;
        save_config();
        progress->value(0);
        std::string host = ip->value();
        int p = server_port();
        std::string path = upload_path->value();
        std::thread([this, host, p, path] {
            std::string name = sharenet::base_name(path);
            if (!sharenet::safe_name(name) || !fs::is_regular_file(path)) {
                post("Choose a valid upload file", 0, true); return;
            }
            auto size = static_cast<std::uint64_t>(fs::file_size(path));
            int fd = connect_to_server_timeout(host.c_str(), p, 5000);
            if (fd < 0) { post("Could not connect to server", 0, true); return; }
            if (!sharenet::send_upload_metadata(fd, name, size)) {
                sharenet::close_if_valid(fd); post("Could not send upload metadata", 0, true); return;
            }
            FILE *file = std::fopen(path.c_str(), "rb");
            if (!file) { sharenet::close_if_valid(fd); post("Could not open upload file", 0, true); return; }
            std::uint32_t total = calculate_total_chunks(static_cast<long>(size), CHUNK_SIZE);
            unsigned char buffer[CHUNK_SIZE];
            for (std::uint32_t i = 0; i < total; ++i) {
                size_t got = std::fread(buffer, 1, sizeof(buffer), file);
                ChunkHeader ch{hton32(i), hton32(total), hton32(static_cast<std::uint32_t>(got))};
                if (send_message_header(fd, MSG_CHUNK_DATA, sizeof(ChunkHeader) + static_cast<std::uint32_t>(got)) != 0 ||
                    send_all(fd, &ch, sizeof(ch)) != 0 || send_all(fd, buffer, got) != 0) {
                    std::fclose(file); sharenet::close_if_valid(fd); post("Upload failed", 0, true); return;
                }
                post("Uploading " + name, static_cast<int>(((i + 1) * 100) / (total == 0 ? 1 : total)), false);
            }
            std::fclose(file);
            send_message_header(fd, MSG_DONE_TRANSFER, 0);
            char response[BUFFER_SIZE]{};
            int type = recv_string_payload(fd, response, sizeof(response));
            sharenet::close_if_valid(fd);
            post(type == MSG_UPLOAD_RESPONSE ? response : "Upload finished with server error", 100, true);
        }).detach();
    }

    void download_selected() {
        if (busy.exchange(true)) return;
        save_config();
        progress->value(0);
        int idx = files->value();
        if (idx <= 0) { set_busy(false); fl_alert("Select a server file first."); return; }
        std::string row = files->text(idx);
        std::string name = row.substr(0, row.find(" | "));
        std::string host = ip->value();
        int p = server_port();
        std::string out_dir = download_dir->value();
        std::thread([this, host, p, name, out_dir] {
            fs::create_directories(out_dir);
            std::string out = (fs::path(out_dir) / name).string();
            int fd = connect_to_server_timeout(host.c_str(), p, 5000);
            if (fd < 0) { post("Could not connect to server", 0, true); return; }
            if (send_string_payload(fd, MSG_DOWNLOAD_REQUEST, name.c_str()) != 0) {
                sharenet::close_if_valid(fd); post("Could not request download", 0, true); return;
            }
            FILE *file = std::fopen(out.c_str(), "wb");
            if (!file) { sharenet::close_if_valid(fd); post("Could not create output file", 0, true); return; }
            std::uint32_t expected = 0;
            while (true) {
                MessageHeader header{};
                if (recv_message_header(fd, &header) != 0) { std::fclose(file); sharenet::close_if_valid(fd); post("Download failed", 0, true); return; }
                if (header.type == MSG_DONE_TRANSFER) break;
                if (header.type == MSG_ERROR) {
                    std::string err = sharenet::receive_string_payload(fd, header.payload_size);
                    std::fclose(file); sharenet::close_if_valid(fd); post(err, 0, true); return;
                }
                ChunkHeader ch{};
                if (header.type != MSG_CHUNK_DATA || recv_all(fd, &ch, sizeof(ch)) != 0) {
                    std::fclose(file); sharenet::close_if_valid(fd); post("Invalid download packet", 0, true); return;
                }
                ch.chunk_index = ntoh32(ch.chunk_index);
                ch.total_chunks = ntoh32(ch.total_chunks);
                ch.chunk_size = ntoh32(ch.chunk_size);
                std::vector<unsigned char> buf(ch.chunk_size);
                if (ch.chunk_size > 0 && recv_all(fd, buf.data(), ch.chunk_size) != 0) {
                    std::fclose(file); sharenet::close_if_valid(fd); post("Download failed", 0, true); return;
                }
                if (!buf.empty()) std::fwrite(buf.data(), 1, buf.size(), file);
                expected = ch.total_chunks;
                post("Downloading " + name, expected ? static_cast<int>(((ch.chunk_index + 1) * 100) / expected) : 100, false);
            }
            std::fclose(file);
            sharenet::close_if_valid(fd);
            post("Downloaded to " + fs::absolute(out).string(), 100, true);
        }).detach();
    }

    void save_config() {
        std::ofstream out(".sharenet_fltk_client.ini");
        out << ip->value() << "\n" << port->value() << "\n" << upload_path->value() << "\n" << download_dir->value() << "\n";
    }

    void load_config() {
        std::ifstream in(".sharenet_fltk_client.ini");
        std::string a, b, c, d;
        if (std::getline(in, a) && std::getline(in, b) && std::getline(in, c) && std::getline(in, d)) {
            ip->value(a.c_str());
            port->value(b.c_str());
            upload_path->value(c.c_str());
            download_dir->value(d.c_str());
        }
    }
};

void awake_cb(void *data) {
    auto *event = static_cast<UiEvent *>(data);
    event->window->apply(event);
    delete event;
}

} // namespace

int main(int argc, char **argv) {
    init_socket_system();
    Fl::lock();
    ClientWindow app;
    app.show();
    int rc = Fl::run();
    cleanup_socket_system();
    return rc;
}
