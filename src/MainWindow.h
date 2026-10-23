#pragma once
#include <QFile>
#include <QString>
#include <QWidget>

#include "Worker.h"

class QCheckBox;
class QCloseEvent;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QProcess;
class QPushButton;
class QThread;

class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(bool fileLogging, QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onDownload();
    void onBrowse();
    void onRefreshPorts();
    void onFlash();
    void onMsc();
    void appendLog(const QString& message);
    void onFirmwareReady(const QString& path);
    void onWorkerFinished();
    void onPortAccessDenied(const QString& portLocation);

private:
    Params collectParams() const;
    void setBusy(bool busy);
    void openLogFile();
    void cleanupTemp();
    void stopThread();

    QComboBox* m_modelCombo = nullptr;
    QPushButton* m_downloadBtn = nullptr;
    QCheckBox* m_coresChk = nullptr;
    QCheckBox* m_delTempChk = nullptr;
    QLineEdit* m_pathEdit = nullptr;
    QPushButton* m_browseBtn = nullptr;
    QComboBox* m_comCombo = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QPushButton* m_flashBtn = nullptr;
    QPushButton* m_mscBtn = nullptr;
    QPlainTextEdit* m_log = nullptr;

    QProcess* m_accessProc = nullptr;     // pkexec: установка правила udev
    QThread* m_thread = nullptr;
    Worker* m_worker = nullptr;

    QString m_firmwarePath;
    bool m_busy = false;
    bool m_closeRequested = false;
    bool m_accessRuleInstalled = false;   // правило udev уже ставилось в этом запуске

    bool m_fileLogging = false;
    QFile m_logFile;
};
