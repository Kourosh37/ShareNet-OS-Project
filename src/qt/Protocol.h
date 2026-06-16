#ifndef SHARENET_QT_PROTOCOL_H
#define SHARENET_QT_PROTOCOL_H

#include <QtCore>
#include <QtEndian>
#include <QtNetwork>

enum MessageType : quint32 {
    MSG_REQUEST_LIST = 1,
    MSG_LIST_RESPONSE,
    MSG_REQUEST_UPLOAD,
    MSG_UPLOAD_RESPONSE,
    MSG_DOWNLOAD_REQUEST,
    MSG_CHUNK_DATA,
    MSG_DONE_TRANSFER,
    MSG_ERROR,
    MSG_EXIT
};

static constexpr quint32 QtChunkSize = 4096;

inline QByteArray packU32(quint32 value) {
    QByteArray out(4, Qt::Uninitialized);
    qToBigEndian(value, reinterpret_cast<uchar *>(out.data()));
    return out;
}

inline QByteArray packU64(quint64 value) {
    QByteArray out(8, Qt::Uninitialized);
    qToBigEndian(value, reinterpret_cast<uchar *>(out.data()));
    return out;
}

inline quint32 readU32(const char *data) {
    return qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(data));
}

inline quint64 readU64(const char *data) {
    return qFromBigEndian<quint64>(reinterpret_cast<const uchar *>(data));
}

inline QByteArray messageHeader(quint32 type, quint32 payloadSize) {
    return packU32(type) + packU32(payloadSize);
}

inline QByteArray chunkHeader(quint32 index, quint32 total, quint32 size) {
    return packU32(index) + packU32(total) + packU32(size);
}

inline QByteArray stringMessage(quint32 type, const QString &text) {
    QByteArray payload = text.toUtf8();
    return messageHeader(type, quint32(payload.size())) + payload;
}

inline QString safeFileName(const QString &name) {
    QFileInfo info(name);
    return info.fileName();
}

inline bool isSafeFileName(const QString &name) {
    return !name.isEmpty() && !name.contains('/') && !name.contains('\\') && !name.contains("..");
}

inline quint32 totalChunks(qint64 size) {
    if (size <= 0) return 0;
    return quint32((quint64(size) + QtChunkSize - 1) / QtChunkSize);
}

#endif
