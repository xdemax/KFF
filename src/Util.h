#pragma once
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

// Имя файла, которое безопасно подставлять в путь и URL.
bool isSafeFileName(const QString& name);

// Рабочая папка приложения (в кэше пользователя, а не в общем /tmp).
QString workDir();

// Полные пути файлов в папке dir с расширением ext (без учёта регистра).
QStringList listFilesWithExt(const QString& dir, const QString& ext);

// Окружение для запуска системных утилит (udisksctl, gdbus, pkexec...). Внутри AppImage
// переменная LD_LIBRARY_PATH указывает на библиотеки из образа, и они могут сломать системные
// программы, поэтому такие пути из неё убираются. Вне AppImage окружение не меняется.
QProcessEnvironment cleanChildEnvironment();

// Открыть папку в файловом менеджере системы (Проводник, Finder, xdg-open).
bool openInFileManager(const QString& path);

// Копирование с fsync: данные гарантированно записаны на носитель.
bool copyFileSynced(const QString& src, const QString& dst, QString* err);

// Метка тома, которую отдаёт Карабас в MSC-режиме (карта 1 - "CORES", карта 2 - "EXTRA").
// Используется, чтобы отличить Карабас от случайно вставленной флешки.
bool isKarabasVolumeLabel(const QString& label);
