#include "HyprIpc.h"
#include "WallpapersManager.h"
#include <QGuiApplication>
#include <QCoreApplication>
#include <QWindow>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQmlEngine>
#include <QQmlContext>
#include <QPixmapCache>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <algorithm>
#include <malloc.h>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif

static void performDeepMemoryTrim() {
    bool mainWinVisible = false;
    for (QWindow *w : QGuiApplication::topLevelWindows()) {
        if (w && w->isVisible() && w->objectName() == "mainControlWindow") {
            mainWinVisible = true;
            break;
        }
    }

    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QPixmapCache::clear();

    if (!mainWinVisible) {
        WallThumbProvider::clearCache();
    }

    for (QWindow *w : QGuiApplication::topLevelWindows()) {
        if (!w || w->property("isOverlayWindow").toBool()) continue;
        if (auto *qw = qobject_cast<QQuickWindow *>(w)) {
            QQmlEngine *eng = qmlEngine(qw);
            if (!eng && qw->contentItem()) eng = qmlEngine(qw->contentItem());
            if (eng) {
                eng->collectGarbage();
                if (!mainWinVisible) eng->trimComponentCache();
            }
            if (!mainWinVisible) {
                qw->releaseResources();
            }
        }
    }

    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
#ifdef Q_OS_WIN
    _heapmin();
    EmptyWorkingSet(GetCurrentProcess());
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
#endif
}

HyprIpc::HyprIpc(QObject *parent) : QObject(parent) {
    QFile vf(QCoreApplication::applicationDirPath() + "/version.txt");
    if (vf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString v = QTextStream(&vf).readLine().trimmed();
        if (!v.isEmpty()) m_appVersion = v;
    }
    m_updateStatus = QString("Текущая версия: v%1 (GitHub: whrr-fdev/WinWal)").arg(m_appVersion);
}

void HyprIpc::dispatch(const QString &cmd) {
    Q_UNUSED(cmd);
}

QString HyprIpc::getActiveWorkspace() {
    return QStringLiteral("{\"id\":1,\"name\":\"1\"}");
}

void HyprIpc::trimMemory() {
    QTimer::singleShot(150, []() { performDeepMemoryTrim(); });
}

void HyprIpc::ensureUpdProcess() {
    if (m_updProc) return;
    m_updProc = new QProcess(this);
    connect(m_updProc, &QProcess::readyReadStandardOutput, this, [this]() {
        while (m_updProc->canReadLine()) {
            QString line = QString::fromUtf8(m_updProc->readLine()).trimmed();
            if (line.startsWith("UPD_PROG:")) {
                int pct = std::clamp(line.mid(9).toInt(), 0, 100);
                m_updateProgress = pct / 100.0;
                m_updateStatus = QString("󰇚 Скачивание обновления с GitHub: %1%").arg(pct);
                emit updateStateChanged();
            } else if (line.startsWith("VERSION:")) {
                QString rem = line.mid(8).trimmed();
                if (rem.startsWith('v', Qt::CaseInsensitive)) rem = rem.mid(1);
                m_remoteVersion = rem;
                if (!rem.isEmpty() && rem != m_appVersion) {
                    m_updateAvailable = true;
                    m_updateStatus = QString("🔥 Доступна новая версия v%1! Нажми «Обновить с GitHub»").arg(rem);
                } else {
                    m_updateAvailable = false;
                    m_updateStatus = QString("✓ Установлена последняя версия (v%1)").arg(m_appVersion);
                }
                emit updateStateChanged();
            } else if (line.startsWith("UPDATED_OK")) {
                m_updateProgress = 1.0;
                m_updateStatus = "✓ Обновление 100% скачано! Перезапуск WinWal...";
                emit updateStateChanged();
                QString applyScript = QDir::toNativeSeparators(QDir::tempPath() + "/winwal_self_update/apply_upd.ps1");
                QProcess::startDetached("powershell.exe", {"-NoProfile", "-ExecutionPolicy", "Bypass", "-WindowStyle", "Hidden", "-File", applyScript});
                QTimer::singleShot(400, this, &HyprIpc::quitApplication);
            } else if (line.startsWith("NO_REPO")) {
                m_updateStatus = "Не удалось скачать обновление (проверь соединение)";
                emit updateStateChanged();
            }
        }
    });
    connect(m_updProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
        m_checkingUpdate = false;
        emit updateStateChanged();
    });
}

void HyprIpc::checkForUpdates() {
    if (m_checkingUpdate) return;
    ensureUpdProcess();
    m_checkingUpdate = true;
    m_updateProgress = 0.0;
    m_updateStatus = "Проверка обновлений на GitHub (whrr-fdev/WinWal)...";
    emit updateStateChanged();

    QString psCheck =
        "$ErrorActionPreference = 'SilentlyContinue'\r\n"
        "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12\r\n"
        "$v = ''\r\n"
        "try {\r\n"
        "  $raw = Invoke-RestMethod -Uri 'https://raw.githubusercontent.com/whrr-fdev/WinWal/main/version.txt' -Headers @{'User-Agent'='WinWal'} -TimeoutSec 6\r\n"
        "  if ($raw) { $v = $raw.ToString().Trim() }\r\n"
        "} catch {}\r\n"
        "if (-not $v) {\r\n"
        "  try {\r\n"
        "    $rel = Invoke-RestMethod -Uri 'https://api.github.com/repos/whrr-fdev/WinWal/releases/latest' -Headers @{'User-Agent'='WinWal'} -TimeoutSec 6\r\n"
        "    if ($rel -and $rel.tag_name) { $v = $rel.tag_name.Trim() }\r\n"
        "  } catch {}\r\n"
        "}\r\n"
        "if ($v) { [Console]::WriteLine('VERSION:' + $v) } else { [Console]::WriteLine('NO_REPO') }\r\n";

    QString psPath = QDir::tempPath() + "/winwal_check_upd.ps1";
    QFile f(psPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        f.write("\xEF\xBB\xBF");
        f.write(psCheck.toUtf8());
        f.close();
    }
    m_updProc->start("powershell.exe", {"-NoProfile", "-ExecutionPolicy", "Bypass", "-WindowStyle", "Hidden", "-File", QDir::toNativeSeparators(psPath)});
}

void HyprIpc::installUpdate() {
    if (m_checkingUpdate) return;
    ensureUpdProcess();
    m_checkingUpdate = true;
    m_updateProgress = 0.02;
    m_updateStatus = "󰇚 Скачивание обновления с GitHub: 0%";
    emit updateStateChanged();

    QString appDir = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
    QString appExe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());

    QStringList lines = {
        "$ErrorActionPreference = 'SilentlyContinue'",
        "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12",
        QString("$appDir = '%1'").arg(appDir),
        QString("$appExe = '%1'").arg(appExe),
        "$tmp = Join-Path $env:TEMP 'winwal_self_update'",
        "Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue",
        "New-Item -ItemType Directory -Force -Path $tmp | Out-Null",
        "$newExe = Join-Path $tmp 'WinWal.exe'",
        "$newVer = Join-Path $tmp 'version.txt'",
        "$dlUrl = 'https://github.com/whrr-fdev/WinWal/raw/main/WinWal-Portable/WinWal.exe'",
        "try {",
        "    $req = [System.Net.HttpWebRequest]::Create($dlUrl)",
        "    $req.UserAgent = 'WinWal-Updater'",
        "    $req.AllowAutoRedirect = $true",
        "    $resp = $req.GetResponse()",
        "    $total = $resp.ContentLength",
        "    if ($total -le 0) { $total = 3100000 }",
        "    $inStream = $resp.GetResponseStream()",
        "    $outStream = [System.IO.File]::Create($newExe)",
        "    $buf = New-Object byte[] 32768",
        "    $readTotal = 0",
        "    $lastPct = -1",
        "    while (($r = $inStream.Read($buf, 0, $buf.Length)) -gt 0) {",
        "        $outStream.Write($buf, 0, $r)",
        "        $readTotal += $r",
        "        $pct = [Math]::Min(99, [int](($readTotal * 100) / $total))",
        "        if ($pct -ne $lastPct) {",
        "            $lastPct = $pct",
        "            [Console]::WriteLine('UPD_PROG:' + $pct)",
        "        }",
        "    }",
        "    $outStream.Close()",
        "    $inStream.Close()",
        "    $resp.Close()",
        "} catch {}",
        "if (-not (Test-Path $newExe) -or (Get-Item $newExe).Length -lt 100000) {",
        "    curl.exe -s -L --fail 'https://raw.githubusercontent.com/whrr-fdev/WinWal/main/WinWal-Portable/WinWal.exe' -o $newExe",
        "}",
        "curl.exe -s -L --fail 'https://raw.githubusercontent.com/whrr-fdev/WinWal/main/version.txt' -o $newVer",
        "if ((Test-Path $newExe) -and (Get-Item $newExe).Length -gt 100000) {",
        "    [Console]::WriteLine('UPD_PROG:100')",
        "    $applyPs1 = Join-Path $tmp 'apply_upd.ps1'",
        "    $s1 = 'for ($i = 0; $i -lt 30; $i++) { if (-not (Get-Process -Name \"WinWal\" -ErrorAction SilentlyContinue)) { break }; Start-Sleep -Milliseconds 200 }'",
        "    $s2 = 'Stop-Process -Name \"WinWal\" -Force -ErrorAction SilentlyContinue'",
        "    $s3 = 'Start-Sleep -Milliseconds 700'",
        "    $s4 = 'Copy-Item -Path \"' + $newExe + '\" -Destination \"' + $appExe + '\" -Force'",
        "    $s5 = 'if (Test-Path \"' + $newVer + '\") { Copy-Item -Path \"' + $newVer + '\" -Destination \"' + $appDir + '\\version.txt\" -Force }'",
        "    $s6 = 'Start-Sleep -Milliseconds 300'",
        "    $s7 = 'Start-Process -FilePath \"' + $appExe + '\" -WorkingDirectory \"' + $appDir + '\"'",
        "    Set-Content -Path $applyPs1 -Value ($s1, $s2, $s3, $s4, $s5, $s6, $s7) -Encoding UTF8",
        "    [Console]::WriteLine('UPDATED_OK')",
        "} else {",
        "    [Console]::WriteLine('NO_REPO')",
        "}"
    };

    QString psPath = QDir::tempPath() + "/winwal_run_upd.ps1";
    QFile f(psPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        f.write("\xEF\xBB\xBF");
        f.write(lines.join("\r\n").toUtf8());
        f.close();
    }
    m_updProc->start("powershell.exe", {"-NoProfile", "-ExecutionPolicy", "Bypass", "-WindowStyle", "Hidden", "-File", QDir::toNativeSeparators(psPath)});
}

void HyprIpc::quitApplication() {
    if (onCleanShutdown) {
        onCleanShutdown();
    }
    QCoreApplication::quit();
}

void HyprIpc::playIntro() {
    if (onPlayIntro) onPlayIntro();
}