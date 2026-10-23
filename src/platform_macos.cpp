// macOS: тома монтируются системой в /Volumes. Свойства тома берутся из `diskutil info -plist`.
// Не проверено на реальном Mac.
#include "Platform.h"

#include <QDir>
#include <QProcess>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QStringList>
#include <QThread>

#include <unistd.h>

namespace Platform {
namespace {

int runProcess(const QString& program, const QStringList& args, int timeoutMs,
               QString* out = nullptr, QString* err = nullptr) {
    QProcess p;
    p.start(program, args);
    if (!p.waitForStarted(3000)) return -1;
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(1000);
        return -2;
    }
    if (out) *out = QString::fromUtf8(p.readAllStandardOutput());
    if (err) *err = QString::fromUtf8(p.readAllStandardError());
    return p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -3;
}

QString xmlUnescape(QString s) {
    s.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    s.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    s.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    s.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
    s.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    return s;
}

QString plistString(const QString& xml, const QString& key) {
    const QRegularExpression re(QStringLiteral("<key>") + QRegularExpression::escape(key) +
                                QStringLiteral("</key>\\s*<string>(.*?)</string>"),
                                QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch m = re.match(xml);
    return m.hasMatch() ? xmlUnescape(m.captured(1)) : QString();
}

bool plistBool(const QString& xml, const QString& key, bool def) {
    const QRegularExpression re(QStringLiteral("<key>") + QRegularExpression::escape(key) +
                                QStringLiteral("</key>\\s*<(true|false)/>"));
    const QRegularExpressionMatch m = re.match(xml);
    return m.hasMatch() ? (m.captured(1) == QLatin1String("true")) : def;
}

// Внешние USB-тома, смонтированные в /Volumes
QList<Candidate> usbVolumes() {
    QList<Candidate> out;
    const QList<QStorageInfo> vols = QStorageInfo::mountedVolumes();
    for (const QStorageInfo& s : vols) {
        const QString root = s.rootPath();
        if (!root.startsWith(QStringLiteral("/Volumes/")) || !s.isReady()) continue;

        QString xml;
        if (runProcess(QStringLiteral("diskutil"),
                       QStringList() << QStringLiteral("info") << QStringLiteral("-plist") << root, 10000, &xml) != 0)
            continue;
        const QString node = plistString(xml, QStringLiteral("DeviceNode"));
        const QString whole = plistString(xml, QStringLiteral("ParentWholeDisk"));
        const QString bus = plistString(xml, QStringLiteral("BusProtocol"));
        if (node.isEmpty() || whole.isEmpty()) continue;
        if (plistBool(xml, QStringLiteral("Internal"), true)) continue;
        if (bus.compare(QStringLiteral("USB"), Qt::CaseInsensitive) != 0) continue;

        Candidate c;
        c.diskId = whole;                                   // "disk4"
        c.diskNode = QStringLiteral("/dev/") + whole;
        c.node = node;                                      // "/dev/disk4s1"
        c.label = plistString(xml, QStringLiteral("VolumeName"));
        c.mountPoint = root;
        out << c;
    }
    return out;
}

bool isBootloaderLabel(const QString& l) { return l.compare(QStringLiteral("RPI-RP2"), Qt::CaseInsensitive) == 0; }

}  // namespace

QSet<QString> readyRemovableDisks() {
    QSet<QString> ids;
    for (const Candidate& c : usbVolumes()) ids.insert(c.diskId);
    return ids;
}

QList<Candidate> newDisks(const QSet<QString>& before) {
    QList<Candidate> out;
    for (const Candidate& c : usbVolumes()) {
        if (before.contains(c.diskId)) continue;
        if (isBootloaderLabel(c.label)) continue;
        out << c;
    }
    return out;
}

QList<Candidate> bootloaderDisks() {
    QList<Candidate> out;
    for (const Candidate& c : usbVolumes()) {
        if (isBootloaderLabel(c.label)) out << c;
    }
    return out;
}

bool mountCandidate(Candidate& c, QString* detail) {
    if (!c.mountPoint.isEmpty()) return true;
    QString out, err;
    const int rc = runProcess(QStringLiteral("diskutil"),
                              QStringList() << QStringLiteral("mount") << c.node, 15000, &out, &err);
    if (rc != 0) {
        if (detail) *detail = err.trimmed().isEmpty() ? QStringLiteral("diskutil mount failed") : err.trimmed();
        return false;
    }
    QString xml;
    runProcess(QStringLiteral("diskutil"), QStringList() << QStringLiteral("info") << QStringLiteral("-plist") << c.node,
               10000, &xml);
    c.mountPoint = plistString(xml, QStringLiteral("MountPoint"));
    return !c.mountPoint.isEmpty();
}

bool ejectCandidate(const Candidate& c, QString* detail) {
    auto setDetail = [&](const QString& s) { if (detail) *detail = s; };
    setDetail(QString());

    ::sync();

    // 1) размонтирование (Spotlight может ненадолго занять том - повторяем)
    bool unmounted = false;
    QString lastErr;
    for (int i = 0; i < 10 && !unmounted; ++i) {
        QString out, err;
        const int rc = runProcess(QStringLiteral("diskutil"),
                                  QStringList() << QStringLiteral("unmount") << c.mountPoint, 15000, &out, &err);
        if (rc == 0) {
            unmounted = true;
        } else {
            lastErr = (err.trimmed().isEmpty() ? out.trimmed() : err.trimmed());
            QThread::msleep(500);
        }
    }
    if (!unmounted) {
        setDetail(QStringLiteral("cannot unmount ") + c.mountPoint + QStringLiteral(": ") + lastErr);
        return false;
    }

    // 2) извлечение носителя
    QString out, err;
    const int rc = runProcess(QStringLiteral("diskutil"),
                              QStringList() << QStringLiteral("eject") << c.diskNode, 15000, &out, &err);
    if (rc != 0) {
        setDetail(QStringLiteral("eject failed: ") + (err.trimmed().isEmpty() ? out.trimmed() : err.trimmed()));
        return false;
    }
    return true;
}

QString diagnostics() {
    QStringList lines;
    const QList<Candidate> vols = usbVolumes();
    if (vols.isEmpty()) lines << QStringLiteral("Diagnostics: no external USB volumes are mounted in /Volumes.");
    for (const Candidate& c : vols) {
        lines << QStringLiteral("Diagnostics: %1 node=%2 label=[%3] mounted at %4")
                     .arg(c.diskId, c.node, c.label, c.mountPoint);
    }
    return lines.join(QLatin1Char('\n'));
}

}  // namespace Platform
