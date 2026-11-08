#pragma once
#include <QByteArray>
#include <QFile>
#include <QString>

namespace FileUtil {
inline QByteArray readAll(const QString& path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}
inline bool writeAll(const QString& path, const QByteArray& data) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    return f.write(data) == data.size();
}
inline bool touch(const QString& path) { return writeAll(path, QByteArray()); }
}
