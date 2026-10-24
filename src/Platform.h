#pragma once
// Платформенный слой: всё, что зависит от ОС при работе со съёмными дисками.
// Реализации: platform_linux.cpp, platform_windows.cpp, platform_macos.cpp
// (какая из них собирается - выбирает CMakeLists.txt).

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

namespace Platform {

struct Candidate {
    QString diskId;       // идентификатор диска, например "sdb"
    QString diskNode;     // "/dev/sdb"
    QString node;         // раздел или весь диск: "/dev/sdb1"
    QString label;        // метка тома (может быть пустой)
    QString mountPoint;   // точка монтирования; пусто, если том не смонтирован
};

// Идентификаторы USB-дисков, в которых СЕЙЧАС есть носитель (размер > 0).
// Снимок "до" команды MSC: диски без носителя в него не входят, поэтому SD,
// которая появится на прежнем имени устройства, будет считаться новой.
QSet<QString> readyRemovableDisks();

// USB-диски с носителем, которых не было в снимке (кроме тома RPI-RP2).
QList<Candidate> newDisks(const QSet<QString>& readyBefore);

// Тома с меткой RPI-RP2 (загрузчик RP2040).
QList<Candidate> bootloaderDisks();

// Смонтировать том (udisks). При успехе заполняет c.mountPoint.
bool mountCandidate(Candidate& c, QString* detail);

// Безопасно извлечь диск: sync, размонтирование, eject.
// При успехе detail может содержать примечание (например, о запасном способе).
bool ejectCandidate(const Candidate& c, QString* detail);

// Удалить служебные файлы ОС, которые она сама создаёт на смонтированном томе
// (macOS: AppleDouble "._*", .DS_Store, .Trashes и т.п.). Вызывается после копирования ядер
// и ДО извлечения карты: такие файлы могут иметь расширение ядра (._alf.kg3), и плата
// приняла бы их за ядра. Чистятся корень тома и папки из extraDirs. Возвращает число
// удалённых файлов и папок. На Linux и Windows ничего не делает.
int cleanupVolumeJunk(const QString& mountPoint, const QStringList& extraDirs);

// Текст для лога: что система видит сейчас.
QString diagnostics();

}  // namespace Platform
