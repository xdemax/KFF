#pragma once
#include <QString>
#include <QStringList>

// Имя файла, которое безопасно подставлять в путь и URL.
bool isSafeFileName(const QString& name);

// Рабочая папка приложения (в кэше пользователя, а не в общем /tmp).
QString workDir();

// Полные пути файлов в папке dir с расширением ext (без учёта регистра).
QStringList listFilesWithExt(const QString& dir, const QString& ext);

// Копирование с fsync: данные гарантированно записаны на носитель.
bool copyFileSynced(const QString& src, const QString& dst, QString* err);
