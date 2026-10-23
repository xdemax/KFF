#include <QApplication>
#include <QStringList>

#include "MainWindow.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

int main(int argc, char* argv[]) {
#ifdef Q_OS_WIN
    // Не показывать системные диалоги "нет диска в устройстве" при опросе дисков
    SetErrorMode(SEM_FAILCRITICALERRORS);
#endif
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("KarabasFlasher"));

    const QStringList args = app.arguments();
    const bool fileLogging = args.contains(QStringLiteral("-log")) || args.contains(QStringLiteral("--log"));

    MainWindow w(fileLogging);
    w.show();
    return app.exec();
}
