# Аудит кода Karabas Flasher

Дата: 2026-10-25

Ниже — результаты ревью по файлам проекта. Разделено на **баги** (могут привести к неверному поведению), **потенциальные проблемы** и **мелочи**.

---

## 1. `src/Worker.cpp`

### 1.1. `downloadFiles` — утечка `QSaveFile` при ошибке открытия
Здесь всё корректно (`unique_ptr`). **Не баг.**

### 1.2. `downloadFiles` — `results[i].ok` не сбрасывается при ошибке открытия
Если `file->open()` не удался, `results[i].ok` остаётся `false` (инициализировано), `error` заполнен — ок. Но `bytes` остаётся 0, а `timer` не запускался — ок.

### 1.3. `downloadFiles` — `maybeQuit()` не вызывается после `continue`
Если **все** файлы не открылись (например, нет прав на `coresDir`), `active` пуст, `next == jobs.size()`, но `loop.exec()` не вызывается (проверка `if (!active.isEmpty())`). Функция вернёт результаты — ок. **Не баг**, но стоит убедиться, что `startMore()` в цикле `while` не зациклится: `next` инкрементируется до `continue`, так что цикл конечен. **Ок.**

### 1.4. `downloadFiles` — `cancelPoll` не останавливается при раннем выходе
Если `active` пуст — `cancelPoll` не запускался, `stop()` не нужен. **Ок.**

### 1.5. `httpFetch` — `QNetworkAccessManager nam` создаётся на стеке, `reply` — его ребёнок
После выхода из функции `nam` уничтожается, `reply` — тоже. Но `reply` используется **после** `loop.exec()` (чтение `status`, `error`). Это **безопасно**, т.к. `nam` жив до конца функции. **Ок.**

### 1.6. `httpFetch` — `drain()` вызывается после `loop.exec()`, но `reply` мог быть удалён?
Нет, `nam` жив. **Ок.**

### 1.7. `doDownload` — `firmwareReady` эмитится до проверки `isValidUf2`
Проверка **до** `firmwareReady` — ок. **Не баг.**

### 1.8. `doDownload` — маркер `download_complete.txt` не удаляется при повторной загрузке
`removeRecursively()` удаляет всё, включая старый маркер. **Ок.**

### 1.9. `doDownload` — при частичном успехе маркер не создаётся, но старый мог остаться
`removeRecursively()` удаляет всё, включая старый маркер. **Ок.**

### 1.10. `updateCoresOnSd` — `rollbackCores` не восстанавливает файлы, если `QFile::rename` не удался
Если rename не удался — файл остаётся в backup. Логируется только счётчик. **Приемлемо**, но стоит логировать имена неудачных.

### 1.11. `updateCoresOnSd` — `QDir().rmdir(backupDir)` не проверяет результат
Если папка не пуста (например, `cleanupVolumeJunk` не успел) — `rmdir` вернёт `false`, папка останется. **Мелочь.**

### 1.12. `doFlash` — `resetPortToBootloader` вызывается даже если `portFound == false`
Если порт не найден, `resetPortToBootloader` всё равно пытается открыть — это **намеренно** (порт мог переименоваться). **Ок.**

### 1.13. `doFlash` — `m_permissionDenied` не сбрасывается между операциями
`guarded()` сбрасывает `m_permissionDenied = false` в начале. **Ок.**

### 1.14. `doFlash` — после `updateCoresOnSd` не проверяется `m_cancel`
`pause` проверяет отмену. **Ок.**

### 1.15. `doFlash` — `copyFileSynced` в `boot.mountPoint` может упасть, если `boot.mountPoint` пуст
`waitForMountedVolume` гарантирует непустой `mountPoint` (проверяет `!c.mountPoint.isEmpty()`). **Ок.**

### 1.16. `sendMscCommand` — `sp.waitForReadyRead(1000)` блокирует поток на 1 с
Это в рабочем потоке, GUI не блокируется. **Ок.**

### 1.17. `sendMscCommand` — `m_permissionDenied` устанавливается, но `portAccessDenied` эмитится только при `PermissionError`
Ок.

### 1.18. `resetPortToBootloader` — `sp.setBaudRate(1200)` вызывается дважды
Второй вызов **избыточен**, но безвреден. **Мелочь.**

### 1.19. `resetPortToBootloader` — `QThread::msleep(50)` в рабочем потоке
Ок.

### 1.20. `waitForMountedVolume` — `lastMountTry = -100000` — магическое число
Работает, но лучше `-std::numeric_limits<qint64>::max()`. **Мелочь.**

### 1.21. `waitForMountedVolume` — `pause(500)` возвращает `false` при отмене, но `seenUnmounted` не сбрасывается
При отмене функция возвращает `false` — ок.

### 1.22. `ejectBestEffort` — логирует `detail` даже при успехе
`ejectCandidate` в Linux/macOS/Windows **очищает** `detail` в начале (`setDetail(QString())`). Так что `detail` пуст при успехе. **Ок.**

### 1.23. `guarded` — `m_cancel.store(false)` в начале
Если пользователь нажал «Отмена» до запуска — сбросится. Но `runDownload`/`runFlash` вызываются через `invokeMethod` после `setBusy(true)`, отмена возможна только во время работы. **Ок.**

### 1.24. `guarded` — `emit finished()` в конце, даже если было исключение
Ок — GUI разблокируется.

---

## 2. `src/MainWindow.cpp`

### 2.1. `onRefreshPorts` — `m_comCombo->setCurrentIndex(added == 0 ? 0 : 1)`
Если `added == 0`, индекс 0 — «(none)». Если `added > 0`, индекс 1 — первый реальный порт. **Ок**, но если пользователь ранее выбрал порт, выбор сбрасывается. **Мелочь UX.**

### 2.2. `onRefreshPorts` — `appendLog` вызывается до `m_log` создан?
`m_log` создаётся в конструкторе до `onRefreshPorts()`. **Ок.**

### 2.3. `onPortAccessDenied` — `m_accessProc` не удаляется при `FailedToStart` до `deleteLater`
Ок.

### 2.4. `onPortAccessDenied` — `m_accessProc` может быть удалён дважды
Если `FailedToStart` и `finished` придут оба — `deleteLater` дважды. `deleteLater` идемпотентен? Нет, но второй вызов на уже удалённом объекте — UB. **Потенциальный баг.** Нужно проверять `if (!m_accessProc) return;` в обоих лямбдах.

### 2.5. `onPortAccessDenied` — `script` собирается через `QString::arg` с `rule`, содержащим `%`
`rule` содержит `%1`, `%2` (из `.arg(vidS, pidS)`), но после `.arg(rule, file, oldFile)` они уже подставлены. **Ок**, но если `rule` содержит `%` (не должно — только hex), `.arg` может съесть. **Мелочь.**

### 2.6. `closeEvent` — `m_closeRequested` не сбрасывается
Если пользователь отменил закрытие, `m_closeRequested` остаётся `false` (устанавливается только при подтверждении). **Ок.**

### 2.7. `closeEvent` — `stopThread()` вызывается дважды (в `closeEvent` и `~MainWindow`)
`stopThread` проверяет `m_thread->isRunning()`. **Ок.**

### 2.8. `cleanupTemp` — `QDir(workDir()).removeRecursively()` без проверки
Если папка занята — вернёт `false`, мусор останется. **Мелочь.**

### 2.9. `appendLog` — `message.contains("Error")` — регистрозависимо
Логи на русском/с маленькой буквы не подсветятся. **Мелочь.**

### 2.10. `appendLog` — `line.toHtmlEscaped()` — ок, но `style` не экранируется
`style` формируется из констант. **Ок.**

### 2.11. `setBusy` — `m_mscHelp` не блокируется
`m_mscHelp` остаётся активным во время работы. **Мелочь UX** — можно нажать «?» во время прошивки, ничего страшного.

### 2.12. `onDownload` — `m_downloadBtn->setText(tr("Downloading..."))` не сбрасывается при ошибке
`setBusy(false)` в `onWorkerFinished` сбрасывает текст. **Ок.**

### 2.13. `onFlash`/`onMsc` — не проверяют `m_firmwarePath` перед запуском
Проверка в `Worker::doFlash`. **Ок.**

### 2.14. `onBrowse` — `isValidUf2` вызывается в GUI-потоке
Для больших файлов может подморозить UI. **Мелочь.**

---

## 3. `src/platform_linux.cpp`

### 3.1. `primaryNodeForDisk` — `entryList` с `QDir::Dirs` может вернуть не раздел
В `/sys/block/sdb/` есть `sdb1`, но также `queue`, `slaves`, `holders` и т.д. Фильтр `id + '*'` отсекает их (они не начинаются с `sdb`). **Ок.**

### 3.2. `primaryNodeForDisk` — если разделов нет, возвращает `/dev/sdb`
Ок.

### 3.3. `newDisks` — `c.label.compare("RPI-RP2", Qt::CaseInsensitive) == 0` — пропускает bootloader
Ок.

### 3.4. `newDisks` — `QFileInfo::exists(c.node)` — если udev ещё не создал узел, диск пропускается
Ок, но при следующем вызове `newDisks` (через 500 мс) он появится. **Ок.**

### 3.5. `mountCandidate` — `udisksctl mount` без `--no-user-interaction`?
Есть `--no-user-interaction`. **Ок.**

### 3.6. `mountCandidate` — regex `^Mounted\s+\S+\s+at\s+(.+?)\.?$`
Жадность `.+?` + `\.?$` — если путь заканчивается на `.`, отрежет. **Мелочь.**

### 3.7. `ejectCandidate` — `::sync()` вызывается до размонтирования
Ок.

### 3.8. `ejectCandidate` — `udisksEject` использует `gdbus`, потом `busctl`
Ок.

### 3.9. `ejectCandidate` — `eject` как fallback требует прав на `/dev/sdX`
Ок, документировано.

### 3.10. `diagnostics` — `arg(id).arg(size).arg(node, label, ...)` — 5 аргументов, но `arg` с 3 строками
`.arg(node, label, ...)` — три строки, но `%3`, `%4`, `%5` — три плейсхолдера. **Ок.**

### 3.11. `runProcess` — `p.waitForStarted(3000)` — если программа не найдена, вернёт `false`, код `-1`
Ок.

### 3.12. `runProcess` — `p.kill()` при таймауте, но не `terminate()`
Ок для внешних утилит.

---

## 4. `src/platform_macos.cpp`

### 4.1. `usbVolumes` — `runProcess("diskutil", ...)` для каждого тома
Может быть медленно при многих томах. **Мелочь.**

### 4.2. `usbVolumes` — `plistBool(xml, "Internal", true)` — если ключа нет, считает внутренним
Ок, консервативно.

### 4.3. `mountCandidate` — `diskutil mount` без `-plist`
Ок.

### 4.4. `ejectCandidate` — `diskutil unmount` по `mountPoint`, потом `eject` по `diskNode`
Ок.

### 4.5. `cleanupVolumeJunk` — удаляет `.Trashes`, `.Spotlight-V100` и т.д.
Ок, но `.metadata_never_index` не создаётся — мусор появится снова. **Мелочь.**

---

## 5. `src/platform_windows.cpp`

### 5.1. `volumeLabel` — `GetVolumeInformationW` с `nullptr` для FS name
Ок.

### 5.2. `ejectCandidate` — `FSCTL_LOCK_VOLUME` в цикле 10 раз по 500 мс
Ок.

### 5.3. `ejectCandidate` — `IOCTL_STORAGE_MEDIA_REMOVAL` с `PreventMediaRemoval = FALSE`
Ок.

### 5.4. `ejectCandidate` — `CloseHandle` вызывается и при успехе, и при ошибке
Ок.

### 5.5. `newDisks` — `QDir(r).exists()` — если буква есть, но том не смонтирован, пропустит
Ок.

---

## 6. `CMakeLists.txt`

### 6.1. `qt_add_executable` — нет `src/Audit.cpp` (если аудит существует)
Сейчас в списке нет файлов аудита. **Ок для текущего кода.**

### 6.2. `CPACK_DEBIAN_PACKAGE_DEPENDS` — `libglib2.0-bin` для `gdbus`
Ок.

### 6.3. `CPACK_DEBIAN_PACKAGE_RECOMMENDS` — `eject` как fallback
Ок.

---

## 7. `src/Worker.h`

### 7.1. `m_permissionDenied` — комментарий «только рабочий поток»
Ок.

### 7.2. `m_cancel` — `std::atomic<bool>`
Ок.

---

## 8. `src/MainWindow.h`

### 8.1. `m_accessProc` — не проверяется на `nullptr` в `onPortAccessDenied` перед `deleteLater`
См. п. 2.4 — **потенциальный баг**.

---

## Итог: что стоит исправить

**Критично:**
- **2.4** — двойной `deleteLater` для `m_accessProc` (UB).

**Желательно:**
- **1.10** — логировать имена неудачных rollback.
- **1.11** — проверять `rmdir(backupDir)`.
- **1.18** — убрать дублирующий `setBaudRate(1200)`.
- **2.9** — регистронезависимая подсветка логов.
- **2.11** — блокировать `m_mscHelp` в `setBusy`.

**Мелочи:**
- **1.20** — магическое число `-100000`.
- **3.6** — regex для `Mounted ... at ...`.
- **4.5** — создавать `.metadata_never_index` на macOS.
