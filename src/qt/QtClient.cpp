#include "Protocol.h"

#include <QtWidgets>

class ClientWindow : public QWidget {
public:
    ClientWindow() {
        setWindowTitle("ShareNet Client - Qt");
        resize(1120, 760);
        setMinimumSize(980, 680);
        setStyleSheet(R"(
            QWidget { background:#0d121b; color:#eef4ff; font-family:'Segoe UI','Inter','Arial'; }
            QLineEdit { background:#1a2433; border:1px solid #4d5d73; border-radius:8px; padding:9px; color:#ffffff; }
            QPushButton { background:#2f73df; color:white; border:0; border-radius:9px; padding:10px 14px; font-weight:600; }
            QPushButton:hover { background:#4b8cf1; }
            QPushButton:disabled { background:#434b58; color:#9ca8b8; }
            QListWidget, QTextEdit { background:#161f2d; border:1px solid #344256; border-radius:10px; padding:8px; }
            QListWidget::item { padding:10px; border-radius:7px; }
            QListWidget::item:selected { background:#315d99; }
            QProgressBar { background:#1a2433; border:1px solid #344256; border-radius:9px; text-align:center; height:20px; }
            QProgressBar::chunk { background:#5fa8ff; border-radius:8px; }
            QLabel#Title { font-size:34px; font-weight:700; }
            QLabel#Status { color:#b8c7da; }
            QLabel#Check { color:#44d68c; font-size:48px; font-weight:800; }
        )");

        serverIp = new QLineEdit("127.0.0.1");
        serverPort = new QLineEdit("5000");
        uploadPath = new QLineEdit(QFileInfo("client_files/sample.txt").absoluteFilePath());
        downloadDir = new QLineEdit(QFileInfo("downloads").absoluteFilePath());
        fileList = new QListWidget;
        activity = new QTextEdit;
        activity->setReadOnly(true);
        progress = new QProgressBar;
        progress->setRange(0, 100);
        progress->setValue(0);
        status = new QLabel("Ready");
        status->setObjectName("Status");
        check = new QLabel("✓");
        check->setObjectName("Check");
        check->setAlignment(Qt::AlignCenter);
        check->setVisible(false);
        effect = new QGraphicsOpacityEffect(check);
        check->setGraphicsEffect(effect);

        auto *title = new QLabel("ShareNet Client");
        title->setObjectName("Title");
        auto *sub = new QLabel("Select a local file, list server files, then download selected items with visible progress.");
        sub->setObjectName("Status");

        auto *listBtn = new QPushButton("List Files");
        auto *browseBtn = new QPushButton("Browse Upload File");
        auto *folderBtn = new QPushButton("Choose Download Folder");
        uploadBtn = new QPushButton("Upload");
        downloadBtn = new QPushButton("Download Selected");

        connect(listBtn, &QPushButton::clicked, this, [this] { listFiles(); });
        connect(browseBtn, &QPushButton::clicked, this, [this] { chooseUpload(); });
        connect(folderBtn, &QPushButton::clicked, this, [this] { chooseFolder(); });
        connect(uploadBtn, &QPushButton::clicked, this, [this] { uploadFile(); });
        connect(downloadBtn, &QPushButton::clicked, this, [this] { downloadSelected(); });

        auto *grid = new QGridLayout;
        grid->addWidget(new QLabel("Server IP"), 0, 0);
        grid->addWidget(serverIp, 1, 0);
        grid->addWidget(new QLabel("Port"), 0, 1);
        grid->addWidget(serverPort, 1, 1);
        grid->addWidget(listBtn, 1, 2);
        grid->addWidget(new QLabel("Upload file (absolute path)"), 2, 0, 1, 3);
        grid->addWidget(uploadPath, 3, 0, 1, 2);
        grid->addWidget(browseBtn, 3, 2);
        grid->addWidget(uploadBtn, 3, 3);
        grid->addWidget(new QLabel("Download folder (absolute path)"), 4, 0, 1, 3);
        grid->addWidget(downloadDir, 5, 0, 1, 2);
        grid->addWidget(folderBtn, 5, 2);
        grid->addWidget(downloadBtn, 5, 3);

        auto *split = new QSplitter;
        split->addWidget(fileList);
        split->addWidget(activity);
        split->setStretchFactor(0, 1);
        split->setStretchFactor(1, 1);

        auto *bottom = new QHBoxLayout;
        bottom->addWidget(progress, 1);
        bottom->addWidget(check);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(28, 24, 28, 24);
        layout->setSpacing(16);
        layout->addWidget(title);
        layout->addWidget(sub);
        layout->addLayout(grid);
        layout->addWidget(split, 1);
        layout->addWidget(status);
        layout->addLayout(bottom);
        loadConfig();
    }

private:
    QLineEdit *serverIp{};
    QLineEdit *serverPort{};
    QLineEdit *uploadPath{};
    QLineEdit *downloadDir{};
    QListWidget *fileList{};
    QTextEdit *activity{};
    QProgressBar *progress{};
    QLabel *status{};
    QLabel *check{};
    QGraphicsOpacityEffect *effect{};
    QPushButton *uploadBtn{};
    QPushButton *downloadBtn{};
    QTcpSocket *socket{};
    QFile transferFile;
    QByteArray rx;
    QString downloadName;
    quint32 expectedChunk = 0;
    quint32 expectedTotal = 0;
    qint64 totalBytes = 0;
    qint64 transferredBytes = 0;

    void log(const QString &text) {
        activity->append(QTime::currentTime().toString("HH:mm:ss") + "  " + text);
    }

    quint16 port() const { return serverPort->text().toUShort(); }

    void setBusy(const QString &text, bool busy) {
        status->setText(text);
        uploadBtn->setDisabled(busy);
        downloadBtn->setDisabled(busy);
        if (busy) {
            check->setVisible(false);
            progress->setRange(0, 0);
        } else {
            progress->setRange(0, 100);
        }
    }

    void showDone(const QString &text) {
        setBusy(text, false);
        progress->setValue(100);
        check->setVisible(true);
        auto *anim = new QPropertyAnimation(effect, "opacity", this);
        anim->setDuration(650);
        anim->setStartValue(0.0);
        anim->setKeyValueAt(0.45, 1.0);
        anim->setEndValue(1.0);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }

    QTcpSocket *newSocket() {
        if (socket) socket->deleteLater();
        socket = new QTcpSocket(this);
        connect(socket, &QTcpSocket::errorOccurred, this, [this] {
            setBusy("Socket error: " + socket->errorString(), false);
            log("Error: " + socket->errorString());
        });
        socket->connectToHost(serverIp->text(), port());
        return socket;
    }

    void chooseUpload() {
        QString path = QFileDialog::getOpenFileName(this, "Select file to upload");
        if (!path.isEmpty()) {
            uploadPath->setText(QFileInfo(path).absoluteFilePath());
            saveConfig();
        }
    }

    void chooseFolder() {
        QString path = QFileDialog::getExistingDirectory(this, "Select download folder", downloadDir->text());
        if (!path.isEmpty()) {
            downloadDir->setText(QFileInfo(path).absoluteFilePath());
            QDir().mkpath(downloadDir->text());
            saveConfig();
        }
    }

    void listFiles() {
        saveConfig();
        rx.clear();
        setBusy("Loading server file list...", true);
        QTcpSocket *s = newSocket();
        connect(s, &QTcpSocket::connected, this, [s] { s->write(messageHeader(MSG_REQUEST_LIST, 0)); });
        connect(s, &QTcpSocket::readyRead, this, [this, s] {
            rx += s->readAll();
            if (rx.size() < 8) return;
            quint32 type = readU32(rx.constData());
            quint32 size = readU32(rx.constData() + 4);
            if (rx.size() < int(8 + size)) return;
            QString text = QString::fromUtf8(rx.mid(8, size));
            fileList->clear();
            if (type == MSG_LIST_RESPONSE) {
                for (const QString &line : text.split('\n', Qt::SkipEmptyParts)) {
                    QString name = line.section(" | ", 0, 0);
                    if (!name.isEmpty() && !line.startsWith("No files")) {
                        auto *item = new QListWidgetItem(line);
                        item->setData(Qt::UserRole, name);
                        fileList->addItem(item);
                    }
                }
                showDone(QString("Loaded %1 files").arg(fileList->count()));
            } else {
                setBusy(text, false);
            }
            log(text.trimmed());
            s->disconnectFromHost();
        });
    }

    void uploadFile() {
        QFileInfo info(uploadPath->text());
        if (!info.exists() || !info.isFile()) {
            setBusy("Choose a valid upload file first.", false);
            return;
        }
        QString filename = info.fileName();
        if (!isSafeFileName(filename)) {
            setBusy("Unsafe filename rejected.", false);
            return;
        }

        saveConfig();
        transferFile.setFileName(info.absoluteFilePath());
        if (!transferFile.open(QIODevice::ReadOnly)) {
            setBusy("Could not open file.", false);
            return;
        }

        totalBytes = transferFile.size();
        transferredBytes = 0;
        progress->setRange(0, 100);
        progress->setValue(0);
        setBusy("Uploading " + filename + "...", true);

        QByteArray name = filename.toUtf8();
        QByteArray payload = packU32(quint32(name.size())) + packU64(quint64(totalBytes)) + name;
        QTcpSocket *s = newSocket();
        connect(s, &QTcpSocket::connected, this, [this, s, payload] {
            s->write(messageHeader(MSG_REQUEST_UPLOAD, quint32(payload.size())) + payload);
            sendNextUploadChunk(s, 0, totalChunks(totalBytes));
        });
        connect(s, &QTcpSocket::readyRead, this, [this, s] {
            rx += s->readAll();
            if (rx.size() < 8) return;
            quint32 type = readU32(rx.constData());
            quint32 size = readU32(rx.constData() + 4);
            if (rx.size() < int(8 + size)) return;
            QString response = QString::fromUtf8(rx.mid(8, size));
            transferFile.close();
            if (type == MSG_UPLOAD_RESPONSE) showDone(response);
            else setBusy(response, false);
            log(response);
            s->disconnectFromHost();
        });
    }

    void sendNextUploadChunk(QTcpSocket *s, quint32 index, quint32 total) {
        if (!transferFile.isOpen()) return;
        if (index >= total) {
            s->write(messageHeader(MSG_DONE_TRANSFER, 0));
            return;
        }
        QByteArray chunk = transferFile.read(QtChunkSize);
        QByteArray packet = messageHeader(MSG_CHUNK_DATA, 12 + quint32(chunk.size())) +
                            chunkHeader(index, total, quint32(chunk.size())) + chunk;
        s->write(packet);
        transferredBytes += chunk.size();
        int pct = totalBytes > 0 ? int((transferredBytes * 100) / totalBytes) : 100;
        progress->setValue(qBound(0, pct, 100));
        status->setText(QString("Uploading... %1%").arg(progress->value()));
        QTimer::singleShot(1, this, [this, s, index, total] { sendNextUploadChunk(s, index + 1, total); });
    }

    void downloadSelected() {
        QListWidgetItem *item = fileList->currentItem();
        if (!item) {
            setBusy("Select a server file first.", false);
            return;
        }
        downloadName = item->data(Qt::UserRole).toString();
        QDir().mkpath(downloadDir->text());
        transferFile.setFileName(QDir(downloadDir->text()).absoluteFilePath(downloadName));
        if (!transferFile.open(QIODevice::WriteOnly)) {
            setBusy("Could not create output file.", false);
            return;
        }

        rx.clear();
        expectedChunk = 0;
        expectedTotal = 0;
        progress->setRange(0, 100);
        progress->setValue(0);
        setBusy("Downloading " + downloadName + "...", true);
        QTcpSocket *s = newSocket();
        connect(s, &QTcpSocket::connected, this, [s, this] {
            QByteArray name = downloadName.toUtf8();
            s->write(messageHeader(MSG_DOWNLOAD_REQUEST, quint32(name.size())) + name);
        });
        connect(s, &QTcpSocket::readyRead, this, [this, s] { consumeDownload(s); });
    }

    void consumeDownload(QTcpSocket *s) {
        rx += s->readAll();
        while (rx.size() >= 8) {
            quint32 type = readU32(rx.constData());
            quint32 size = readU32(rx.constData() + 4);
            if (rx.size() < int(8 + size)) return;
            QByteArray payload = rx.mid(8, size);
            rx.remove(0, 8 + size);
            if (type == MSG_ERROR) {
                transferFile.close();
                setBusy(QString::fromUtf8(payload), false);
                return;
            }
            if (type == MSG_DONE_TRANSFER) {
                transferFile.close();
                showDone("Downloaded to " + QFileInfo(transferFile.fileName()).absoluteFilePath());
                s->disconnectFromHost();
                return;
            }
            if (type != MSG_CHUNK_DATA || payload.size() < 12) {
                transferFile.close();
                setBusy("Invalid download packet.", false);
                return;
            }
            quint32 index = readU32(payload.constData());
            quint32 total = readU32(payload.constData() + 4);
            quint32 chunkSize = readU32(payload.constData() + 8);
            if (index != expectedChunk || payload.size() != int(12 + chunkSize)) {
                transferFile.close();
                setBusy("Invalid chunk order.", false);
                return;
            }
            expectedTotal = total;
            transferFile.write(payload.constData() + 12, chunkSize);
            expectedChunk++;
            int pct = expectedTotal > 0 ? int((expectedChunk * 100) / expectedTotal) : 100;
            progress->setValue(qBound(0, pct, 100));
            status->setText(QString("Downloading... %1%").arg(progress->value()));
        }
    }

    void saveConfig() {
        QSettings settings(".sharenet_qt_client.ini", QSettings::IniFormat);
        settings.setValue("server_ip", serverIp->text());
        settings.setValue("server_port", serverPort->text());
        settings.setValue("upload_path", uploadPath->text());
        settings.setValue("download_dir", downloadDir->text());
    }

    void loadConfig() {
        QSettings settings(".sharenet_qt_client.ini", QSettings::IniFormat);
        serverIp->setText(settings.value("server_ip", serverIp->text()).toString());
        serverPort->setText(settings.value("server_port", serverPort->text()).toString());
        uploadPath->setText(settings.value("upload_path", uploadPath->text()).toString());
        downloadDir->setText(settings.value("download_dir", downloadDir->text()).toString());
    }
};

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    ClientWindow window;
    window.show();
    return app.exec();
}
