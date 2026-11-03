#include <thread>
#include "WallpapersManager.h"
#include <algorithm>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QProcess>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QImageReader>
#include <QPainter>
#include <QMutex>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QUrl>
#include <QQuickWindow>
#include <QQmlComponent>
#include <QQmlEngine>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#include <shobjidl.h>
#endif

static QMutex s_thumbMutex;
static QHash<QString, QImage> s_thumbRamCache;

static QString normalizeLocalPath(const QString &raw) {
    QString p = raw.trimmed();
    if (p.startsWith("file:///")) {
#ifdef Q_OS_WIN
        p = p.mid(8);
#else
        p = p.mid(7);
#endif
    }
#ifdef Q_OS_WIN
    if (p.startsWith('/') && p.size() > 2 && p[2] == ':') {
        p = p.mid(1);
    }
    if (p.startsWith("/home/")) {
        int thirdSlash = p.indexOf('/', 6);
        if (thirdSlash != -1) {
            p = QDir::homePath() + p.mid(thirdSlash);
        }
    }
#endif
    return QDir::fromNativeSeparators(p);
}

static QString findPresetLiveVideo(const QStringList &fileNames) {
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList searchDirs = {
        appDir + "/Wallpapers/Live",
        appDir + "/assets/Wallpapers/Live",
        "C:/WinWal/Wallpapers/Live",
        "C:/WinWal/winwal/Wallpapers/Live",
        "C:/WinWal/winwal/assets/Wallpapers/Live",
        "C:/WinWal/WinWal-Portable/Wallpapers/Live",
        QDir::homePath() + "/Pictures/Wallpapers/Live",
        "E:/Wallpapers/Live"
    };
    for (const QString &dir : searchDirs) {
        for (const QString &fn : fileNames) {
            QString cand = dir + "/" + fn;
            if (QFile::exists(cand)) return QDir::fromNativeSeparators(cand);
        }
    }
    return appDir + "/Wallpapers/Live/" + fileNames.first();
}

static QQuickWindow *s_gpuWallWin = nullptr;

#ifdef Q_OS_WIN
static HWND s_cachedWorkerBg = nullptr;

static HWND findDesktopBackgroundWorkerW() {
    if (s_cachedWorkerBg && IsWindow(s_cachedWorkerBg)) return s_cachedWorkerBg;

    HWND hProgman = FindWindowW(L"Progman", nullptr);
    if (!hProgman) return nullptr;

    DWORD_PTR res = 0;
    SendMessageTimeoutW(hProgman, 0x052C, 0, 0, SMTO_NORMAL, 1000, &res);
    SendMessageTimeoutW(hProgman, 0x052C, 0xD, 0x1, SMTO_NORMAL, 1000, &res);

    HWND hWorkerBg = nullptr;
    EnumWindows([](HWND topHwnd, LPARAM lParam) -> BOOL {
        HWND hDefView = FindWindowExW(topHwnd, nullptr, L"SHELLDLL_DefView", nullptr);
        if (hDefView != nullptr) {
            LONG_PTR defParentStyle = GetWindowLongPtrW(topHwnd, GWL_STYLE);
            SetWindowLongPtrW(topHwnd, GWL_STYLE, defParentStyle | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);

            HWND *pOut = reinterpret_cast<HWND*>(lParam);
            *pOut = FindWindowExW(nullptr, topHwnd, L"WorkerW", nullptr);
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&hWorkerBg));

    if (!hWorkerBg) {
        hWorkerBg = FindWindowExW(hProgman, nullptr, L"WorkerW", nullptr);
    }

    if (hProgman) {
        LONG_PTR ps = GetWindowLongPtrW(hProgman, GWL_STYLE);
        SetWindowLongPtrW(hProgman, GWL_STYLE, ps | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
    }
    if (hWorkerBg) {
        LONG_PTR ws = GetWindowLongPtrW(hWorkerBg, GWL_STYLE);
        SetWindowLongPtrW(hWorkerBg, GWL_STYLE, ws | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
    }

    s_cachedWorkerBg = hWorkerBg;
    return hWorkerBg;
}

static bool isWindowAbove(HWND hUpper, HWND hLower) {
    if (!hUpper || !hLower || hUpper == hLower) return false;
    for (HWND cur = GetWindow(hUpper, GW_HWNDNEXT); cur != nullptr; cur = GetWindow(cur, GW_HWNDNEXT)) {
        if (cur == hLower) return true;
    }
    return false;
}

static HWND findLowestNormalAppWindow(HWND hWall, HWND hClock) {
    HWND hTray = FindWindowW(L"Shell_TrayWnd", nullptr);
    HWND hProgman = FindWindowW(L"Progman", nullptr);
    HWND lowestApp = nullptr;

    for (HWND cur = GetTopWindow(nullptr); cur != nullptr; cur = GetWindow(cur, GW_HWNDNEXT)) {
        if (cur == hWall || cur == hClock || cur == hTray || cur == hProgman) continue;
        if (!IsWindowVisible(cur) || IsIconic(cur)) continue;
        LONG_PTR ex = GetWindowLongPtrW(cur, GWL_EXSTYLE);
        if ((ex & WS_EX_TOPMOST) || (ex & WS_EX_TOOLWINDOW) || (ex & WS_EX_NOACTIVATE)) continue;
        wchar_t cls[128] = {};
        GetClassNameW(cur, cls, 127);
        if (wcscmp(cls, L"Progman") == 0 || wcscmp(cls, L"WorkerW") == 0 ||
            wcscmp(cls, L"Shell_TrayWnd") == 0 || wcscmp(cls, L"Windows.UI.Core.CoreWindow") == 0 ||
            wcscmp(cls, L"WinWalDwmGlassSheet") == 0) continue;
        lowestApp = cur;
    }
    return lowestApp;
}

static void syncClockAndWallpaperLayer(HWND hWall) {
    HWND hClock = nullptr;
    for (QWindow *topW : QGuiApplication::topLevelWindows()) {
        if (topW && topW->isVisible() && topW->objectName() == "clockOverlayWin") {
            hClock = reinterpret_cast<HWND>(topW->winId());
            break;
        }
    }
    if (hClock && IsWindow(hClock)) {
        HWND lowestApp = findLowestNormalAppWindow(hWall, hClock);
        if (lowestApp && !isWindowAbove(lowestApp, hClock)) {
            SetWindowPos(hClock, lowestApp, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
        }
    }
}

// Сверхбыстрое извлечение кадра видео через системный кэш и Media Foundation (IShellItemImageFactory)
static QImage extractVideoThumbnailWin32(const QString &videoPath) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IShellItemImageFactory *pImgFactory = nullptr;
    QString nativePath = QDir::toNativeSeparators(videoPath);
    HRESULT hr = SHCreateItemFromParsingName(
        reinterpret_cast<LPCWSTR>(nativePath.utf16()),
        nullptr,
        IID_IShellItemImageFactory,
        reinterpret_cast<void **>(&pImgFactory)
    );
    if (SUCCEEDED(hr) && pImgFactory) {
        SIZE sz = { 240, 135 };
        HBITMAP hBmp = nullptr;
        hr = pImgFactory->GetImage(sz, SIIGBF_THUMBNAILONLY | SIIGBF_BIGGERSIZEOK, &hBmp);
        if (FAILED(hr) || !hBmp) {
            hr = pImgFactory->GetImage(sz, SIIGBF_RESIZETOFIT, &hBmp);
        }
        pImgFactory->Release();
        if (SUCCEEDED(hr) && hBmp) {
            BITMAP bm = {};
            if (GetObject(hBmp, sizeof(bm), &bm) && bm.bmWidth > 0 && bm.bmHeight > 0) {
                QImage res(bm.bmWidth, bm.bmHeight, QImage::Format_RGB32);
                BITMAPINFO bmi = {};
                bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bmi.bmiHeader.biWidth = bm.bmWidth;
                bmi.bmiHeader.biHeight = -bm.bmHeight;
                bmi.bmiHeader.biPlanes = 1;
                bmi.bmiHeader.biBitCount = 32;
                bmi.bmiHeader.biCompression = BI_RGB;
                HDC hdc = GetDC(nullptr);
                GetDIBits(hdc, hBmp, 0, bm.bmHeight, res.bits(), &bmi, DIB_RGB_COLORS);
                ReleaseDC(nullptr, hdc);
                DeleteObject(hBmp);
                return res.scaled(168, 94, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            }
            DeleteObject(hBmp);
        }
    }
    return QImage();
}
#endif

static void attachWindowToDesktopBottom(QWindow *w) {
#ifdef Q_OS_WIN
    if (!w) return;
    QScreen *scr = QGuiApplication::primaryScreen();
    QRect fullRect = scr ? scr->geometry() : QRect(0, 0, 1920, 1080);
    if (w->geometry() != fullRect) w->setGeometry(fullRect);
    if (!w->isVisible()) w->show();

    HWND myHwnd = reinterpret_cast<HWND>(w->winId());
    if (!myHwnd) return;

    SetPropW(myHwnd, L"NonRudeHWND", reinterpret_cast<HANDLE>(1));

    HWND hWorkerBg = findDesktopBackgroundWorkerW();
    if (hWorkerBg && IsWindow(hWorkerBg)) {
        if (GetParent(myHwnd) != hWorkerBg) {
            ShowWindow(hWorkerBg, SW_SHOWNA);

            LONG_PTR st = GetWindowLongPtrW(myHwnd, GWL_STYLE);
            st &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_BORDER);
            st |= (WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
            SetWindowLongPtrW(myHwnd, GWL_STYLE, st);

            LONG_PTR ex = GetWindowLongPtrW(myHwnd, GWL_EXSTYLE);
            ex &= ~(WS_EX_LAYERED | WS_EX_TOPMOST);
            ex |= (WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT);
            SetWindowLongPtrW(myHwnd, GWL_EXSTYLE, ex);

            SetParent(myHwnd, hWorkerBg);

            DWORD myTid = GetCurrentThreadId();
            DWORD expTid = GetWindowThreadProcessId(hWorkerBg, nullptr);
            if (myTid != expTid && expTid != 0) {
                AttachThreadInput(myTid, expTid, FALSE);
            }

            RECT wr{};
            GetClientRect(hWorkerBg, &wr);
            int physW = (wr.right - wr.left) > 200 ? (wr.right - wr.left) : GetSystemMetrics(SM_CXSCREEN);
            int physH = (wr.bottom - wr.top) > 200 ? (wr.bottom - wr.top) : GetSystemMetrics(SM_CYSCREEN);
            SetWindowPos(myHwnd, HWND_TOP, 0, 0, physW, physH,
                         SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
        }
    } else {
        int physW = GetSystemMetrics(SM_CXSCREEN);
        int physH = GetSystemMetrics(SM_CYSCREEN);
        SetWindowPos(myHwnd, HWND_BOTTOM, 0, 0, physW, physH, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
    syncClockAndWallpaperLayer(myHwnd);
#endif
}

static QQuickWindow *ensureGpuWallpaperWindow() {
    if (s_gpuWallWin) return s_gpuWallWin;
    for (QWindow *w : QGuiApplication::topLevelWindows()) {
        if (w->objectName() == "desktopWallWin") {
            s_gpuWallWin = qobject_cast<QQuickWindow *>(w);
            return s_gpuWallWin;
        }
    }
    return nullptr;
}

static QImage loadOrGenerateThumbnail(const QString &rawId) {
    QString srcPath = normalizeLocalPath(rawId);
    if (srcPath.isEmpty() || srcPath.contains("рукиASCII")) return QImage();

    {
        QMutexLocker lock(&s_thumbMutex);
        auto it = s_thumbRamCache.constFind(srcPath);
        if (it != s_thumbRamCache.constEnd()) {
            return it.value();
        }
    }

    QString thumbDir = QDir::homePath() + "/.cache/wallpaper-thumbs-v5";
    QDir().mkpath(thumbDir);

    QByteArray hash = QCryptographicHash::hash(srcPath.toUtf8(), QCryptographicHash::Md5).toHex();
    QString thumbPath = thumbDir + "/th_" + QString(hash.left(14)) + ".jpg";

    QImage img;
    if (QFile::exists(thumbPath)) {
        img.load(thumbPath, "JPG");
    }

    if (img.isNull()) {
        bool isVid = srcPath.endsWith(".mp4", Qt::CaseInsensitive) ||
                     srcPath.endsWith(".webm", Qt::CaseInsensitive) ||
                     srcPath.endsWith(".mkv", Qt::CaseInsensitive);
        if (isVid) {
#ifdef Q_OS_WIN
            img = extractVideoThumbnailWin32(srcPath);
#endif
            if (!img.isNull()) {
                img.save(thumbPath, "JPG", 82);
            }
        } else {
            QImageReader reader(srcPath);
            reader.setAutoTransform(true);
            QSize origSz = reader.size();
            if (origSz.isValid() && origSz.width() > 0 && origSz.height() > 0) {
                QSize targetSz = origSz.scaled(168, 94, Qt::KeepAspectRatioByExpanding);
                reader.setScaledSize(targetSz);
            } else {
                reader.setScaledSize(QSize(168, 94));
            }
            QImage decoded = reader.read();
            if (!decoded.isNull()) {
                img = decoded.scaled(168, 94, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)
                             .convertToFormat(QImage::Format_RGB32);
                img.save(thumbPath, "JPG", 80);
            }
        }
    }

    if (img.isNull()) {
        img = QImage(168, 94, QImage::Format_RGB32);
        img.fill(QColor(24, 26, 34));
        QPainter ptr(&img);
        ptr.setPen(QColor(190, 200, 220));
        QFont f("Segoe UI", 8, QFont::Bold);
        ptr.setFont(f);
        ptr.drawText(img.rect().adjusted(6, 6, -6, -6), Qt::AlignCenter | Qt::TextWordWrap, QFileInfo(srcPath).completeBaseName());
        ptr.end();
        img.save(thumbPath, "JPG", 75);
    } else if (img.size() != QSize(168, 94) || img.format() != QImage::Format_RGB32) {
        img = img.scaled(168, 94, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)
                 .convertToFormat(QImage::Format_RGB32);
    }

    {
        QMutexLocker lock(&s_thumbMutex);
        if (s_thumbRamCache.size() > 24) s_thumbRamCache.clear();
        s_thumbRamCache.insert(srcPath, img);
    }
    return img;
}

WallThumbProvider::WallThumbProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

void WallThumbProvider::clearCache() {
    QMutexLocker lock(&s_thumbMutex);
    s_thumbRamCache.clear();
}

QImage WallThumbProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize) {
    Q_UNUSED(requestedSize);
    QImage img = loadOrGenerateThumbnail(id);
    if (size) *size = img.size();
    return img;
}

WallpapersManager::WallpapersManager(QObject *parent) : QObject(parent) {
    connect(&m_daemonTimer, &QTimer::timeout, this, &WallpapersManager::onAutoWallpaperTick);

#ifdef Q_OS_WIN
    connect(qApp, &QCoreApplication::aboutToQuit, this, []() {
        if (s_gpuWallWin) {
            HWND hWall = reinterpret_cast<HWND>(s_gpuWallWin->winId());
            if (hWall && IsWindow(hWall)) {
                ShowWindow(hWall, SW_HIDE);
                SetParent(hWall, nullptr);
            }
        }
    });

    QTimer *keepBot = new QTimer(this);
    connect(keepBot, &QTimer::timeout, this, []() {
        if (s_gpuWallWin && s_gpuWallWin->isVisible()) {
            HWND hWall = reinterpret_cast<HWND>(s_gpuWallWin->winId());
            if (hWall && IsWindow(hWall)) {
                HWND hParent = GetParent(hWall);
                if (!hParent || !IsWindow(hParent)) {
                    s_cachedWorkerBg = nullptr;
                    attachWindowToDesktopBottom(s_gpuWallWin);
                } else {
                    syncClockAndWallpaperLayer(hWall);
                }

                // Авто-пауза видеообоев, когда рабочий стол закрыт развёрнутым на весь экран окном (0% нагрузки на CPU/GPU!)
                bool mainWinOpen = false;
                for (QWindow *tw : QGuiApplication::topLevelWindows()) {
                    if (tw && tw->isVisible() && tw->objectName() == "mainControlWindow") {
                        mainWinOpen = true;
                        break;
                    }
                }
                bool deskCovered = false;
                if (!mainWinOpen) {
                    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
                        if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) return TRUE;
                        INT cloaked = 0;
                        if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked != 0)
                            return TRUE;
                        LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
                        if ((ex & WS_EX_TRANSPARENT) || (ex & WS_EX_TOOLWINDOW)) return TRUE;
                        wchar_t cls[64] = {};
                        GetClassNameW(hwnd, cls, 63);
                        if (wcscmp(cls, L"Progman") == 0 || wcscmp(cls, L"WorkerW") == 0 ||
                            wcscmp(cls, L"Shell_TrayWnd") == 0 || wcscmp(cls, L"Windows.UI.Core.CoreWindow") == 0)
                            return TRUE;
                        if (IsZoomed(hwnd)) {
                            *reinterpret_cast<bool *>(lParam) = true;
                            return FALSE;
                        }
                        RECT wr = {};
                        GetWindowRect(hwnd, &wr);
                        int ww = wr.right - wr.left, wh = wr.bottom - wr.top;
                        int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
                        if (ww * wh >= int(sw * sh * 0.75)) {
                            *reinterpret_cast<bool *>(lParam) = true;
                            return FALSE;
                        }
                        return TRUE;
                    }, reinterpret_cast<LPARAM>(&deskCovered));
                }
                QMetaObject::invokeMethod(s_gpuWallWin, "setVideoPausedByMaximized", Q_ARG(QVariant, deskCovered));
            }
        }
    });
    keepBot->start(600);
#endif

    loadDaemonSettings();
    loadPresets();
    scanWallpapers();
    updateDaemonTimer();

    // Автоматически запускаем проверку/докачку обоев с GitHub при каждом старте
    QTimer::singleShot(9500, this, &WallpapersManager::downloadGithubWallpapers);

    QTimer::singleShot(450, this, [this]() {
        QString lastW;
        QFile f(QDir::homePath() + "/.config/hypr/scripts/last_wallpaper.conf");
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            lastW = normalizeLocalPath(QTextStream(&f).readLine().trimmed());
        }
        // Если после чистой установки обои ещё не выбирались — включаем пресет «Сакура (Live)» (индекс 1)!
        if (lastW.isEmpty() || !QFile::exists(lastW)) {
            applyPreset(1);
            return;
        }
        bool isVid = lastW.endsWith(".mp4", Qt::CaseInsensitive) ||
                     lastW.endsWith(".webm", Qt::CaseInsensitive) ||
                     lastW.endsWith(".mkv", Qt::CaseInsensitive);
        if (isVid) {
            applyWallpaperInternal(lastW, m_activePresetIndex >= 0);
        } else if (QQuickWindow *gpuWin = ensureGpuWallpaperWindow()) {
            QMetaObject::invokeMethod(gpuWin, "initCurrentImage",
                                      Q_ARG(QVariant, QUrl::fromLocalFile(lastW).toString()));
        }
    });
}

void WallpapersManager::downloadGithubWallpapers() {
    if (m_downloading) return;
    m_downloading = true;
    m_downloadStatus = "󰇚  Подключение к GitHub...";
    emit downloadStatusChanged();

    if (!m_dlProcess) {
        m_dlProcess = new QProcess(this);
        connect(m_dlProcess, &QProcess::readyReadStandardOutput, this, [this]() {
            while (m_dlProcess->canReadLine()) {
                QString line = QString::fromUtf8(m_dlProcess->readLine()).trimmed();
                if (line.startsWith("PROGRESS:")) {
                    QStringList parts = line.mid(9).split('/');
                    if (parts.size() == 2) {
                        int cur = parts[0].toInt();
                        int tot = std::max(1, parts[1].toInt());
                        int pct = (cur * 100) / tot;
                        m_downloadStatus = QString("󰇚  Загрузка: %1 / %2 (%3%)").arg(cur).arg(tot).arg(pct);
                        emit downloadStatusChanged();
                        
                    }
                }
            }
        });
        connect(m_dlProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this](int exitCode, QProcess::ExitStatus) {
            Q_UNUSED(exitCode);
            m_downloading = false;
            scanWallpapers();
            m_downloadStatus = m_wallpapers.isEmpty()
                               ? "󰇚  Ошибка сети (повторить)"
                               : QString("✓  Обои загружены (%1 шт.)").arg(m_wallpapers.size());
            emit downloadStatusChanged();
            QTimer::singleShot(5000, this, [this]() {
                if (!m_downloading) {
                    m_downloadStatus.clear();
                    emit downloadStatusChanged();
                }
            });
        });
    }

    QString targetDir = QDir::toNativeSeparators(QDir::homePath() + "/Pictures/Wallpapers");

    QString psScript = QString(
        "$ErrorActionPreference = 'SilentlyContinue'; "
        "[Console]::OutputEncoding = [System.Text.Encoding]::UTF8; "
        "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; "
        "$dest = '%1'; "
        "New-Item -ItemType Directory -Force -Path $dest | Out-Null; "
        "$tree = Invoke-RestMethod -Uri 'https://api.github.com/repos/whrr-fdev/wallpapers/git/trees/HEAD?recursive=1' -Headers @{'User-Agent'='WinWal-Client'}; "
        "if ($tree -and $tree.tree) { "
        "    $files = @($tree.tree | Where-Object { $_.type -eq 'blob' -and $_.path -match '\\.(jpg|jpeg|png|webp|bmp|mp4|webm|mkv)$' }); "
        "    $tot = $files.Count; "
        "    $cur = 0; "
        "    foreach ($item in $files) { "
        "        $cur++; "
        "        $relPath = $item.path -replace '/', '\\'; "
        "        $outPath = Join-Path $dest $relPath; "
        "        $parentDir = Split-Path $outPath -Parent; "
        "        if (-not (Test-Path $parentDir)) { New-Item -ItemType Directory -Force -Path $parentDir | Out-Null; } "
        "        if (-not (Test-Path $outPath) -or (Get-Item $outPath).Length -lt 100) { "
        "            $segments = $item.path -split '/' | ForEach-Object { [uri]::EscapeDataString($_) }; "
        "            $rawUrl = 'https://raw.githubusercontent.com/whrr-fdev/wallpapers/HEAD/' + ($segments -join '/'); "
        "            curl.exe -s -L --fail $rawUrl -o $outPath; "
        "        } "
        "        [Console]::WriteLine('PROGRESS:' + $cur + '/' + $tot); "
        "    } "
        "    if ($tot -gt 0) { Set-Content -Path (Join-Path $dest '.gh_whrr_wallpapers_v2.ok') -Value 'ok'; } "
        "} else { "
        "    $tmpDir = Join-Path $env:TEMP 'winwal_gh_wallpapers'; "
        "    New-Item -ItemType Directory -Force -Path $tmpDir | Out-Null; "
        "    $zipPath = Join-Path $tmpDir 'repo.zip'; "
        "    curl.exe -s -L --fail 'https://codeload.github.com/whrr-fdev/wallpapers/zip/refs/heads/main' -o $zipPath; "
        "    if (-not (Test-Path $zipPath) -or (Get-Item $zipPath).Length -lt 500) { "
        "        curl.exe -s -L --fail 'https://codeload.github.com/whrr-fdev/wallpapers/zip/refs/heads/master' -o $zipPath; "
        "    } "
        "    if ((Test-Path $zipPath) -and (Get-Item $zipPath).Length -gt 500) { "
        "        tar.exe -xf $zipPath -C $tmpDir; "
        "        $extractedRoot = Get-ChildItem -Path $tmpDir -Directory | Select-Object -First 1; "
        "        if ($extractedRoot) { "
        "            Get-ChildItem -Path $extractedRoot.FullName -Force | Where-Object { $_.Name -notlike '.*' -and $_.Name -notlike 'README*' } | ForEach-Object { "
        "                Copy-Item -Path $_.FullName -Destination $dest -Recurse -Force; "
        "            }; "
        "            Set-Content -Path (Join-Path $dest '.gh_whrr_wallpapers_v2.ok') -Value 'ok'; "
        "        } "
        "    } "
        "    Remove-Item -Recurse -Force $tmpDir -ErrorAction SilentlyContinue; "
        "}"
    ).arg(targetDir);

    QString psFilePath = QDir::tempPath() + "/winwal_gh_dl.ps1";
    QFile psFile(psFilePath);
    if (psFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        psFile.write("\xEF\xBB\xBF");
        psFile.write(psScript.toUtf8());
        psFile.close();
    }
    m_dlProcess->start("powershell.exe", {"-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-WindowStyle", "Hidden", "-File", QDir::toNativeSeparators(psFilePath)});
}

void WallpapersManager::updateDaemonTimer() {
    if (m_daemonRunning && m_autoInterval > 0) {
        m_daemonTimer.start(std::max(10, m_autoInterval) * 1000);
    } else {
        m_daemonTimer.stop();
    }
}

void WallpapersManager::onAutoWallpaperTick() {
    if (m_wallpapers.isEmpty()) return;
    QVariantList filtered;
    for (const QVariant &v : m_wallpapers) {
        QVariantMap m = v.toMap();
        bool isVid = m["isVideo"].toBool();
        if (m_autoMode == "video" && !isVid) continue;
        if (m_autoMode == "image" && isVid) continue;
        filtered.append(m);
    }
    if (filtered.isEmpty()) filtered = m_wallpapers;
    int idx = QRandomGenerator::global()->bounded(filtered.size());
    applyWallpaperInternal(filtered[idx].toMap()["path"].toString(), false);
}

void WallpapersManager::setDaemonRunning(bool v) {
    if (m_daemonRunning == v) return;
    m_daemonRunning = v;
    emit daemonChanged();
    QDir().mkpath(QDir::homePath() + "/.config/hypr/scripts");
    QFile f(QDir::homePath() + "/.config/hypr/scripts/auto_wallpaper.conf");
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) QTextStream(&f) << (v ? "1" : "0") << "\n";
    updateDaemonTimer();
}

void WallpapersManager::loadDaemonSettings() {
    QString home = QDir::homePath();
    QFile fState(home + "/.config/hypr/scripts/auto_wallpaper.conf");
    if (fState.open(QIODevice::ReadOnly | QIODevice::Text)) m_daemonRunning = (QTextStream(&fState).readLine().trimmed() == "1");
    QFile fMode(home + "/.config/hypr/scripts/auto_mode.conf");
    if (fMode.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_autoMode = QTextStream(&fMode).readLine().trimmed();
        if (m_autoMode.isEmpty()) m_autoMode = "all";
    }
    QFile fInt(home + "/.config/hypr/scripts/auto_interval.conf");
    if (fInt.open(QIODevice::ReadOnly | QIODevice::Text)) {
        int val = QTextStream(&fInt).readLine().trimmed().toInt();
        if (val > 0) m_autoInterval = val;
    }
    QFile fAnim(home + "/.config/hypr/scripts/random_anim.conf");
    if (fAnim.open(QIODevice::ReadOnly | QIODevice::Text)) m_randomAnim = (QTextStream(&fAnim).readLine().trimmed() == "1");
}

void WallpapersManager::setAutoMode(const QString &v) {
    if (m_autoMode == v) return;
    m_autoMode = v;
    QFile f(QDir::homePath() + "/.config/hypr/scripts/auto_mode.conf");
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) QTextStream(&f) << v << "\n";
    emit daemonChanged();
}

void WallpapersManager::setAutoInterval(int v) {
    if (m_autoInterval == v) return;
    m_autoInterval = v;
    QFile f(QDir::homePath() + "/.config/hypr/scripts/auto_interval.conf");
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) QTextStream(&f) << v << "\n";
    updateDaemonTimer();
    emit daemonChanged();
}

void WallpapersManager::setRandomAnim(bool v) {
    if (m_randomAnim == v) return;
    m_randomAnim = v;
    QString home = QDir::homePath();
    for (const QString &fn : {"random_anim.conf", "auto_anim.conf"}) {
        QFile f(home + "/.config/hypr/scripts/" + fn);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) QTextStream(&f) << (v ? "1" : "0") << "\n";
    }
    emit animChanged();
}

void WallpapersManager::refreshWallpapers() { scanWallpapers(); }

void WallpapersManager::scanWallpapers() {
    m_categories.clear();
    m_wallpapers.clear();

    QStringList primaryRoots = {
        QCoreApplication::applicationDirPath() + "/Wallpapers",
        QCoreApplication::applicationDirPath() + "/assets/Wallpapers",
        "C:/WinWal/Wallpapers",
        "C:/WinWal/winwal/Wallpapers",
        QDir::homePath() + "/Pictures/Wallpapers"
    };

    QStringList imgExts = {"*.jpg", "*.jpeg", "*.png", "*.webp", "*.bmp", "*.mp4", "*.webm", "*.mkv"};
    QMap<QString, QVariantList> catWallpapers;
    QStringList catOrder;
    QSet<QString> seenFiles;

    auto addFileToCategory = [&](const QString &filePath, const QString &catName) {
        QFileInfo fi(filePath);
        if (!fi.exists() || fi.size() < 100 || fi.fileName().contains("рукиASCII")) return;
        QString dedupKey = catName.toLower() + "/" + fi.fileName().toLower();
        if (seenFiles.contains(dedupKey)) return;
        seenFiles.insert(dedupKey);

        QString fullPath = QDir::fromNativeSeparators(fi.absoluteFilePath());
        if (!fullPath.startsWith('/')) fullPath = "/" + fullPath;
        bool isVid = fullPath.endsWith(".mp4", Qt::CaseInsensitive) ||
                     fullPath.endsWith(".webm", Qt::CaseInsensitive) ||
                     fullPath.endsWith(".mkv", Qt::CaseInsensitive);

        QVariantMap w;
        w["path"] = fullPath;
        w["name"] = fi.fileName();
        w["category"] = catName;
        w["isVideo"] = isVid;

        if (!catWallpapers.contains(catName)) {
            catOrder.append(catName);
        }
        catWallpapers[catName].append(w);
    };

    auto scanRootDir = [&](const QString &rootPath) {
        QDir rootDir(rootPath);
        if (!rootDir.exists()) return;

        QStringList subDirs = rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &sub : subDirs) {
            if (sub.startsWith('.')) continue;
            QDirIterator it(rootDir.filePath(sub), imgExts, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                addFileToCategory(it.next(), sub);
            }
        }

        QStringList rootFiles = rootDir.entryList(imgExts, QDir::Files);
        for (const QString &rf : rootFiles) {
            QString fp = rootDir.filePath(rf);
            bool isVid = fp.endsWith(".mp4", Qt::CaseInsensitive) ||
                         fp.endsWith(".webm", Qt::CaseInsensitive) ||
                         fp.endsWith(".mkv", Qt::CaseInsensitive);
            addFileToCategory(fp, isVid ? "Live" : "All");
        }
    };

    for (const QString &r : primaryRoots) {
        scanRootDir(r);
    }

    if (catWallpapers.isEmpty()) {
        scanRootDir("C:/Windows/Web/Wallpaper");
    }

    std::stable_sort(catOrder.begin(), catOrder.end(), [](const QString &a, const QString &b) {
        return (a.compare("Live", Qt::CaseInsensitive) == 0) >
               (b.compare("Live", Qt::CaseInsensitive) == 0);
    });

    QStringList allPathsForPrewarm;
    for (const QString &cName : catOrder) {
        const QVariantList &list = catWallpapers[cName];
        if (list.isEmpty()) continue;
        QVariantMap cat;
        cat["name"] = cName;
        cat["count"] = list.size();
        m_categories.append(cat);
        for (const QVariant &item : list) {
            m_wallpapers.append(item);
            allPathsForPrewarm.append(item.toMap()["path"].toString());
        }
    }

    emit categoriesChanged();
    emit wallpapersChanged();

    // Фоновый предпрогрев миниатюр (видео идут первыми!), чтобы при открытии вкладки «Обои» всё уже было в RAM
    if (!allPathsForPrewarm.isEmpty()) {
        std::thread([allPathsForPrewarm]() {
            int cnt = 0;
            for (const QString &p : allPathsForPrewarm) {
                loadOrGenerateThumbnail(p);
                if (++cnt > 18) {
                    WallThumbProvider::clearCache();
                    cnt = 0;
                }
            }
        }).detach();
    }
}

void WallpapersManager::applyWallpaperInternal(const QString &rawPath, bool fromPreset) {
    QString path = normalizeLocalPath(rawPath);
    if ((path.isEmpty() || !QFile::exists(path)) && fromPreset) {
        if (rawPath.contains("grass", Qt::CaseInsensitive)) {
            path = findPresetLiveVideo({"wavy-grass-moewalls-com.mp4", "grass.mp4"});
        } else if (rawPath.contains("cherry", Qt::CaseInsensitive) || rawPath.contains("tsushima", Qt::CaseInsensitive)) {
            path = findPresetLiveVideo({"cherry-blossom-ghost-of-tsushima-moewalls-com.mp4"});
        }
        if (path.isEmpty() || !QFile::exists(path)) {
            for (const QVariant &v : m_wallpapers) {
                QVariantMap wm = v.toMap();
                if (wm["isVideo"].toBool()) {
                    QString cand = normalizeLocalPath(wm["path"].toString());
                    if (QFile::exists(cand)) { path = cand; break; }
                }
            }
        }
    }
    if (path.isEmpty() || !QFile::exists(path)) return;
    if (!fromPreset && m_activePresetIndex != -1) { m_activePresetIndex = -1; emit activePresetChanged(); }

    QString home = QDir::homePath();
    QDir().mkpath(home + "/.config/hypr/scripts");
    QDir().mkpath(home + "/.cache/wallpaper-thumbs-v5");

    for (const QString &fn : {home + "/.config/hypr/scripts/last_wallpaper.conf", home + "/.cache/last_wallpaper.txt"}) {
        QFile f(fn);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) QTextStream(&f) << path << "\n";
    }

    bool isVid = path.endsWith(".mp4", Qt::CaseInsensitive) ||
                 path.endsWith(".webm", Qt::CaseInsensitive) ||
                 path.endsWith(".mkv", Qt::CaseInsensitive);

    QQuickWindow *gpuWin = ensureGpuWallpaperWindow();
    int animKind = m_randomAnim ? QRandomGenerator::global()->bounded(3) : 0;

    if (isVid) {
        if (gpuWin) {
            attachWindowToDesktopBottom(gpuWin);
            QMetaObject::invokeMethod(gpuWin, "playVideoWallpaper",
                                      Q_ARG(QVariant, QUrl::fromLocalFile(path).toString()),
                                      Q_ARG(QVariant, animKind));
        }
        return;
    }

    if (gpuWin) {
        attachWindowToDesktopBottom(gpuWin);
        QMetaObject::invokeMethod(gpuWin, "playImageTransition",
                                  Q_ARG(QVariant, QUrl::fromLocalFile(path).toString()),
                                  Q_ARG(QVariant, animKind));
    }

#ifdef Q_OS_WIN
    QString targetImgPath = path;
    if (path.endsWith(".webp", Qt::CaseInsensitive)) {
        QImage img(path);
        if (!img.isNull()) {
            QString bmpPath = home + "/.cache/winwal_converted.bmp";
            if (img.save(bmpPath, "BMP")) targetImgPath = bmpPath;
        }
    }
    QString winPath = QDir::toNativeSeparators(targetImgPath);
    QTimer::singleShot(850, this, [winPath]() {
        std::thread([winPath]() {
            SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, (void*)winPath.utf16(), SPIF_UPDATEINIFILE);
        }).detach();
    });
#endif
}

void WallpapersManager::applyWallpaper(const QString &path) { applyWallpaperInternal(path, false); }

void WallpapersManager::clearWallpaper() {
    if (m_daemonRunning) setDaemonRunning(false);
    if (auto *gw = ensureGpuWallpaperWindow()) QMetaObject::invokeMethod(gw, "stopAll");
    if (m_activePresetIndex != -1) { m_activePresetIndex = -1; emit activePresetChanged(); savePresetsToFile(); }
    QString home = QDir::homePath();
    QDir().mkpath(home + "/.cache");
    QString blackImg = home + "/.cache/wallpaper-black.bmp";
    if (!QFile::exists(blackImg)) {
        QImage img(64, 64, QImage::Format_RGB32);
        img.fill(QColor(8, 8, 10));
        img.save(blackImg, "BMP");
    }
#ifdef Q_OS_WIN
    QString winPath = QDir::toNativeSeparators(blackImg);
    QTimer::singleShot(100, this, [winPath]() {
        std::thread([winPath]() {
            SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, (void*)winPath.utf16(), SPIF_UPDATEINIFILE);
        }).detach();
    });
#endif
}

void WallpapersManager::applyRandomWallpaper() { onAutoWallpaperTick(); }

void WallpapersManager::loadPresets() {
    m_presets.clear();
    QString defGrass = findPresetLiveVideo({"wavy-grass-moewalls-com.mp4", "grass.mp4"});
    QString defCherry = findPresetLiveVideo({"cherry-blossom-ghost-of-tsushima-moewalls-com.mp4"});

    auto makeDef = [&](int i) {
        QVariantMap m;
        m["name"] = (i == 1) ? "󰌪 Трава (Live)" : "󰉊 Сакура (Live)";
        m["wall"] = (i == 1) ? defGrass : defCherry;
        m["active_opacity"] = 0.85;
        m["inactive_opacity"] = (i == 1) ? 0.75 : 0.65;
        m["blur_enabled"] = true;
        m["blur_size"] = (i == 1) ? 8.0 : 5.0;
        m["canDelete"] = false;
        return m;
    };
    QMap<int, QVariantMap> loaded;
    loaded[1] = makeDef(1);
    loaded[2] = makeDef(2);

    QString path = QDir::homePath() + "/.config/hypr/scripts/presets.conf";
    QFile f(path);
    bool needsResave = false;
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&f);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (!line.contains('=')) continue;
            int eq = line.indexOf('=');
            QString k = line.left(eq).trimmed();
            QString v = line.mid(eq + 1).trimmed();
            if (k == "active_preset") { m_activePresetIndex = v.toInt(); continue; }
            if (k.startsWith('p')) {
                int us = k.indexOf('_');
                if (us > 1) {
                    int idx = k.mid(1, us - 1).toInt();
                    QString field = k.mid(us + 1);
                    if (idx >= 1) {
                        if (!loaded.contains(idx)) {
                            QVariantMap custom;
                            custom["name"] = QString("Пресет %1").arg(idx);
                            custom["wall"] = "";
                            custom["active_opacity"] = 0.85;
                            custom["inactive_opacity"] = 0.75;
                            custom["blur_enabled"] = true;
                            custom["blur_size"] = 8.0;
                            custom["canDelete"] = (idx > 2);
                            loaded[idx] = custom;
                        }
                        QVariantMap p = loaded[idx];
                        if (field == "name") p["name"] = v;
                        else if (field == "wall") {
                            QString normV = normalizeLocalPath(v);
                            if (idx == 1 && (!QFile::exists(normV) || normV.endsWith("/grass.mp4", Qt::CaseInsensitive))) {
                                p["wall"] = defGrass;
                                needsResave = true;
                            } else if (idx == 2 && !QFile::exists(normV)) {
                                p["wall"] = defCherry;
                                needsResave = true;
                            } else {
                                p["wall"] = normV;
                            }
                        }
                        else if (field == "active_opacity") p["active_opacity"] = v.toDouble();
                        else if (field == "inactive_opacity") p["inactive_opacity"] = v.toDouble();
                        else if (field == "blur_enabled") p["blur_enabled"] = (v.toDouble() >= 0.5);
                        else if (field == "blur_size") p["blur_size"] = v.toDouble();
                        p["canDelete"] = (idx > 2);
                        loaded[idx] = p;
                    }
                }
            }
        }
        f.close();
    }
    for (auto it = loaded.begin(); it != loaded.end(); ++it) m_presets.append(it.value());
    if (needsResave) savePresetsToFile();
    emit presetsChanged();
}

void WallpapersManager::savePresetsToFile() {
    QDir().mkpath(QDir::homePath() + "/.config/hypr/scripts");
    QString path = QDir::homePath() + "/.config/hypr/scripts/presets.conf";
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&f);
        out << "active_preset=" << m_activePresetIndex << "\n\n";
        for (int i = 0; i < m_presets.size(); ++i) {
            QVariantMap p = m_presets[i].toMap();
            int idx = i + 1;
            out << QString("p%1_name=%2\n").arg(idx).arg(p["name"].toString());
            out << QString("p%1_wall=%2\n").arg(idx).arg(p["wall"].toString());
            out << QString("p%1_active_opacity=%2\n").arg(idx).arg(p["active_opacity"].toDouble());
            out << QString("p%1_inactive_opacity=%2\n").arg(idx).arg(p["inactive_opacity"].toDouble());
            out << QString("p%1_blur_enabled=%2\n").arg(idx).arg(p["blur_enabled"].toBool() ? 1.0 : 0.0);
            out << QString("p%1_blur_size=%2\n\n").arg(idx).arg(p["blur_size"].toDouble());
        }
    }
}

void WallpapersManager::applyPreset(int idx) {
    if (idx < 0 || idx >= m_presets.size()) return;
    QVariantMap p = m_presets[idx].toMap();
    m_activePresetIndex = idx;
    emit activePresetChanged();
    savePresetsToFile();
    setDaemonRunning(false);
    applyWallpaperInternal(p["wall"].toString(), true);
}

void WallpapersManager::savePreset(int idx, const QString &name, const QString &wall, qreal actOp, qreal inactOp, bool blurEn, qreal blurSz) {
    QVariantMap p;
    p["name"] = name; p["wall"] = wall;
    p["active_opacity"] = actOp; p["inactive_opacity"] = inactOp;
    p["blur_enabled"] = blurEn; p["blur_size"] = blurSz;
    if (idx >= 0 && idx < m_presets.size()) {
        p["canDelete"] = (idx >= 2);
        m_presets[idx] = p;
    } else {
        p["canDelete"] = true;
        m_presets.append(p);
    }
    savePresetsToFile();
    emit presetsChanged();
}

void WallpapersManager::deletePreset(int idx) {
    if (idx < 2 || idx >= m_presets.size()) return;
    if (m_activePresetIndex == idx) { m_activePresetIndex = -1; emit activePresetChanged(); }
    else if (m_activePresetIndex > idx) { m_activePresetIndex--; emit activePresetChanged(); }
    m_presets.removeAt(idx);
    savePresetsToFile();
    emit presetsChanged();
}

QVariantMap WallpapersManager::snapshotCurrentSettings() {
    QVariantMap m;
    QFile f(QDir::homePath() + "/.config/hypr/scripts/last_wallpaper.conf");
    m["wall"] = f.open(QIODevice::ReadOnly | QIODevice::Text) ? QTextStream(&f).readLine().trimmed() : "";
    return m;
}
void WallpapersManager::shutdownRestoreDesktop() {
    m_daemonTimer.stop();
    if (s_gpuWallWin) {
        QMetaObject::invokeMethod(s_gpuWallWin, "stopAll", Qt::DirectConnection);
#ifdef Q_OS_WIN
        HWND hWall = reinterpret_cast<HWND>(s_gpuWallWin->winId());
        if (hWall && IsWindow(hWall)) {
            ShowWindow(hWall, SW_HIDE);
            SetParent(hWall, nullptr);
        }
#endif
        s_gpuWallWin->hide();
    }
#ifdef Q_OS_WIN
    SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, nullptr, SPIF_SENDCHANGE);
    if (HWND hProgman = FindWindowW(L"Progman", nullptr)) {
        RedrawWindow(hProgman, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }
#endif
}