#include "Util.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#if defined(Q_OS_UNIX)
#include <cerrno>
#include <cstring>
#include <unistd.h>
#elif defined(Q_OS_WIN)
#include <io.h>
#endif

bool isSafeFileName(const QString& n) {
    if (n.isEmpty() || n.size() > 128 || n == QLatin1String(".") || n == QLatin1String(".."))
        return false;
    static const QString forbidden = QStringLiteral("\\/:*?\"<>|");
    for (const QChar c : n) {
        if (c.unicode() < 0x20) return false;
        if (forbidden.contains(c)) return false;
    }
    return !n.endsWith(QLatin1Char(' ')) && !n.endsWith(QLatin1Char('.'));
}

QString workDir() {
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/work");
}

QStringList listFilesWithExt(const QString& dir, const QString& ext) {
    QStringList out;
    const QFileInfoList list = QDir(dir).entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Name);
    for (const QFileInfo& fi : list) {
        if (fi.fileName().endsWith(ext, Qt::CaseInsensitive)) out << fi.absoluteFilePath();
    }
    return out;
}

static void setErr(QString* e, const QString& m) {
    if (e) *e = m;
}

bool copyFileSynced(const QString& src, const QString& dst, QString* err) {
    QFile in(src);
    if (!in.open(QIODevice::ReadOnly)) {
        setErr(err, "cannot open source: " + in.errorString());
        return false;
    }
    QFile out(dst);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setErr(err, "cannot create destination: " + out.errorString());
        return false;
    }

    QByteArray buf;
    buf.resize(256 * 1024);
    while (!in.atEnd()) {
        const qint64 n = in.read(buf.data(), buf.size());
        if (n < 0) {
            setErr(err, "read error: " + in.errorString());
            return false;
        }
        if (n == 0) break;
        if (out.write(buf.constData(), n) != n) {
            setErr(err, "write error: " + out.errorString());
            return false;
        }
    }
    if (!out.flush()) {
        setErr(err, "flush error: " + out.errorString());
        return false;
    }
#if defined(Q_OS_UNIX)
    if (::fsync(out.handle()) != 0) {
        setErr(err, QStringLiteral("fsync failed: ") + QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }
#elif defined(Q_OS_WIN)
    {
        const int fd = out.handle();
        if (fd >= 0) _commit(fd);      // ошибка не критична: том всё равно блокируется и сбрасывается при eject
    }
#endif
    out.close();
    return true;
}
