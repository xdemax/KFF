#include "Uf2.h"

#include <QFile>
#include <QtEndian>

bool isValidUf2(const QString& path, QString* why) {
    auto fail = [&](const QString& m) {
        if (why) *why = m;
        return false;
    };

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return fail("cannot open file: " + f.errorString());

    const qint64 size = f.size();
    if (size < 512 || (size % 512) != 0) return fail("file size is not a multiple of 512 bytes");

    const QByteArray blk = f.read(512);
    if (blk.size() != 512) return fail("read error");

    const quint32 m0   = qFromLittleEndian<quint32>(blk.constData());
    const quint32 m1   = qFromLittleEndian<quint32>(blk.constData() + 4);
    const quint32 mEnd = qFromLittleEndian<quint32>(blk.constData() + 508);
    if (m0 != 0x0A324655u || m1 != 0x9E5D5157u || mEnd != 0x0AB16F30u)
        return fail("UF2 magic numbers not found");
    return true;
}
