#pragma once
#include <QString>

// Проверка, что файл похож на UF2: размер кратен 512, в первом блоке есть magic-числа.
bool isValidUf2(const QString& path, QString* why);
