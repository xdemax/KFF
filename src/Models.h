#pragma once
#include <QList>
#include <QString>

struct ModelInfo {
    QString name;
    QString repo;
    QString firmwareUrl;
    QString ext;        // расширение файлов ядер: .kg1 / .kg2 / .kg3
};

inline const QList<ModelInfo>& models() {
    static const QList<ModelInfo> list = {
        { QStringLiteral("Karabas Go"),
          QStringLiteral("karabas-go"),
          QStringLiteral("https://raw.githubusercontent.com/andykarpov/karabas-go/master/firmware/firmware.uf2"),
          QStringLiteral(".kg1") },
        { QStringLiteral("Karabas Go Mini rev A-F"),
          QStringLiteral("karabas-mini"),
          QStringLiteral("https://raw.githubusercontent.com/andykarpov/karabas-mini/master/firmware/firmware.uf2"),
          QStringLiteral(".kg2") },
        { QStringLiteral("Karabas Go Mini rev G"),
          QStringLiteral("karabas-mini"),
          QStringLiteral("https://raw.githubusercontent.com/andykarpov/karabas-mini/master/firmware/firmware_g.uf2"),
          QStringLiteral(".kg3") },
    };
    return list;
}
