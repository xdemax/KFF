#include <QApplication>
#include <QStringList>

#include <cstdio>
#include <cstring>

#include "MainWindow.h"
#include "Version.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

int main(int argc, char* argv[]) {
    // --version / -v: вывести версию и выйти (до создания окна)
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--version") || !std::strcmp(argv[i], "-v")) {
            std::printf("Karabas Flasher %s (%s)\n", APP_VERSION, APP_GIT_DESCRIBE);
            return 0;
        }
    }

#ifdef Q_OS_WIN
    // Не показывать системные диалоги "нет диска в устройстве" при опросе дисков
    SetErrorMode(SEM_FAILCRITICALERRORS);
#endif
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("KarabasFlasher"));
    QCoreApplication::setApplicationVersion(QString::fromLatin1(APP_VERSION));

    const QStringList args = app.arguments();
    const bool fileLogging = args.contains(QStringLiteral("-log")) || args.contains(QStringLiteral("--log"));

    MainWindow w(fileLogging);
    w.show();
    return app.exec();
}
