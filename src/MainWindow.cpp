#include "MainWindow.h"

#include "Models.h"
#include "Uf2.h"
#include "Util.h"
#include "Version.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QStandardPaths>
#include <QThread>
#include <QVBoxLayout>

#include <algorithm>

MainWindow::MainWindow(bool fileLogging, QWidget* parent) : QWidget(parent), m_fileLogging(fileLogging) {
    setWindowTitle(QStringLiteral("Karabas Go Family Firmware Flasher v") + QString::fromLatin1(APP_VERSION));

    auto* root = new QVBoxLayout(this);

    root->addWidget(new QLabel(tr("Select Device Model:")));
    auto* row1 = new QHBoxLayout;
    m_modelCombo = new QComboBox;
    for (const ModelInfo& m : models()) m_modelCombo->addItem(m.name);
    m_downloadBtn = new QPushButton(tr("Download via GitHub"));
    row1->addWidget(m_modelCombo, 1);
    row1->addWidget(m_downloadBtn);
    root->addLayout(row1);

    m_coresChk = new QCheckBox(tr("Update core too"));
    m_delTempChk = new QCheckBox(tr("Delete temp files on exit"));
    m_delTempChk->setChecked(true);
    root->addWidget(m_coresChk);
    root->addWidget(m_delTempChk);

    root->addWidget(new QLabel(tr("Firmware File Path (.uf2):")));
    auto* row2 = new QHBoxLayout;
    m_pathEdit = new QLineEdit;
    m_pathEdit->setReadOnly(true);
    m_browseBtn = new QPushButton(tr("Browse..."));
    row2->addWidget(m_pathEdit, 1);
    row2->addWidget(m_browseBtn);
    root->addLayout(row2);

    root->addWidget(new QLabel(tr("Serial port (choose '(none)' to skip reset):")));
    auto* row3 = new QHBoxLayout;
    m_comCombo = new QComboBox;
    m_refreshBtn = new QPushButton(tr("Refresh ports"));
    row3->addWidget(m_comCombo, 1);
    row3->addWidget(m_refreshBtn);
    root->addLayout(row3);

    auto* row4 = new QHBoxLayout;
    m_flashBtn = new QPushButton(tr("FLASH!"));
    m_flashBtn->setMinimumHeight(40);
    m_mscBtn = new QPushButton(tr("SD Mode (MSC)"));
    m_mscBtn->setMinimumHeight(40);
    row4->addWidget(m_flashBtn, 2);
    row4->addWidget(m_mscBtn, 1);
    root->addLayout(row4);

    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(5000);          // старые строки отбрасываются автоматически
    root->addWidget(m_log, 1);

    resize(520, 600);

    // ---- Рабочий поток ----
    m_thread = new QThread(this);
    m_worker = new Worker;
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &Worker::log, this, &MainWindow::appendLog);
    connect(m_worker, &Worker::firmwareReady, this, &MainWindow::onFirmwareReady);
    connect(m_worker, &Worker::finished, this, &MainWindow::onWorkerFinished);
    connect(m_worker, &Worker::portAccessDenied, this, &MainWindow::onPortAccessDenied);
    m_thread->start();

    connect(m_downloadBtn, &QPushButton::clicked, this, &MainWindow::onDownload);
    connect(m_browseBtn, &QPushButton::clicked, this, &MainWindow::onBrowse);
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshPorts);
    connect(m_flashBtn, &QPushButton::clicked, this, &MainWindow::onFlash);
    connect(m_mscBtn, &QPushButton::clicked, this, &MainWindow::onMsc);

    if (m_fileLogging) openLogFile();
    appendLog(QStringLiteral("Karabas Flasher v") + QString::fromLatin1(APP_VERSION) + QStringLiteral(" (") +
              QString::fromLatin1(APP_GIT_DESCRIBE) + QLatin1Char(')'));
    onRefreshPorts();
    appendLog(QStringLiteral("Application is ready."));
}

MainWindow::~MainWindow() {
    stopThread();
}

void MainWindow::stopThread() {
    if (m_thread && m_thread->isRunning()) {
        m_worker->requestCancel();
        m_thread->quit();
        m_thread->wait();
    }
}

void MainWindow::openLogFile() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    m_logFile.setFileName(dir + QStringLiteral("/flasher_log.txt"));
    if (m_logFile.open(QIODevice::Append)) {
        m_logFile.write("--- Application started with -log switch ---\n");
        m_logFile.flush();
        appendLog("Logging to file: " + m_logFile.fileName());
    } else {
        appendLog("Warning: Could not open the log file " + m_logFile.fileName() + "; file logging is disabled.");
    }
}

void MainWindow::appendLog(const QString& message) {
    const QString line = "[" + QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") + "] " + message;

    QString color;
    if (message.contains("Error") || message.contains("Failed") || message.contains("Warning")) {
        color = QStringLiteral("#e04040");
    } else if (message.contains("Success") || message.contains("completed") || message.contains("Found") ||
               message.contains("Ready") || message.contains("Downloaded")) {
        color = QStringLiteral("#2ea043");
    }
    const QString style = color.isEmpty() ? QStringLiteral("white-space:pre-wrap;")
                                          : "white-space:pre-wrap;color:" + color + ";";
    m_log->appendHtml("<span style=\"" + style + "\">" + line.toHtmlEscaped() + "</span>");

    if (m_logFile.isOpen()) {
        m_logFile.write((line + QLatin1Char('\n')).toUtf8());
        m_logFile.flush();
    }
}

Params MainWindow::collectParams() const {
    Params p;
    p.modelIndex = m_modelCombo->currentIndex();
    p.updateCores = m_coresChk->isChecked();
    p.portLocation = m_comCombo->currentData().toString();
    p.firmwarePath = m_firmwarePath;
    return p;
}

void MainWindow::setBusy(bool busy) {
    m_busy = busy;
    const bool en = !busy;
    for (QWidget* w : {static_cast<QWidget*>(m_modelCombo), static_cast<QWidget*>(m_downloadBtn),
                       static_cast<QWidget*>(m_coresChk), static_cast<QWidget*>(m_delTempChk),
                       static_cast<QWidget*>(m_browseBtn), static_cast<QWidget*>(m_comCombo),
                       static_cast<QWidget*>(m_refreshBtn), static_cast<QWidget*>(m_flashBtn),
                       static_cast<QWidget*>(m_mscBtn)}) {
        w->setEnabled(en);
    }
    if (en) m_downloadBtn->setText(tr("Download via GitHub"));
}

// ----------------------------------------------------------------------------
// Действия
// ----------------------------------------------------------------------------
void MainWindow::onDownload() {
    if (m_busy) return;
    setBusy(true);
    m_downloadBtn->setText(tr("Downloading..."));
    const Params p = collectParams();
    QMetaObject::invokeMethod(m_worker, [w = m_worker, p]() { w->runDownload(p); }, Qt::QueuedConnection);
}

void MainWindow::onFlash() {
    if (m_busy) return;
    setBusy(true);
    const Params p = collectParams();
    QMetaObject::invokeMethod(m_worker, [w = m_worker, p]() { w->runFlash(p); }, Qt::QueuedConnection);
}

void MainWindow::onMsc() {
    if (m_busy) return;
    setBusy(true);
    const Params p = collectParams();
    QMetaObject::invokeMethod(m_worker, [w = m_worker, p]() { w->runMsc(p); }, Qt::QueuedConnection);
}

void MainWindow::onBrowse() {
    const QString path = QFileDialog::getOpenFileName(this, tr("Select firmware"), QString(),
                                                      tr("UF2 Files (*.uf2);;All Files (*)"));
    if (path.isEmpty()) return;
    QString why;
    if (!isValidUf2(path, &why)) {
        appendLog("Error: Selected file is not a valid UF2 image (" + why + "): " + path);
        return;
    }
    m_firmwarePath = path;
    m_pathEdit->setText(path);
    appendLog("User manually selected file: " + path);
}

void MainWindow::onRefreshPorts() {
    m_comCombo->clear();
    m_comCombo->addItem(tr("(none - skip reset)"), QString());
    appendLog(QStringLiteral("Scanning available serial ports..."));

    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    std::sort(ports.begin(), ports.end(), [](const QSerialPortInfo& a, const QSerialPortInfo& b) {
        return a.systemLocation() < b.systemLocation();
    });
    int added = 0;
    for (const QSerialPortInfo& info : ports) {
#if defined(Q_OS_MACOS)
        if (info.systemLocation().startsWith(QStringLiteral("/dev/tty."))) continue;   // оставляем /dev/cu.*
#endif
#if defined(Q_OS_LINUX)
        if (!info.hasVendorIdentifier()) continue;       // встроенные /dev/ttyS* без USB не показываем
#endif
#if defined(Q_OS_WIN)
        QString text = info.portName();                  // COM3
#else
        QString text = info.systemLocation();
#endif
        if (!info.description().isEmpty()) text += " (" + info.description() + ")";
        if (info.hasVendorIdentifier() && info.hasProductIdentifier()) {
            text += QStringLiteral(" [%1:%2]")
                        .arg(info.vendorIdentifier(), 4, 16, QLatin1Char('0'))
                        .arg(info.productIdentifier(), 4, 16, QLatin1Char('0'));
        }
        m_comCombo->addItem(text, info.systemLocation());
        appendLog("Found port: " + text);
        ++added;
    }
    if (added == 0) appendLog(QStringLiteral("Error: Active serial ports not found."));
    m_comCombo->setCurrentIndex(added == 0 ? 0 : 1);
}

void MainWindow::onFirmwareReady(const QString& path) {
    m_firmwarePath = path;
    m_pathEdit->setText(path);
}

void MainWindow::onWorkerFinished() {
    setBusy(false);
    if (m_closeRequested) close();
}

// Нет прав на порт (Linux): предлагаем поставить правило udev с TAG+="uaccess".
// Пароль администратора запрашивает polkit (pkexec), один раз.
void MainWindow::onPortAccessDenied(const QString& portLocation) {
#if defined(Q_OS_LINUX)
    if (m_accessProc) return;                       // установка уже идёт
    if (m_accessRuleInstalled) {
        appendLog("Warning: The access rule is already installed, but " + portLocation +
                  " is still not accessible. Replug the device (or log out and in again). "
                  "You can check the permissions with: getfacl " + portLocation);
        return;
    }

    quint16 vid = 0, pid = 0;
    bool found = false;
    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo& info : ports) {
        if (info.systemLocation() == portLocation && info.hasVendorIdentifier() && info.hasProductIdentifier()) {
            vid = info.vendorIdentifier();
            pid = info.productIdentifier();
            found = true;
            break;
        }
    }
    if (!found) {
        appendLog("Warning: Cannot identify the USB device behind " + portLocation +
                  ". Add your user to the 'dialout' group ('uucp' on Arch/Manjaro) and log in again.");
        return;
    }

    const QString vidS = QStringLiteral("%1").arg(vid, 4, 16, QLatin1Char('0'));
    const QString pidS = QStringLiteral("%1").arg(pid, 4, 16, QLatin1Char('0'));

    const auto answer = QMessageBox::question(
        this, QStringLiteral("Karabas Flasher"),
        tr("The program has no permission to open %1.\n\nInstall a udev rule that gives the current user access "
           "to this device (USB %2:%3)?\n\nYou will be asked for the administrator password once.")
            .arg(portLocation, vidS, pidS),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (answer != QMessageBox::Yes) {
        appendLog(QStringLiteral("Access rule was not installed. The serial port stays unavailable."));
        return;
    }

    // vid/pid - только шестнадцатеричные цифры, подстановка в команду безопасна
    const QString rule = QStringLiteral("SUBSYSTEM==\"tty\", ATTRS{idVendor}==\"%1\", ATTRS{idProduct}==\"%2\", "
                                        "TAG+=\"uaccess\", ENV{ID_MM_DEVICE_IGNORE}=\"1\"")
                             .arg(vidS, pidS);
    // Номер файла должен быть МЕНЬШЕ 73: тег uaccess обрабатывает 73-seat-late.rules.
    const QString file = QStringLiteral("/etc/udev/rules.d/70-karabas-%1-%2.rules").arg(vidS, pidS);
    const QString oldFile = QStringLiteral("/etc/udev/rules.d/99-karabas-%1-%2.rules").arg(vidS, pidS);
    const QString script =
        QStringLiteral("rm -f %3; printf '%s\\n' '%1' > %2 && udevadm control --reload && "
                       "udevadm trigger --subsystem-match=tty --action=change")
            .arg(rule, file, oldFile);

    m_accessProc = new QProcess(this);
    m_accessProc->setProcessEnvironment(cleanChildEnvironment());
    connect(m_accessProc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            appendLog(QStringLiteral("Error: 'pkexec' was not found (install 'polkit'). Add your user to the "
                                     "'dialout' group ('uucp' on Arch/Manjaro) and log in again."));
            m_accessProc->deleteLater();
            m_accessProc = nullptr;
        }
    });
    connect(m_accessProc, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        const QString err = QString::fromUtf8(m_accessProc->readAllStandardError()).trimmed();
        if (status == QProcess::NormalExit && code == 0) {
            m_accessRuleInstalled = true;
            appendLog(QStringLiteral("Success: Access rule installed. If the port is still not accessible, "
                                     "replug the device. Then repeat the operation."));
        } else if (code == 126 || code == 127) {
            appendLog(QStringLiteral("Warning: Administrator authorization was cancelled or denied. "
                                     "The access rule was not installed."));
        } else {
            appendLog("Error: Failed to install the access rule (code " + QString::number(code) + "): " + err);
        }
        m_accessProc->deleteLater();
        m_accessProc = nullptr;
    });

    appendLog(QStringLiteral("Requesting administrator rights to install the access rule..."));
    m_accessProc->start(QStringLiteral("pkexec"), QStringList() << QStringLiteral("sh") << QStringLiteral("-c") << script);
#else
    Q_UNUSED(portLocation);
#endif
}

void MainWindow::cleanupTemp() {
    if (m_delTempChk->isChecked()) QDir(workDir()).removeRecursively();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (m_busy) {
        const auto r = QMessageBox::warning(
            this, QStringLiteral("Karabas Flasher"),
            tr("An operation is still in progress.\n\nStop it and exit? The operation will be stopped at the "
               "nearest safe point; steps that modify the SD card or write the firmware are completed first."),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (r != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        m_closeRequested = true;
        m_worker->requestCancel();
        appendLog(QStringLiteral("Stopping the current operation; the window will close when it is safe..."));
        event->ignore();            // окно закроется из onWorkerFinished()
        return;
    }
    appendLog(QStringLiteral("Application closing."));
    cleanupTemp();
    stopThread();
    event->accept();
}
