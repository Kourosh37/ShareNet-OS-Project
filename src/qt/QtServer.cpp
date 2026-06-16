#include "Protocol.h"

#include <QtWidgets>

class ServerWindow : public QWidget {
public:
    ServerWindow() {
        setWindowTitle("ShareNet Server - Qt");
        resize(1040, 720);
        setMinimumSize(920, 620);
        setStyleSheet(R"(
            QWidget { background:#0d121b; color:#eef4ff; font-family:'Segoe UI','Inter','Arial'; }
            QLineEdit { background:#1a2433; border:1px solid #4d5d73; border-radius:8px; padding:9px; color:white; }
            QPushButton { background:#2f73df; color:white; border:0; border-radius:9px; padding:10px 14px; font-weight:600; }
            QPushButton:hover { background:#4b8cf1; }
            QPushButton#Danger { background:#bd4154; }
            QTableWidget, QTextEdit { background:#161f2d; border:1px solid #344256; border-radius:10px; gridline-color:#344256; }
            QLabel#Title { font-size:34px; font-weight:700; }
            QLabel#Status { color:#b8c7da; }
        )");

        bindIp = new QLineEdit("0.0.0.0");
        portEdit = new QLineEdit("5000");
        status = new QLabel("Stopped");
        status->setObjectName("Status");
        table = new QTableWidget(0, 3);
        table->setHorizontalHeaderLabels({"Name", "Size", "Modified"});
        table->horizontalHeader()->setStretchLastSection(true);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        logBox = new QTextEdit;
        logBox->setReadOnly(true);
        server = new QTcpServer(this);

        auto *title = new QLabel("ShareNet Server");
        title->setObjectName("Title");
        auto *sub = new QLabel("Qt server with local file delete/download controls and protocol-compatible TCP service.");
        sub->setObjectName("Status");
        startBtn = new QPushButton("Start");
        auto *refreshBtn = new QPushButton("Refresh Files");
        auto *deleteBtn = new QPushButton("Delete Selected");
        deleteBtn->setObjectName("Danger");
        auto *downloadBtn = new QPushButton("Download/Copy Selected");

        connect(startBtn, &QPushButton::clicked, this, [this] { toggleServer(); });
        connect(refreshBtn, &QPushButton::clicked, this, [this] { refreshFiles(); });
        connect(deleteBtn, &QPushButton::clicked, this, [this] { deleteSelected(); });
        connect(downloadBtn, &QPushButton::clicked, this, [this] { copySelected(); });
        connect(server, &QTcpServer::newConnection, this, [this] { acceptClient(); });

        auto *top = new QGridLayout;
        top->addWidget(new QLabel("Bind IP"), 0, 0);
        top->addWidget(bindIp, 1, 0);
        top->addWidget(new QLabel("Port"), 0, 1);
        top->addWidget(portEdit, 1, 1);
        top->addWidget(startBtn, 1, 2);
        top->addWidget(status, 1, 3);

        auto *actions = new QHBoxLayout;
        actions->addWidget(refreshBtn);
        actions->addWidget(deleteBtn);
        actions->addWidget(downloadBtn);
        actions->addStretch();

        auto *split = new QSplitter;
        split->addWidget(table);
        split->addWidget(logBox);
        split->setStretchFactor(0, 2);
        split->setStretchFactor(1, 1);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(28, 24, 28, 24);
        layout->setSpacing(16);
        layout->addWidget(title);
        layout->addWidget(sub);
        layout->addLayout(top);
        layout->addLayout(actions);
        layout->addWidget(split, 1);
        QDir().mkpath("server_files");
        refreshFiles();
    }

private:
    struct Context {
        QByteArray rx;
        QFile file;
        QString uploadName;
        quint32 uploadExpected = 0;
        quint32 uploadIndex = 0;
    };

    QLineEdit *bindIp{};
    QLineEdit *portEdit{};
    QLabel *status{};
    QPushButton *startBtn{};
    QTableWidget *table{};
    QTextEdit *logBox{};
    QTcpServer *server{};
    QHash<QTcpSocket *, Context *> contexts;

    void log(const QString &text) {
        logBox->append(QTime::currentTime().toString("HH:mm:ss") + "  " + text);
    }

    QString selectedName() const {
        int row = table->currentRow();
        if (row < 0) return {};
        return table->item(row, 0)->text();
    }

    void refreshFiles() {
        QDir dir("server_files");
        dir.mkpath(".");
        QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
        table->setRowCount(files.size());
        for (int i = 0; i < files.size(); ++i) {
            table->setItem(i, 0, new QTableWidgetItem(files[i].fileName()));
            table->setItem(i, 1, new QTableWidgetItem(QString::number(files[i].size()) + " bytes"));
            table->setItem(i, 2, new QTableWidgetItem(files[i].lastModified().toString("yyyy-MM-dd HH:mm:ss")));
        }
        log(QString("File list refreshed: %1 files").arg(files.size()));
    }

    void deleteSelected() {
        QString name = selectedName();
        if (name.isEmpty()) return;
        if (QMessageBox::question(this, "Delete file", "Delete " + name + "?") != QMessageBox::Yes) return;
        QFile::remove(QDir("server_files").absoluteFilePath(name));
        refreshFiles();
        log("Deleted " + name);
    }

    void copySelected() {
        QString name = selectedName();
        if (name.isEmpty()) return;
        QString src = QDir("server_files").absoluteFilePath(name);
        QString dst = QFileDialog::getSaveFileName(this, "Copy server file", name);
        if (dst.isEmpty()) return;
        QFile::remove(dst);
        if (QFile::copy(src, dst)) log("Copied " + name + " to " + dst);
        else log("Copy failed for " + name);
    }

    void toggleServer() {
        if (server->isListening()) {
            server->close();
            startBtn->setText("Start");
            status->setText("Stopped");
            log("Server stopped");
            return;
        }
        QHostAddress address(bindIp->text().isEmpty() ? "0.0.0.0" : bindIp->text());
        if (!server->listen(address, portEdit->text().toUShort())) {
            status->setText("Failed: " + server->errorString());
            log("Listen failed: " + server->errorString());
            return;
        }
        startBtn->setText("Stop");
        status->setText(QString("Listening on %1:%2").arg(bindIp->text(), portEdit->text()));
        log(status->text());
    }

    void acceptClient() {
        while (server->hasPendingConnections()) {
            QTcpSocket *socket = server->nextPendingConnection();
            auto *ctx = new Context;
            contexts.insert(socket, ctx);
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] { consume(socket); });
            connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
                delete contexts.take(socket);
                socket->deleteLater();
            });
            log("Client connected");
        }
    }

    QString fileListText() const {
        QString out;
        QDir dir("server_files");
        QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
        if (files.isEmpty()) return "No files are available on the server.\n";
        for (const QFileInfo &info : files) {
            out += QString("%1 | %2 bytes | %3\n").arg(info.fileName()).arg(info.size())
                       .arg(info.lastModified().toString("yyyy-MM-dd HH:mm:ss"));
        }
        return out;
    }

    void sendError(QTcpSocket *socket, const QString &msg) {
        socket->write(stringMessage(MSG_ERROR, msg));
    }

    void consume(QTcpSocket *socket) {
        Context *ctx = contexts.value(socket);
        if (!ctx) return;
        ctx->rx += socket->readAll();
        while (ctx->rx.size() >= 8) {
            quint32 type = readU32(ctx->rx.constData());
            quint32 size = readU32(ctx->rx.constData() + 4);
            if (ctx->rx.size() < int(8 + size)) return;
            QByteArray payload = ctx->rx.mid(8, size);
            ctx->rx.remove(0, 8 + size);
            handle(socket, ctx, type, payload);
        }
    }

    void handle(QTcpSocket *socket, Context *ctx, quint32 type, const QByteArray &payload) {
        if (type == MSG_REQUEST_LIST) {
            socket->write(stringMessage(MSG_LIST_RESPONSE, fileListText()));
            socket->disconnectFromHost();
            return;
        }
        if (type == MSG_DOWNLOAD_REQUEST) {
            QString name = QString::fromUtf8(payload);
            if (!isSafeFileName(name)) { sendError(socket, "Unsafe filename rejected"); return; }
            sendFile(socket, name);
            return;
        }
        if (type == MSG_REQUEST_UPLOAD) {
            if (payload.size() < 12) { sendError(socket, "Invalid upload metadata"); return; }
            quint32 nameLen = readU32(payload.constData());
            if (payload.size() != int(12 + nameLen)) { sendError(socket, "Invalid upload metadata"); return; }
            QString name = QString::fromUtf8(payload.mid(12, nameLen));
            quint64 size = readU64(payload.constData() + 4);
            if (!isSafeFileName(name)) { sendError(socket, "Unsafe filename rejected"); return; }
            ctx->uploadName = name;
            ctx->uploadExpected = totalChunks(qint64(size));
            ctx->uploadIndex = 0;
            ctx->file.setFileName(QDir("server_files").absoluteFilePath(name));
            if (!ctx->file.open(QIODevice::WriteOnly)) { sendError(socket, "Could not create server file"); return; }
            log("Receiving " + name);
            return;
        }
        if (type == MSG_CHUNK_DATA) {
            if (!ctx->file.isOpen() || payload.size() < 12) { sendError(socket, "Unexpected chunk"); return; }
            quint32 index = readU32(payload.constData());
            quint32 total = readU32(payload.constData() + 4);
            quint32 chunkSize = readU32(payload.constData() + 8);
            if (index != ctx->uploadIndex || total != ctx->uploadExpected || payload.size() != int(12 + chunkSize)) {
                sendError(socket, "Invalid chunk order");
                return;
            }
            ctx->file.write(payload.constData() + 12, chunkSize);
            ctx->uploadIndex++;
            return;
        }
        if (type == MSG_DONE_TRANSFER) {
            if (ctx->file.isOpen()) ctx->file.close();
            socket->write(stringMessage(MSG_UPLOAD_RESPONSE, "Upload completed successfully"));
            socket->disconnectFromHost();
            refreshFiles();
            log("Upload complete: " + ctx->uploadName);
            return;
        }
        sendError(socket, "Unknown request type");
    }

    void sendFile(QTcpSocket *socket, const QString &name) {
        QFile file(QDir("server_files").absoluteFilePath(name));
        if (!file.open(QIODevice::ReadOnly)) { sendError(socket, "File not found on server"); return; }
        quint32 total = totalChunks(file.size());
        for (quint32 i = 0; i < total; ++i) {
            QByteArray chunk = file.read(QtChunkSize);
            socket->write(messageHeader(MSG_CHUNK_DATA, 12 + quint32(chunk.size())) +
                          chunkHeader(i, total, quint32(chunk.size())) + chunk);
        }
        socket->write(messageHeader(MSG_DONE_TRANSFER, 0));
        socket->disconnectFromHost();
        log("Sent " + name);
    }
};

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    ServerWindow window;
    window.show();
    return app.exec();
}
