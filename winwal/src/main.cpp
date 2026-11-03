#include <thread>
#include <atomic>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QLoggingCategory>
#include <QPixmapCache>
#include <QFontDatabase>
#include <QFont>
#include <QIcon>
#include <QTimer>
#include <QElapsedTimer>
#include <QFile>
#include <QDir>
#include <QVector>
#include <QImage>
#include <QPainter>
#include <QAbstractNativeEventFilter>
#include <cmath>
#include <algorithm>

#include "ThemeEngine.h"
#include "HyprIpc.h"
#include "AppInstance.h"
#include "AppConfig.h"
#include "LookManager.h"
#include "WidgetsManager.h"
#include "WallpapersManager.h"
#include "AtmosphereManager.h"
#include "TaskbarManager.h"

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <psapi.h>
#include <dwmapi.h>

// Плавная тёмная завеса экрана в отдельном независимом потоке (0% фризов и 0% мигания таскбара)
std::atomic<bool> g_introCurtainOpaque{false};
std::atomic<bool> g_introThreadRunning{false};

static void runStartupCurtainThreadFunc(QString greetingText) {
    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);
    if (scrW <= 100 || scrH <= 100) {
        g_introCurtainOpaque = false;
        g_introThreadRunning = false;
        return;
    }

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"WinWalStartupCurtainThreadCls";
    RegisterClassExW(&wc);

    int txtW = 860;
    int txtH = 250;
    int textBaseX = (scrW - txtW) / 2;
    int textBaseY = (scrH - txtH) / 2;

    HWND hCurtain = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST,
        L"WinWalStartupCurtainThreadCls", L"", WS_POPUP,
        0, 0, scrW, scrH, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

    HWND hTextWin = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST,
        L"WinWalStartupCurtainThreadCls", L"", WS_POPUP,
        textBaseX, textBaseY, txtW, txtH, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

    HDC hdcScreen = GetDC(nullptr);

    // Загружаем чёрный битмап (#050608) в hCurtain
    {
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = scrW;
        bmi.bmiHeader.biHeight = -scrH;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        void *pvBits = nullptr;
        HBITMAP hBmp = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pvBits, nullptr, 0);
        if (hBmp && pvBits) {
            DWORD *px = static_cast<DWORD *>(pvBits);
            size_t total = size_t(scrW) * size_t(scrH);
            for (size_t i = 0; i < total; ++i) px[i] = 0xFF050608u;
            HGDIOBJ old = SelectObject(hdcMem, hBmp);
            POINT ptDst = { 0, 0 };
            SIZE szDst = { scrW, scrH };
            POINT ptSrc = { 0, 0 };
            BLENDFUNCTION bf = { AC_SRC_OVER, 0, 15, AC_SRC_ALPHA };
            UpdateLayeredWindow(hCurtain, hdcScreen, &ptDst, &szDst, hdcMem, &ptSrc, 0, &bf, ULW_ALPHA);
            SelectObject(hdcMem, old);
            DeleteObject(hBmp);
        }
        DeleteDC(hdcMem);
    }

    // Создаём DIB-буфер для плавной 75 FPS отрисовки текста и 3 точек
    HDC hdcTextMem = CreateCompatibleDC(hdcScreen);
    BITMAPINFO bmiTxt = {};
    bmiTxt.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmiTxt.bmiHeader.biWidth = txtW;
    bmiTxt.bmiHeader.biHeight = -txtH;
    bmiTxt.bmiHeader.biPlanes = 1;
    bmiTxt.bmiHeader.biBitCount = 32;
    bmiTxt.bmiHeader.biCompression = BI_RGB;
    void *pTextBits = nullptr;
    HBITMAP hTextDib = CreateDIBSection(hdcTextMem, &bmiTxt, DIB_RGB_COLORS, &pTextBits, nullptr, 0);
    HGDIOBJ hOldTextBmp = hTextDib ? SelectObject(hdcTextMem, hTextDib) : nullptr;

    SetWindowPos(hCurtain, HWND_TOPMOST, 0, 0, scrW, scrH, SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING);
    SetWindowPos(hTextWin, HWND_TOPMOST, textBaseX, textBaseY, txtW, txtH, SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING);

    QFont mainFont("Segoe UI", 30, QFont::Light);
    mainFont.setPixelSize(46);
    mainFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.4);

    QFont subFont("Segoe UI", 10, QFont::Normal);
    subFont.setPixelSize(15);
    subFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.7);

    QElapsedTimer clock;
    clock.start();

    const double fadeInMs  = 500.0;
    const double holdMs    = 7200.0;
    const double fadeOutMs = 1500.0;
    bool taskbarHiddenForIntro = false;
    bool taskbarRestoredForFade = false;

    while (true) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        qint64 ms = clock.elapsed();

        // Когда экран полностью потемнел (500 мс) — прячем панель задач, чтобы она вообще не могла мигать!
        if (ms >= 480 && ms < holdMs - 380) {
            if (!taskbarHiddenForIntro) {
                taskbarHiddenForIntro = true;
                g_introCurtainOpaque = true;
                if (HWND hTray = FindWindowW(L"Shell_TrayWnd", nullptr)) ShowWindow(hTray, SW_HIDE);
                if (HWND hWs = FindWindowW(L"WinWalWorkspacePill", nullptr)) ShowWindow(hWs, SW_HIDE);
                if (HWND hPb = FindWindowW(L"WinWalPillBackdropStrip", nullptr)) ShowWindow(hPb, SW_HIDE);
            }
        }

        // За 380 мс до начала растворения чёрного экрана включаем таскбар обратно ПОД чёрной завесой
        if (ms >= holdMs - 380 && !taskbarRestoredForFade) {
            taskbarRestoredForFade = true;
            g_introCurtainOpaque = false;
            if (HWND hTray = FindWindowW(L"Shell_TrayWnd", nullptr)) ShowWindow(hTray, SW_SHOWNA);
        }

        if (ms < holdMs) {
            BYTE bgAlpha = 255;
            if (ms < fadeInMs) {
                double p = double(ms) / fadeInMs;
                double ease = 1.0 - std::pow(1.0 - p, 3.0);
                bgAlpha = static_cast<BYTE>(std::clamp(int(std::round(15.0 + 240.0 * ease)), 15, 255));
            }
            POINT ptBg = { 0, 0 };
            BLENDFUNCTION bfBg = { AC_SRC_OVER, 0, bgAlpha, AC_SRC_ALPHA };
            UpdateLayeredWindow(hCurtain, nullptr, &ptBg, nullptr, nullptr, nullptr, 0, &bfBg, ULW_ALPHA);

            if (pTextBits) {
                double titleAlpha = 0.0;
                double titleOffsetY = 26.0;
                if (ms >= 380 && ms < 1680) {
                    double p = double(ms - 380) / 1300.0;
                    double inv = 1.0 - p;
                    double ease = 1.0 - (inv * inv * inv * inv);
                    titleAlpha = ease;
                    titleOffsetY = 26.0 * (1.0 - ease);
                } else if (ms >= 1680 && ms < 6250) {
                    titleAlpha = 1.0;
                    titleOffsetY = 0.0;
                } else if (ms >= 6250 && ms < 7150) {
                    double p = double(ms - 6250) / 900.0;
                    titleAlpha = std::clamp(1.0 - p * p, 0.0, 1.0);
                    titleOffsetY = -16.0 * p;
                }

                double subAlpha = 0.0;
                double subOffsetY = 18.0;
                if (ms >= 1000 && ms < 2100) {
                    double p = double(ms - 1000) / 1100.0;
                    double inv = 1.0 - p;
                    double ease = 1.0 - (inv * inv * inv);
                    subAlpha = ease;
                    subOffsetY = 18.0 * (1.0 - ease);
                } else if (ms >= 2100 && ms < 6150) {
                    subAlpha = 1.0;
                    subOffsetY = 0.0;
                } else if (ms >= 6150 && ms < 7050) {
                    double p = double(ms - 6150) / 900.0;
                    subAlpha = std::clamp(1.0 - p * p, 0.0, 1.0);
                    subOffsetY = -12.0 * p;
                }

                QImage img(reinterpret_cast<uchar *>(pTextBits), txtW, txtH, QImage::Format_ARGB32_Premultiplied);
                img.fill(Qt::transparent);

                if (titleAlpha > 0.005 || subAlpha > 0.005) {
                    QPainter p(&img);
                    p.setRenderHint(QPainter::Antialiasing, true);
                    p.setRenderHint(QPainter::TextAntialiasing, true);

                    if (titleAlpha > 0.005) {
                        p.setFont(mainFont);
                        int ta = std::clamp(int(std::round(titleAlpha * 255.0)), 0, 255);
                        p.setPen(QColor(255, 255, 255, ta));
                        p.drawText(QRectF(0, 32.0 + titleOffsetY, txtW, 76.0), Qt::AlignCenter, greetingText);
                    }

                    if (subAlpha > 0.005) {
                        p.setFont(subFont);
                        int dotStep = int((ms / 420) % 4);
                        QString dotsStr = (dotStep == 1) ? "." : ((dotStep == 2) ? ".." : ((dotStep == 3) ? "..." : ""));
                        QString subStr = QStringLiteral("Подготавливаем рабочее окружение WinWal") + dotsStr;

                        int sa = std::clamp(int(std::round(subAlpha * 195.0)), 0, 255);
                        p.setPen(QColor(158, 168, 186, sa));
                        p.drawText(QRectF(0, 118.0 + subOffsetY, txtW, 34.0), Qt::AlignCenter, subStr);

                        double centerX = txtW * 0.5;
                        double dotsY = 178.0 + subOffsetY;
                        double spacing = 22.0;

                        p.setPen(Qt::NoPen);
                        for (int i = 0; i < 3; ++i) {
                            double dx = centerX + (i - 1) * spacing;
                            double phase = double(ms) * 0.0055 - double(i) * 1.15;
                            double wave = std::pow(0.5 + 0.5 * std::sin(phase), 2.2);

                            double radius = 3.4 + 2.5 * wave;
                            int coreAlpha = std::clamp(int(std::round((45.0 + 210.0 * wave) * subAlpha)), 0, 255);
                            int glowAlpha = std::clamp(int(std::round((75.0 * wave) * subAlpha)), 0, 255);

                            if (glowAlpha > 4) {
                                p.setBrush(QColor(255, 255, 255, glowAlpha));
                                p.drawEllipse(QPointF(dx, dotsY), radius * 2.1, radius * 2.1);
                            }
                            p.setBrush(QColor(255, 255, 255, coreAlpha));
                            p.drawEllipse(QPointF(dx, dotsY), radius, radius);
                        }
                    }
                    p.end();
                }

                POINT ptDst = { textBaseX, textBaseY };
                SIZE szDst = { txtW, txtH };
                POINT ptSrc = { 0, 0 };
                BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
                UpdateLayeredWindow(hTextWin, hdcScreen, &ptDst, &szDst, hdcTextMem, &ptSrc, 0, &bf, ULW_ALPHA);
            }

            SetWindowPos(hCurtain, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
            SetWindowPos(hTextWin, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
            Sleep(13);
            continue;
        }

        if (hTextWin && IsWindow(hTextWin)) {
            DestroyWindow(hTextWin);
            hTextWin = nullptr;
        }

        double t = std::clamp((double(ms) - holdMs) / fadeOutMs, 0.0, 1.0);
        if (t >= 1.0) {
            break;
        }
        double inv = 1.0 - t;
        BYTE alpha = static_cast<BYTE>(std::clamp(int(std::round(255.0 * (inv * inv * inv))), 0, 255));
        POINT ptDst = { 0, 0 };
        BLENDFUNCTION bf = { AC_SRC_OVER, 0, alpha, AC_SRC_ALPHA };
        UpdateLayeredWindow(hCurtain, nullptr, &ptDst, nullptr, nullptr, nullptr, 0, &bf, ULW_ALPHA);
        SetWindowPos(hCurtain, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
        Sleep(13);
    }

    g_introCurtainOpaque = false;
    if (hTextWin && IsWindow(hTextWin)) DestroyWindow(hTextWin);
    if (hCurtain && IsWindow(hCurtain)) DestroyWindow(hCurtain);
    if (hdcTextMem) {
        if (hOldTextBmp) SelectObject(hdcTextMem, hOldTextBmp);
        if (hTextDib) DeleteObject(hTextDib);
        DeleteDC(hdcTextMem);
    }
    ReleaseDC(nullptr, hdcScreen);
    g_introThreadRunning = false;
}

static void showStartupDarkCurtain() {
    if (g_introThreadRunning.exchange(true)) return;
    QString userName = qEnvironmentVariable("USERNAME").trimmed();
    if (userName.isEmpty()) userName = qEnvironmentVariable("USER").trimmed();
    if (userName.isEmpty()) userName = "User";
    userName[0] = userName[0].toUpper();
    QString greeting = QString("Привет, %1!").arg(userName);

    std::thread(runStartupCurtainThreadFunc, greeting).detach();
}
static HICON createTrayHIconFromResource() {
    HICON hIco = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1));
    if (hIco) return hIco;
    QIcon qic(":/winwal.ico");
    QImage img = qic.pixmap(32, 32).toImage().convertToFormat(QImage::Format_ARGB32);
    if (!img.isNull()) {
        ICONINFO ii = {};
        ii.fIcon = TRUE;
        HDC hdc = GetDC(nullptr);
        BITMAPV5HEADER bi = {};
        bi.bV5Size = sizeof(BITMAPV5HEADER);
        bi.bV5Width = img.width();
        bi.bV5Height = -img.height();
        bi.bV5Planes = 1;
        bi.bV5BitCount = 32;
        bi.bV5Compression = BI_BITFIELDS;
        bi.bV5RedMask   = 0x00FF0000;
        bi.bV5GreenMask = 0x0000FF00;
        bi.bV5BlueMask  = 0x000000FF;
        bi.bV5AlphaMask = 0xFF000000;
        void *bits = nullptr;
        ii.hbmColor = CreateDIBSection(hdc, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS, &bits, nullptr, 0);
        ii.hbmMask = CreateBitmap(img.width(), img.height(), 1, 1, nullptr);
        if (ii.hbmColor && bits) {
            memcpy(bits, img.constBits(), size_t(img.width() * img.height() * 4));
            hIco = CreateIconIndirect(&ii);
        }
        if (ii.hbmColor) DeleteObject(ii.hbmColor);
        if (ii.hbmMask) DeleteObject(ii.hbmMask);
        ReleaseDC(nullptr, hdc);
    }
    return hIco ? hIco : LoadIconW(nullptr, IDI_APPLICATION);
}

static HWND s_trayMsgHwnd = nullptr;
static std::function<void()> s_trayToggleMain;
static std::function<void()> s_trayRandomWall;
static std::function<void()> s_trayCheckUpdate;
static std::function<void()> s_trayResetAll;
static std::function<void()> s_trayQuitApp;

static LRESULT CALLBACK TrayMsgWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_USER + 77) {
        UINT ev = LOWORD(lParam);
        if (ev == WM_LBUTTONUP || ev == WM_LBUTTONDBLCLK) {
            if (s_trayToggleMain) s_trayToggleMain();
            return 0;
        }
        if (ev == WM_RBUTTONUP || ev == WM_CONTEXTMENU) {
            POINT pt = {};
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            if (hMenu) {
                AppendMenuW(hMenu, MF_STRING, 1, L"Открыть / Скрыть WinWal");
                AppendMenuW(hMenu, MF_STRING, 2, L"Случайные обои");
                AppendMenuW(hMenu, MF_STRING, 3, L"Проверить обновления (GitHub)");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, MF_STRING, 4, L"Сбросить панель задач и окна");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, MF_STRING, 5, L"Выключить WinWal (Выход)");

                SetForegroundWindow(hwnd);
                UINT cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
                PostMessageW(hwnd, WM_NULL, 0, 0);
                DestroyMenu(hMenu);

                if (cmd == 1 && s_trayToggleMain) s_trayToggleMain();
                else if (cmd == 2 && s_trayRandomWall) s_trayRandomWall();
                else if (cmd == 3 && s_trayCheckUpdate) s_trayCheckUpdate();
                else if (cmd == 4 && s_trayResetAll) s_trayResetAll();
                else if (cmd == 5 && s_trayQuitApp) s_trayQuitApp();
            }
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

class WinHotkeyFilter : public QAbstractNativeEventFilter {
public:
    std::function<void()> onToggleMain;
    std::function<void()> onOpenWallpaper;
    std::function<void()> onToggleKb;

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override {
        Q_UNUSED(eventType);
        Q_UNUSED(result);
        MSG *msg = static_cast<MSG *>(message);
        if (msg && msg->message == WM_HOTKEY) {
            if (msg->wParam == 1001 && onToggleMain) onToggleMain();
            else if (msg->wParam == 1002 && onOpenWallpaper) onOpenWallpaper();
            else if (msg->wParam == 1003 && onToggleKb) onToggleKb();
            return true;
        }
        return false;
    }
};
#endif

static bool ensureDefaultConfigs() {
    QString targetDir = QDir::homePath() + "/.config/hypr/scripts";
    QDir().mkpath(targetDir);
    QString firstRunMarker = QCoreApplication::applicationDirPath() + "/.winwal_first_launch.done";
    bool isFirstRun = !QFile::exists(firstRunMarker);

    QString srcDir = QCoreApplication::applicationDirPath() + "/assets/configs";
    QDir d(srcDir);
    if (d.exists()) {
        for (const QString &fn : d.entryList(QDir::Files)) {
            QString dst = targetDir + "/" + fn;
            if (isFirstRun || !QFile::exists(dst)) {
                if (QFile::exists(dst)) QFile::remove(dst);
                QFile::copy(d.filePath(fn), dst);
            }
        }
    }
    return isFirstRun;
}

static void loadBundledFonts() {
    QFontDatabase::addApplicationFont(":/fonts/NerdFont.ttf");
    QString extFont = QCoreApplication::applicationDirPath() + "/assets/fonts/NerdFont.ttf";
    if (QFile::exists(extFont)) QFontDatabase::addApplicationFont(extFont);
    QString anuratiFont = QCoreApplication::applicationDirPath() + "/assets/fonts/Anurati-Regular.otf";
    if (QFile::exists(anuratiFont)) QFontDatabase::addApplicationFont(anuratiFont);
}

int main(int argc, char *argv[]) {
    bool isDaemonFlag = false;
    QString launchTarget = "default";
    QVector<char*> cleanArgv;
    cleanArgv.push_back(argv[0]);
    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromUtf8(argv[i]);
        if (arg == "--daemon") {
            isDaemonFlag = true;
            cleanArgv.push_back(argv[i]);
        } else if (arg == "--wallpaper" || arg == "--wallpapers") {
            launchTarget = "wallpaper";
        } else if (arg == "--atmosphere-overlay") {
            return 0;
        } else {
            cleanArgv.push_back(argv[i]);
        }
    }
    qputenv("HYSETT_DAEMON_START", isDaemonFlag ? "1" : "0");
    if (!isDaemonFlag) {
        QFile cmdFile(QDir::tempPath() + "/winwal_launch_cmd");
        if (cmdFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            cmdFile.write(launchTarget.toUtf8());
            cmdFile.close();
        }
    }
    argc = cleanArgv.size();
    cleanArgv.push_back(nullptr);
    argv = cleanArgv.data();

    qputenv("QML_DISK_CACHE", "aot"); qputenv("QT_FFMPEG_DECODING_HW_DEVICE_TYPES", "d3d11va");
    QQuickStyle::setStyle("Basic");
    QQuickWindow::setDefaultAlphaBuffer(true);

    QGuiApplication app(argc, argv);
    QPixmapCache::setCacheLimit(2048);
    app.setApplicationName("WinWal");
    app.setWindowIcon(QIcon(":/winwal.ico"));
    app.setOrganizationName("whrr");
    app.setOrganizationDomain("com.whrr");

    AppInstance instance("com.whrr.WinWal.socket");
    if (!instance.checkSingleInstance()) {
        return 0;
    }

    bool isFirstRunAfterInstall = ensureDefaultConfigs();
#ifdef Q_OS_WIN
    if (isFirstRunAfterInstall && !isDaemonFlag) {
        showStartupDarkCurtain();
        // Пока экран закрыт тёмной завесой, сворачиваем все открытые сторонние окна,
        // чтобы после растворения завесы был виден чистый рабочий стол с Сакурой, часами и WinWal!
        QTimer::singleShot(520, &app, []() {
            DWORD myPid = GetCurrentProcessId();
            EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
                if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) return TRUE;
                DWORD pid = 0;
                GetWindowThreadProcessId(hwnd, &pid);
                if (pid == static_cast<DWORD>(lParam)) return TRUE;

                INT cloaked = 0;
                if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked != 0)
                    return TRUE;

                LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
                LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
                if (!(style & WS_MINIMIZEBOX)) return TRUE;
                if ((ex & WS_EX_TOOLWINDOW) || (ex & WS_EX_NOACTIVATE) || (ex & WS_EX_TRANSPARENT)) return TRUE;

                wchar_t cls[128] = {};
                GetClassNameW(hwnd, cls, 127);
                if (wcscmp(cls, L"Progman") == 0 || wcscmp(cls, L"WorkerW") == 0 ||
                    wcscmp(cls, L"Shell_TrayWnd") == 0 || wcscmp(cls, L"Shell_SecondaryTrayWnd") == 0 ||
                    wcscmp(cls, L"Windows.UI.Core.CoreWindow") == 0)
                    return TRUE;

                ShowWindowAsync(hwnd, SW_MINIMIZE);
                return TRUE;
            }, static_cast<LPARAM>(myPid));
        });
    }
#endif
    loadBundledFonts();

    QQmlApplicationEngine engine;
    engine.addImageProvider("wallthumb", new WallThumbProvider());

    static ThemeEngine themeEngine;
    static HyprIpc hyprIpc;
    hyprIpc.setEngine(&engine);
    static AppConfig appConfig;
    static LookManager lookManager;
    static WidgetsManager widgetsManager;
    static WallpapersManager wallpapersManager;
    static AtmosphereManager atmosphereManager(false);
    static TaskbarManager taskbarManager;
    static QObject dummyObj;

    if (isFirstRunAfterInstall) {
        QString marker = QCoreApplication::applicationDirPath() + "/.winwal_first_launch.done";
        QFile mf(marker);
        if (mf.open(QIODevice::WriteOnly | QIODevice::Text)) {
            mf.write("1\n");
            mf.close();
        }
        // Применяем при первом старте:
        // 1) 5% размытие окон (Aero Blur) + 100% непрозрачность
        lookManager.setProperty("enabled", true);
        lookManager.setProperty("activeOpacity", 1.0);
        lookManager.setProperty("inactiveOpacity", 1.0);
        lookManager.setProperty("blurEnabled", true);
        lookManager.setProperty("blurMode", 0);
        lookManager.setProperty("blurSize", 1.0);
        // 2) Включаем виджет часов на рабочем столе
        widgetsManager.toggleClockProcess(true);
        // 3) Включаем пресет «Сакура (Live)» через 350 мс (когда загружен DesktopWallpaperOverlay)
        QTimer::singleShot(350, &wallpapersManager, []() {
            wallpapersManager.applyPreset(1);
        });
    }

    QQmlEngine::setObjectOwnership(&themeEngine, QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(&hyprIpc, QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(&appConfig, QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(&lookManager, QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(&widgetsManager, QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(&wallpapersManager, QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(&atmosphereManager, QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(&taskbarManager, QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(&dummyObj, QQmlEngine::CppOwnership);

    qmlRegisterSingletonInstance("HySett", 1, 0, "Theme", &themeEngine);
    qmlRegisterSingletonInstance("HySett", 1, 0, "Hypr", &hyprIpc);
    qmlRegisterSingletonInstance("HySett", 1, 0, "Config", &appConfig);
    qmlRegisterSingletonInstance("HySett", 1, 0, "Look", &lookManager);
    qmlRegisterSingletonInstance("HySett", 1, 0, "Widgets", &widgetsManager);
    qmlRegisterSingletonInstance("HySett", 1, 0, "Wallpapers", &wallpapersManager);
    qmlRegisterSingletonInstance("HySett", 1, 0, "Atmosphere", &atmosphereManager);
    qmlRegisterSingletonInstance("HySett", 1, 0, "Taskbar", &taskbarManager);
    qmlRegisterSingletonInstance("HySett", 1, 0, "Monitors", &dummyObj);
    qmlRegisterType<AtmosphereCanvas>("HySett", 1, 0, "AtmosphereCanvas");

    auto ctx = engine.rootContext();
    ctx->setContextProperty("Theme", &themeEngine);
    ctx->setContextProperty("Hypr", &hyprIpc);
    ctx->setContextProperty("Config", &appConfig);
    ctx->setContextProperty("Look", &lookManager);
    ctx->setContextProperty("Widgets", &widgetsManager);
    ctx->setContextProperty("Wallpapers", &wallpapersManager);
    ctx->setContextProperty("Atmosphere", &atmosphereManager);
    ctx->setContextProperty("Taskbar", &taskbarManager);
    ctx->setContextProperty("Monitors", &dummyObj);

    auto toggleWindowFn = [&engine]() {
        if (engine.rootObjects().isEmpty()) return;
        QQuickWindow *win = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        if (win) {
            HWND hwnd = reinterpret_cast<HWND>(win->winId());
            bool isFg = (hwnd && GetForegroundWindow() == hwnd);
            bool wsHidden = win->property("hiddenByWorkspace").toBool();
            if (!wsHidden && win->isVisible() && win->visibility() != QWindow::Minimized && isFg) {
                win->hide();
                hyprIpc.trimMemory();
            } else {
                win->setProperty("hiddenByWorkspace", false);
                win->showNormal();
                win->show();
                win->raise();
                win->requestActivate();
                if (hwnd) {
                    ShowWindow(hwnd, SW_RESTORE);
                    SetForegroundWindow(hwnd);
                }
            }
        }
    };

    QObject::connect(&instance, &AppInstance::toggleRequested, toggleWindowFn);

#ifdef Q_OS_WIN
    static WinHotkeyFilter hkFilter;
    hkFilter.onToggleMain = toggleWindowFn;
    hkFilter.onOpenWallpaper = [&engine]() {
        QFile cmdFile(QDir::tempPath() + "/winwal_launch_cmd");
        if (cmdFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            cmdFile.write("wallpaper");
            cmdFile.close();
        }
        if (!engine.rootObjects().isEmpty()) {
            if (auto *win = qobject_cast<QQuickWindow*>(engine.rootObjects().first())) {
                win->setProperty("hiddenByWorkspace", false);
                win->show();
                win->raise();
                win->requestActivate();
            }
        }
    };
    hkFilter.onToggleKb = []() { widgetsManager.toggleKbVisibility(); };
    app.installNativeEventFilter(&hkFilter);

    WNDCLASSEXW trayWc = { sizeof(WNDCLASSEXW) };
    trayWc.lpfnWndProc = TrayMsgWndProc;
    trayWc.hInstance = GetModuleHandleW(nullptr);
    trayWc.lpszClassName = L"WinWalTrayHostWnd";
    RegisterClassExW(&trayWc);

    s_trayMsgHwnd = CreateWindowExW(WS_EX_TOOLWINDOW, L"WinWalTrayHostWnd", L"WinWalTray", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = s_trayMsgHwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_USER + 77;
    nid.hIcon = createTrayHIconFromResource();
    wcscpy_s(nid.szTip, L"WinWal — ЛКМ: Открыть | ПКМ: Меню и Выход");
    Shell_NotifyIconW(NIM_ADD, &nid);

    static bool s_cleanedUp = false;
    auto performFullCleanShutdown = [&]() {
        if (s_cleanedUp) return;
        s_cleanedUp = true;
        Shell_NotifyIconW(NIM_DELETE, &nid);
        // Восстанавливаем стандартный вид Windows в памяти БЕЗ перезаписи конфигов пользователя нулями!
        taskbarManager.restoreNativeTaskbar();
        lookManager.restoreAllWindows();
        wallpapersManager.shutdownRestoreDesktop();
        widgetsManager.shutdownWidgets();
    };

    hyprIpc.onCleanShutdown = performFullCleanShutdown;
    hyprIpc.onPlayIntro = []() { showStartupDarkCurtain(); };
    s_trayToggleMain = toggleWindowFn;
    s_trayRandomWall = [&]() { wallpapersManager.applyRandomWallpaper(); };
    s_trayCheckUpdate = [&]() {
        toggleWindowFn();
        hyprIpc.checkForUpdates();
    };
    s_trayResetAll = [&]() {
        taskbarManager.restoreNativeTaskbar();
        lookManager.restoreAllWindows();
        wallpapersManager.shutdownRestoreDesktop();
    };
    s_trayQuitApp = [&]() {
        hyprIpc.quitApplication();
    };

    QObject::connect(&app, &QCoreApplication::aboutToQuit, performFullCleanShutdown);

    // Периодическая мягкая очистка рабочего набора ОЗУ (каждые 15 секунд)
    QTimer *ramTrimTimer = new QTimer(&app);
    QObject::connect(ramTrimTimer, &QTimer::timeout, []() {
        _heapmin(); EmptyWorkingSet(GetCurrentProcess()); SetProcessWorkingSetSizeEx(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1, QUOTA_LIMITS_HARDWS_MIN_ENABLE);
    });
    ramTrimTimer->start(10000);

    RegisterHotKey(nullptr, 1001, MOD_ALT | MOD_NOREPEAT, 'Q');
    RegisterHotKey(nullptr, 1002, MOD_ALT | MOD_NOREPEAT, 'W');
    RegisterHotKey(nullptr, 1003, MOD_WIN | MOD_NOREPEAT, VK_F9);
#endif

    const QUrl url(QStringLiteral("qrc:/qt/qml/HySett/qml/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url, isDaemonFlag](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);

        QQuickWindow *win = qobject_cast<QQuickWindow*>(obj);
        if (win && win->objectName() == "atmoGlobalOverlayWin") {
#ifdef Q_OS_WIN
            auto enforceTopMostOverTaskbar = [win]() {
                if (!win->isVisible() || FindWindowW(L"WinWalStartupCurtain", nullptr) != nullptr) return;
                HWND hwnd = reinterpret_cast<HWND>(win->winId());
                if (!hwnd) return;
                MARGINS m = {-1, -1, -1, -1};
                DwmExtendFrameIntoClientArea(hwnd, &m);
                LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
                SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE);
                SetPropW(hwnd, L"NonRudeHWND", reinterpret_cast<HANDLE>(1));
                SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, GetSystemMetrics(SM_CXSCREEN), std::max(200, GetSystemMetrics(SM_CYSCREEN) - 1), SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING);
            };
            QObject::connect(win, &QWindow::visibleChanged, win, [enforceTopMostOverTaskbar](bool v) {
                if (v) QTimer::singleShot(50, enforceTopMostOverTaskbar);
            });
            QTimer *topTimer = new QTimer(win);
            QObject::connect(topTimer, &QTimer::timeout, win, enforceTopMostOverTaskbar);
            topTimer->start(800);
            QTimer::singleShot(100, enforceTopMostOverTaskbar);
#endif
        }
        if (win && objUrl == url) {
            if (isDaemonFlag) {
                win->hide();
                hyprIpc.trimMemory();
            } else {
                themeEngine.applyEffectiveBlur(themeEngine.windowBlur());
            }
        }
    }, Qt::QueuedConnection);

    engine.load(url);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/HySett/qml/AtmosphereOverlay.qml")));
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/HySett/qml/DesktopWallpaperOverlay.qml")));
    return app.exec();
}