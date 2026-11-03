#include <atomic>
extern std::atomic<bool> g_workspaceGlideActive;
#include "LookManager.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QHash>
#include <QRect>
#include <algorithm>
#include <cmath>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>

struct LK_ACCENT_POLICY {
    int AccentState;
    int AccentFlags;
    DWORD GradientColor;
    int AnimationId;
};
struct LK_WINCOMPATTRDATA {
    DWORD Attrib;
    PVOID pvData;
    SIZE_T cbData;
};
typedef BOOL(WINAPI *pfnSetWindowCompositionAttribute)(HWND, LK_WINCOMPATTRDATA *);

static QHash<quintptr, HWND> s_blurBackdrops;
static LookManager *s_lookInst = nullptr;
static HHOOK s_tilingKbHook = nullptr;
static HWINEVENTHOOK s_winMoveEventHook = nullptr;

static void CALLBACK WinLocationChangeEventProc(
    HWINEVENTHOOK, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD, DWORD)
{
    if (event == EVENT_SYSTEM_FOREGROUND && s_lookInst && s_lookInst->enabled()) { QMetaObject::invokeMethod(s_lookInst, "onTick", Qt::QueuedConnection); return; }
    if (event != EVENT_OBJECT_LOCATIONCHANGE || idObject != OBJID_WINDOW || idChild != CHILDID_SELF || !hwnd)
        return;
    if (!s_lookInst || !s_lookInst->enabled() || !s_lookInst->blurEnabled())
        return;
    quintptr key = reinterpret_cast<quintptr>(hwnd);
    auto it = s_blurBackdrops.constFind(key);
    if (it == s_blurBackdrops.constEnd()) return;
    HWND hBlur = it.value();
    if (!hBlur || !IsWindow(hBlur)) return;

    if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) {
        ShowWindow(hBlur, SW_HIDE);
        return;
    }
    RECT r = {};
    if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof(r)))) {
        GetWindowRect(hwnd, &r);
    }
    int w = r.right - r.left, h = r.bottom - r.top;
    if (w >= 80 && h >= 50) {
        SetWindowPos(hBlur, hwnd, r.left, r.top, w, h,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING | SWP_DEFERERASE);
    }
}

static LRESULT CALLBACK TilingLowLevelKbProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (s_lookInst && nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        auto *kb = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
        bool altHeld = ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0);
        bool shiftHeld = ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
        bool ctrlHeld = ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
        bool winHeld = ((GetAsyncKeyState(VK_LWIN) & 0x8000) != 0) || ((GetAsyncKeyState(VK_RWIN) & 0x8000) != 0);
        if (altHeld && !ctrlHeld && !winHeld && kb) {
            // Alt + Space: переключить плавающий режим (Floating) и отцентрировать окно посередине экрана!
            if (!shiftHeld && kb->vkCode == VK_SPACE) {
                QMetaObject::invokeMethod(s_lookInst, "toggleActiveWindowFloating", Qt::QueuedConnection);
                return 1;
            }
            // Alt + Shift + Стрелки ИЛИ Alt + Shift + W/A/S/D: поменять местами активное окно в выбранную сторону!
            if (shiftHeld && s_lookInst->tilingEnabled()) {
                int dir = -1; // 0 = Left (← / A), 1 = Right (→ / D), 2 = Up (↑ / W), 3 = Down (↓ / S)
                if (kb->vkCode == VK_LEFT || kb->vkCode == 'A') dir = 0;
                else if (kb->vkCode == VK_RIGHT || kb->vkCode == 'D') dir = 1;
                else if (kb->vkCode == VK_UP || kb->vkCode == 'W') dir = 2;
                else if (kb->vkCode == VK_DOWN || kb->vkCode == 'S') dir = 3;
                if (dir >= 0) {
                    QMetaObject::invokeMethod(s_lookInst, [inst = s_lookInst, dir]() {
                        inst->moveActiveTiledWindow(dir);
                    }, Qt::QueuedConnection);
                    return 1;
                }
            }
            if (!shiftHeld && kb->vkCode == 'T') {
                QMetaObject::invokeMethod(s_lookInst, [inst = s_lookInst]() {
                    inst->setTilingEnabled(!inst->tilingEnabled());
                }, Qt::QueuedConnection);
                return 1;
            } else if (!shiftHeld && s_lookInst->tilingEnabled() && kb->vkCode == 'M') {
                QMetaObject::invokeMethod(s_lookInst, "promoteActiveToMaster", Qt::QueuedConnection);
                return 1;
            } else if (!shiftHeld && s_lookInst->tilingEnabled() && kb->vkCode == 'R') {
                QMetaObject::invokeMethod(s_lookInst, "cycleTilingLayout", Qt::QueuedConnection);
                return 1;
            }
        }
    }
    return CallNextHookEx(s_tilingKbHook, nCode, wParam, lParam);
}

static LRESULT CALLBACK BlurBackdropWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static HWND getOrCreateBlurBackdrop(quintptr targetKey) {
    auto it = s_blurBackdrops.find(targetKey);
    if (it != s_blurBackdrops.end() && IsWindow(it.value())) {
        return it.value();
    }
    static bool s_clsReg = false;
    if (!s_clsReg) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = BlurBackdropWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        wc.lpszClassName = L"WinWalDwmGlassSheet";
        RegisterClassExW(&wc);
        s_clsReg = true;
    }
    HWND hBlur = CreateWindowExW(
        WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"WinWalDwmGlassSheet", L"", WS_POPUP,
        0, 0, 10, 10, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    MARGINS m = {-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(hBlur, &m);
    s_blurBackdrops.insert(targetKey, hBlur);
    return hBlur;
}

static void applyDwmBlurToSheet(HWND hBlur, int mode, double strength) {
    static pfnSetWindowCompositionAttribute setAttr = nullptr;
    if (!setAttr) {
        if (HMODULE hUser = GetModuleHandleW(L"user32.dll")) {
            setAttr = reinterpret_cast<pfnSetWindowCompositionAttribute>(
                GetProcAddress(hUser, "SetWindowCompositionAttribute"));
        }
    }
    if (!setAttr || !hBlur) return;
    LK_ACCENT_POLICY policy = {};
    policy.AccentState = (mode == 1) ? 4 : 3;
    policy.AccentFlags = 2;
    DWORD a = std::clamp(int(strength * 11.0), 15, 225);
    policy.GradientColor = (a << 24) | 0x00101218U;
    LK_WINCOMPATTRDATA data = {19, &policy, sizeof(policy)};
    setAttr(hBlur, &data);
}

static void destroyAllBlurBackdrops() {
    for (HWND h : s_blurBackdrops) {
        if (IsWindow(h)) DestroyWindow(h);
    }
    s_blurBackdrops.clear();
}

static bool isTileableAppWindow(HWND hwnd, DWORD selfPid, bool skipWinWal, int scrW, int scrH) {
    if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) return false;
    if (GetParent(hwnd) != nullptr || GetWindow(hwnd, GW_OWNER) != nullptr) return false;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (skipWinWal && pid == selfPid) return false;

    INT cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked != 0)
        return false;

    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if ((ex & WS_EX_TOOLWINDOW) || (ex & WS_EX_TRANSPARENT) || (ex & WS_EX_NOACTIVATE) || (ex & WS_EX_TOPMOST))
        return false;
    // Тайлим обычные изменяемые по размеру окна (или главное окно WinWal, если разрешено)
    if (pid != selfPid && !(style & WS_THICKFRAME)) return false;

    wchar_t cls[128] = {};
    GetClassNameW(hwnd, cls, 127);
    if (wcscmp(cls, L"Progman") == 0 || wcscmp(cls, L"WorkerW") == 0 ||
        wcscmp(cls, L"Shell_TrayWnd") == 0 || wcscmp(cls, L"Shell_SecondaryTrayWnd") == 0 ||
        wcscmp(cls, L"Windows.UI.Core.CoreWindow") == 0 || wcscmp(cls, L"WinWalDwmGlassSheet") == 0 ||
        wcscmp(cls, L"#32770") == 0)
        return false;

    wchar_t title[256] = {};
    GetWindowTextW(hwnd, title, 255);
    if (wcslen(title) == 0 || wcscmp(title, L"WinWalDesktopWallpaper") == 0 || wcscmp(title, L"WinWalAtmosphereOverlay") == 0)
        return false;

    RECT r = {};
    GetWindowRect(hwnd, &r);
    int w = r.right - r.left, h = r.bottom - r.top;
    if (w < 120 || h < 80) return false;
    // Не трогаем полноэкранные игры без рамок
    if (w >= scrW && h >= scrH && !(style & WS_CAPTION) && pid != selfPid) return false;

    return true;
}

static void placeWindowWithDwmFrameCompensation(HWND hwnd, const QRect &targetVisRect) {
    if (!IsWindow(hwnd)) return;
    if (IsZoomed(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    }
    RECT winR = {}, frameR = {};
    GetWindowRect(hwnd, &winR);
    int padL = 0, padT = 0, padR = 0, padB = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &frameR, sizeof(frameR)))) {
        padL = std::clamp(int(frameR.left - winR.left), 0, 16);
        padT = std::clamp(int(frameR.top - winR.top), 0, 16);
        padR = std::clamp(int(winR.right - frameR.right), 0, 16);
        padB = std::clamp(int(winR.bottom - frameR.bottom), 0, 16);
    }
    int dstX = targetVisRect.x() - padL;
    int dstY = targetVisRect.y() - padT;
    int dstW = targetVisRect.width() + padL + padR;
    int dstH = targetVisRect.height() + padT + padB;

    if (std::abs(int(winR.left) - dstX) > 2 || std::abs(int(winR.top) - dstY) > 2 ||
        std::abs(int(winR.right - winR.left) - dstW) > 2 || std::abs(int(winR.bottom - winR.top) - dstH) > 2) {
        SetWindowPos(hwnd, nullptr, dstX, dstY, dstW, dstH,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
    }
}
#endif

LookManager::LookManager(QObject *parent) : QObject(parent) {
    s_lookInst = this;
    load();
#ifdef Q_OS_WIN
    if (!s_tilingKbHook) {
        s_tilingKbHook = SetWindowsHookExW(WH_KEYBOARD_LL, TilingLowLevelKbProc, GetModuleHandleW(nullptr), 0);
    if (!s_winMoveEventHook) s_winMoveEventHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_OBJECT_LOCATIONCHANGE, nullptr, WinLocationChangeEventProc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    }
#endif
    m_timer.setInterval(220);
    connect(&m_timer, &QTimer::timeout, this, &LookManager::onTick);
    m_timer.start();
}

LookManager::~LookManager() {
#ifdef Q_OS_WIN
    if (s_tilingKbHook) {
        UnhookWindowsHookEx(s_tilingKbHook);
    if (s_winMoveEventHook) { UnhookWinEvent(s_winMoveEventHook); s_winMoveEventHook = nullptr; }
        s_tilingKbHook = nullptr;
    }
#endif
    if (s_lookInst == this) s_lookInst = nullptr;
    restoreAllWindows();
}

QString LookManager::configPath() const {
    return QDir::homePath() + "/.config/hypr/scripts/look_win.conf";
}

void LookManager::load() {
    QFile f(configPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream in(&f);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        int eq = line.indexOf('=');
        if (line.isEmpty() || line.startsWith('#') || eq == -1) continue;
        QString k = line.left(eq).trimmed(), v = line.mid(eq + 1).trimmed();
        auto pb = [](const QString &s) { return s == "1" || s.compare("true", Qt::CaseInsensitive) == 0; };
        if (k == "enabled") m_enabled = pb(v);
        else if (k == "active_opacity") m_activeOpacity = std::clamp(v.toDouble(), 0.25, 1.0);
        else if (k == "inactive_opacity") m_inactiveOpacity = std::clamp(v.toDouble(), 0.25, 1.0);
        else if (k == "blur_enabled") m_blurEnabled = pb(v);
        else if (k == "blur_mode") m_blurMode = std::clamp(v.toInt(), 0, 1);
        else if (k == "blur_size") m_blurSize = std::clamp(v.toDouble(), 1.0, 20.0);
        else if (k == "ignore_fullscreen") m_ignoreFullscreen = pb(v);
        else if (k == "tiling_enabled") m_tilingEnabled = pb(v);
        else if (k == "tiling_layout") m_tilingLayout = std::clamp(v.toInt(), 0, 3);
        else if (k == "gaps_in") m_gapsIn = std::clamp(v.toInt(), 0, 48);
        else if (k == "gaps_out") m_gapsOut = std::clamp(v.toInt(), 0, 64);
        else if (k == "master_ratio") m_masterRatio = std::clamp(v.toDouble(), 0.25, 0.78);
        else if (k == "float_winwal") m_floatWinWal = pb(v);
    }
}

void LookManager::save() {
    QDir().mkpath(QDir::homePath() + "/.config/hypr/scripts");
    QFile f(configPath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream out(&f);
        out << "enabled=" << (m_enabled ? 1 : 0) << "\n"
            << "active_opacity=" << QString::number(m_activeOpacity, 'f', 2) << "\n"
            << "inactive_opacity=" << QString::number(m_inactiveOpacity, 'f', 2) << "\n"
            << "blur_enabled=" << (m_blurEnabled ? 1 : 0) << "\n"
            << "blur_mode=" << m_blurMode << "\n"
            << "blur_size=" << QString::number(m_blurSize, 'f', 1) << "\n"
            << "ignore_fullscreen=" << (m_ignoreFullscreen ? 1 : 0) << "\n"
            << "tiling_enabled=" << (m_tilingEnabled ? 1 : 0) << "\n"
            << "tiling_layout=" << m_tilingLayout << "\n"
            << "gaps_in=" << m_gapsIn << "\n"
            << "gaps_out=" << m_gapsOut << "\n"
            << "master_ratio=" << QString::number(m_masterRatio, 'f', 2) << "\n"
            << "float_winwal=" << (m_floatWinWal ? 1 : 0) << "\n";
    }
}

void LookManager::restoreAllWindows() {
    m_timer.stop();
#ifdef Q_OS_WIN
    destroyAllBlurBackdrops();
    for (quintptr raw : m_managedHwnds) {
        HWND hwnd = reinterpret_cast<HWND>(raw);
        if (IsWindow(hwnd)) {
            SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
        }
    }
    m_managedHwnds.clear();
#endif
}

void LookManager::applyTilingLayoutPass() {
#ifdef Q_OS_WIN
    if (!m_tilingEnabled || g_workspaceGlideActive.load()) return;
    // Не мешаем пользователю, пока он перетаскивает или меняет размер окна мышью!
    static bool s_wasLmbDown = false;
    bool lmbDown = ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
    if (lmbDown) {
        s_wasLmbDown = true;
        return;
    }

    DWORD selfPid = GetCurrentProcessId();
    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);

    struct TileEnumCtx {
        DWORD selfPid;
        bool floatWinWal;
        int scrW, scrH;
        const QSet<quintptr> *floatingSet;
        QList<quintptr> found;
    } ctx{selfPid, m_floatWinWal, scrW, scrH, &m_floatingHwnds, {}};

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto *c = reinterpret_cast<TileEnumCtx *>(lParam);
        quintptr key = reinterpret_cast<quintptr>(hwnd);
        if (c->floatingSet->contains(key)) return TRUE;
        if (isTileableAppWindow(hwnd, c->selfPid, c->floatWinWal, c->scrW, c->scrH)) {
            c->found.append(key);
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));

    // Удаляем закрытые/свёрнутые окна из m_tiledOrder и добавляем новые
    for (int i = m_tiledOrder.size() - 1; i >= 0; --i) {
        if (!ctx.found.contains(m_tiledOrder[i])) {
            m_tiledOrder.removeAt(i);
        }
    }
    for (quintptr k : ctx.found) {
        if (!m_tiledOrder.contains(k)) {
            m_tiledOrder.append(k);
        }
    }

    int N = m_tiledOrder.size();
    if (N == 0) return;

    // Если пользователь только что отпустил ЛКМ после перетаскивания окна влево — меняем его местами с главным (Master)!
    if (s_wasLmbDown && N >= 2) {
        s_wasLmbDown = false;
        HWND fg = GetForegroundWindow();
        quintptr fgKey = reinterpret_cast<quintptr>(fg);
        int fgIdx = m_tiledOrder.indexOf(fgKey);
        if (fgIdx > 0) {
            RECT rFg = {}, rMaster = {};
            GetWindowRect(fg, &rFg);
            GetWindowRect(reinterpret_cast<HWND>(m_tiledOrder[0]), &rMaster);
            if ((rFg.left + rFg.right) / 2 < (rMaster.left + rMaster.right) / 2 + 80) {
                m_tiledOrder.swapItemsAt(0, fgIdx);
            }
        }
    }
    s_wasLmbDown = false;

    RECT wa = { 0, 0, scrW, scrH - 34 };
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int gOut = std::clamp(m_gapsOut, 0, 64);
    int gIn  = std::clamp(m_gapsIn, 0, 48);

    QRect avail(wa.left + gOut, wa.top + gOut,
                std::max(200, int(wa.right - wa.left) - gOut * 2),
                std::max(150, int(wa.bottom - wa.top) - gOut * 2));

    if (N == 1 || m_tilingLayout == 3) {
        // 1 окно или режим Монокль (Monocle)
        for (int i = 0; i < N; ++i) {
            placeWindowWithDwmFrameCompensation(reinterpret_cast<HWND>(m_tiledOrder[i]), avail);
        }
    } else if (m_tilingLayout == 0) {
        // Умная сетка (Сначала 2 рядом -> затем правая половина делится на 2 -> затем левая половина делится на 2 -> и т.д.)
        int leftW  = int(std::round(avail.width() * m_masterRatio)) - gIn / 2;
        int rightX = avail.left() + leftW + gIn;
        int rightW = avail.width() - leftW - gIn;

        // Чётные индексы (0, 3, 5...) идут в левую колонку, нечётные (1, 2, 4...) — в правую:
        // N=2: слева 1, справа 1 (2 рядом)
        // N=3: слева 1, справа 2 (правая половина делится на 2)
        // N=4: слева 2, справа 2 (левая половина тоже делится на 2)
        // N=5: слева 2, справа 3 (правая на 3), N=6: слева 3, справа 3...
        int leftCount  = N / 2;
        int rightCount = N - leftCount;

        QList<quintptr> leftWins, rightWins;
        leftWins.append(m_tiledOrder[0]);
        for (int i = 1; i < N; ++i) {
            if (rightWins.size() < rightCount && (i <= 2 || (i % 2 == 0) || leftWins.size() >= leftCount)) {
                rightWins.append(m_tiledOrder[i]);
            } else {
                leftWins.append(m_tiledOrder[i]);
            }
        }

        int lCnt = std::max(1, int(leftWins.size())); int lH = std::max(60, (avail.height() - gIn * (lCnt - 1)) / lCnt);
        for (int i = 0; i < leftWins.size(); ++i) {
            int y = avail.top() + i * (lH + gIn);
            int h = (i == leftWins.size() - 1) ? (avail.bottom() - y + 1) : lH;
            placeWindowWithDwmFrameCompensation(reinterpret_cast<HWND>(leftWins[i]), QRect(avail.left(), y, leftW, h));
        }

        int rCnt = std::max(1, int(rightWins.size())); int rH = std::max(60, (avail.height() - gIn * (rCnt - 1)) / rCnt);
        for (int i = 0; i < rightWins.size(); ++i) {
            int y = avail.top() + i * (rH + gIn);
            int h = (i == rightWins.size() - 1) ? (avail.bottom() - y + 1) : rH;
            placeWindowWithDwmFrameCompensation(reinterpret_cast<HWND>(rightWins[i]), QRect(rightX, y, rightW, h));
        }
    } else if (m_tilingLayout == 1) {
        // Master + Стек (1 главное окно слева во всю высоту, остальные стеком справа)
        int masterW = int(std::round(avail.width() * m_masterRatio)) - gIn / 2;
        int stackX  = avail.left() + masterW + gIn;
        int stackW  = avail.width() - masterW - gIn;

        QRect masterRect(avail.left(), avail.top(), masterW, avail.height());
        placeWindowWithDwmFrameCompensation(reinterpret_cast<HWND>(m_tiledOrder[0]), masterRect);

        int stackCount = N - 1;
        int totalGaps = gIn * (stackCount - 1);
        int itemH = std::max(60, (avail.height() - totalGaps) / stackCount);
        for (int i = 0; i < stackCount; ++i) {
            int y = avail.top() + i * (itemH + gIn);
            int h = (i == stackCount - 1) ? (avail.bottom() - y + 1) : itemH;
            placeWindowWithDwmFrameCompensation(reinterpret_cast<HWND>(m_tiledOrder[i + 1]), QRect(stackX, y, stackW, h));
        }
    } else if (m_tilingLayout == 2) {
        // Вертикальные колонки (Columns)
        int colW = std::max(100, (avail.width() - gIn * (N - 1)) / N);
        for (int i = 0; i < N; ++i) {
            int x = avail.left() + i * (colW + gIn);
            int w = (i == N - 1) ? (avail.right() - x + 1) : colW;
            placeWindowWithDwmFrameCompensation(reinterpret_cast<HWND>(m_tiledOrder[i]), QRect(x, avail.top(), w, avail.height()));
        }
    }
#endif
}

void LookManager::onTick() {
#ifdef Q_OS_WIN
    if (m_tilingEnabled) {
        applyTilingLayoutPass();
    }
    if (!m_enabled) return;
    HWND fg = GetForegroundWindow();
    DWORD selfPid = GetCurrentProcessId();
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    QSet<quintptr> seenThisTick;

    struct Ctx {
        LookManager *self;
        HWND fg;
        DWORD selfPid;
        int sw, sh;
        QSet<quintptr> *seen;
    } ctx{this, fg, selfPid, sw, sh, &seenThisTick};

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto *c = reinterpret_cast<Ctx *>(lParam);
        if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) return TRUE;

        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid == c->selfPid) return TRUE;

        INT cloaked = 0;
        if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked != 0)
            return TRUE;

        LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        if ((ex & WS_EX_TOOLWINDOW) || (ex & WS_EX_TRANSPARENT) || (ex & WS_EX_NOACTIVATE)) return TRUE;

        wchar_t cls[128] = {};
        GetClassNameW(hwnd, cls, 127);
        QString cName = QString::fromWCharArray(cls);
        if (cName == "Progman" || cName == "WorkerW" || cName == "Shell_TrayWnd" ||
            cName == "Shell_SecondaryTrayWnd" || cName == "Windows.UI.Core.CoreWindow")
            return TRUE;

        RECT r = {};
        if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof(r)))) {
            GetWindowRect(hwnd, &r);
        }
        int w = r.right - r.left, h = r.bottom - r.top;
        if (w < 80 || h < 50) return TRUE;

        quintptr key = reinterpret_cast<quintptr>(hwnd);
        if (c->self->m_ignoreFullscreen && r.left <= 0 && r.top <= 0 && r.right >= c->sw && r.bottom >= c->sh) {
            if (c->self->m_managedHwnds.contains(key)) {
                SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
            }
            return TRUE;
        }

        bool isAct = (hwnd == c->fg);
        double targetOp = isAct ? c->self->m_activeOpacity : c->self->m_inactiveOpacity;
        if (c->self->m_blurEnabled && targetOp > 0.90) {
            targetOp = 0.86;
        }
        BYTE alpha = static_cast<BYTE>(std::clamp(int(std::round(targetOp * 255.0)), 64, 255));

        struct CachedWinLook { BYTE alpha; int mode; int sz; RECT r; };
        static QHash<quintptr, CachedWinLook> s_lookCache;
        int curSzInt = int(std::round(c->self->m_blurSize * 10.0));
        auto itCache = s_lookCache.find(key);
        bool lookSame = (itCache != s_lookCache.end() &&
                         itCache->alpha == alpha &&
                         itCache->mode == (c->self->m_blurEnabled ? c->self->m_blurMode : -1) &&
                         itCache->sz == curSzInt &&
                         std::abs(itCache->r.left - r.left) <= 1 &&
                         std::abs(itCache->r.top - r.top) <= 1 &&
                         std::abs((itCache->r.right - itCache->r.left) - w) <= 1 &&
                         std::abs((itCache->r.bottom - itCache->r.top) - h) <= 1);

        c->self->m_managedHwnds.insert(key);
        c->seen->insert(key);

        HWND hBlur = c->self->m_blurEnabled ? getOrCreateBlurBackdrop(key) : nullptr;
        bool zOrderLost = (hBlur && (GetWindow(hwnd, GW_HWNDNEXT) != hBlur || !IsWindowVisible(hBlur)));

        if (!lookSame || zOrderLost) {
            if (!(ex & WS_EX_LAYERED)) {
                SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex | WS_EX_LAYERED);
            }
            SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA);

            if (c->self->m_blurEnabled && hBlur) {
                if (!lookSame) {
                    applyDwmBlurToSheet(hBlur, c->self->m_blurMode, c->self->m_blurSize);
                }
                SetWindowPos(hBlur, hwnd, r.left, r.top, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING);
            }
            s_lookCache.insert(key, CachedWinLook{alpha, c->self->m_blurEnabled ? c->self->m_blurMode : -1, curSzInt, r});
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));

    if (!m_blurEnabled) {
        destroyAllBlurBackdrops();
    } else {
        for (auto it = s_blurBackdrops.begin(); it != s_blurBackdrops.end();) {
            if (!seenThisTick.contains(it.key())) {
                if (IsWindow(it.value())) DestroyWindow(it.value());
                it = s_blurBackdrops.erase(it);
            } else {
                ++it;
            }
        }
    }
#endif
}

void LookManager::setEnabled(bool v) {
    if (m_enabled == v) return;
    m_enabled = v;
    if (v) { if (!m_timer.isActive()) m_timer.start(); onTick(); }
    else restoreAllWindows();
    emit stateChanged(); save();
}
void LookManager::setActiveOpacity(qreal v) { v = std::clamp(v, 0.25, 1.0); if (!qFuzzyCompare(m_activeOpacity, v)) { m_activeOpacity = v; onTick(); emit stateChanged(); save(); } }
void LookManager::setInactiveOpacity(qreal v) { v = std::clamp(v, 0.25, 1.0); if (!qFuzzyCompare(m_inactiveOpacity, v)) { m_inactiveOpacity = v; onTick(); emit stateChanged(); save(); } }
void LookManager::setBlurEnabled(bool v) { if (m_blurEnabled != v) { m_blurEnabled = v; onTick(); emit stateChanged(); save(); } }
void LookManager::setBlurMode(int v) { v = std::clamp(v, 0, 1); if (m_blurMode != v) { m_blurMode = v; onTick(); emit stateChanged(); save(); } }
void LookManager::setBlurSize(qreal v) { v = std::clamp(v, 1.0, 20.0); if (!qFuzzyCompare(m_blurSize, v)) { m_blurSize = v; onTick(); emit stateChanged(); save(); } }
void LookManager::setIgnoreFullscreen(bool v) { if (m_ignoreFullscreen != v) { m_ignoreFullscreen = v; emit stateChanged(); save(); } }

void LookManager::setTilingEnabled(bool v) {
    if (m_tilingEnabled != v) {
        m_tilingEnabled = v;
        if (v) applyTilingLayoutPass();
        emit stateChanged();
        save();
    }
}
void LookManager::setTilingLayout(int v) {
    v = std::clamp(v, 0, 3);
    if (m_tilingLayout != v) {
        m_tilingLayout = v;
        applyTilingLayoutPass();
        emit stateChanged();
        save();
    }
}
void LookManager::setGapsIn(int v) {
    v = std::clamp(v, 0, 48);
    if (m_gapsIn != v) {
        m_gapsIn = v;
        applyTilingLayoutPass();
        emit stateChanged();
        save();
    }
}
void LookManager::setGapsOut(int v) {
    v = std::clamp(v, 0, 64);
    if (m_gapsOut != v) {
        m_gapsOut = v;
        applyTilingLayoutPass();
        emit stateChanged();
        save();
    }
}
void LookManager::setMasterRatio(qreal v) {
    v = std::clamp(v, 0.25, 0.78);
    if (!qFuzzyCompare(m_masterRatio, v)) {
        m_masterRatio = v;
        applyTilingLayoutPass();
        emit stateChanged();
        save();
    }
}
void LookManager::setFloatWinWal(bool v) {
    if (m_floatWinWal != v) {
        m_floatWinWal = v;
        applyTilingLayoutPass();
        emit stateChanged();
        save();
    }
}

void LookManager::retileNow() {
    m_tiledOrder.clear();
    applyTilingLayoutPass();
}

void LookManager::toggleActiveWindowFloating() {
#ifdef Q_OS_WIN
    HWND fg = GetForegroundWindow();
    if (!fg) return;
    DWORD selfPid = GetCurrentProcessId();
    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);
    quintptr key = reinterpret_cast<quintptr>(fg);

    if (m_floatingHwnds.contains(key)) {
        // Возвращаем окно обратно в сетку тайлинга
        m_floatingHwnds.remove(key);
        if (!m_tiledOrder.contains(key)) m_tiledOrder.prepend(key);
        applyTilingLayoutPass();
    } else {
        // Переводим в плавающий режим, делаем компактным и ставим ровно по центру экрана!
        if (m_tilingEnabled) {
            m_floatingHwnds.insert(key);
            m_tiledOrder.removeAll(key);
            applyTilingLayoutPass();
        }
        if (IsZoomed(fg)) ShowWindow(fg, SW_RESTORE);
        int floatW = std::clamp(int(scrW * 0.56), 640, scrW - 120);
        int floatH = std::clamp(int(scrH * 0.62), 420, scrH - 120);
        int cx = (scrW - floatW) / 2;
        int cy = (scrH - floatH) / 2;
        placeWindowWithDwmFrameCompensation(fg, QRect(cx, cy, floatW, floatH));
        SetWindowPos(fg, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    }
    Q_UNUSED(selfPid);
#endif
}

void LookManager::moveActiveTiledWindow(int dir) {
#ifdef Q_OS_WIN
    if (!m_tilingEnabled || m_tiledOrder.size() < 2) return;
    HWND fg = GetForegroundWindow();
    if (!fg) return;
    quintptr fgKey = reinterpret_cast<quintptr>(fg);
    int curIdx = m_tiledOrder.indexOf(fgKey);
    if (curIdx < 0) return;

    RECT rCur = {};
    GetWindowRect(fg, &rCur);
    double cx0 = (rCur.left + rCur.right) * 0.5;
    double cy0 = (rCur.top + rCur.bottom) * 0.5;

    int bestIdx = -1;
    double bestScore = 1e18;

    for (int i = 0; i < m_tiledOrder.size(); ++i) {
        if (i == curIdx) continue;
        HWND hOther = reinterpret_cast<HWND>(m_tiledOrder[i]);
        if (!IsWindow(hOther)) continue;
        RECT rO = {};
        GetWindowRect(hOther, &rO);
        double cx = (rO.left + rO.right) * 0.5;
        double cy = (rO.top + rO.bottom) * 0.5;
        double dx = cx - cx0;
        double dy = cy - cy0;

        bool match = false;
        double score = 0.0;
        if (dir == 0 && dx < -30.0) { // Left (← / A)
            match = true; score = std::abs(dx) + std::abs(dy) * 2.2;
        } else if (dir == 1 && dx > 30.0) { // Right (→ / D)
            match = true; score = std::abs(dx) + std::abs(dy) * 2.2;
        } else if (dir == 2 && dy < -30.0) { // Up (↑ / W)
            match = true; score = std::abs(dy) + std::abs(dx) * 2.2;
        } else if (dir == 3 && dy > 30.0) { // Down (↓ / S)
            match = true; score = std::abs(dy) + std::abs(dx) * 2.2;
        }
        if (match && score < bestScore) {
            bestScore = score;
            bestIdx = i;
        }
    }

    if (bestIdx < 0) {
        int n = m_tiledOrder.size();
        bestIdx = (dir == 0 || dir == 2) ? ((curIdx - 1 + n) % n) : ((curIdx + 1) % n);
    }
    if (bestIdx >= 0 && bestIdx != curIdx) {
        m_tiledOrder.swapItemsAt(curIdx, bestIdx);
        applyTilingLayoutPass();
    }
#else
    Q_UNUSED(dir);
#endif
}

void LookManager::promoteActiveToMaster() {
#ifdef Q_OS_WIN
    HWND fg = GetForegroundWindow();
    if (!fg) return;
    quintptr key = reinterpret_cast<quintptr>(fg);
    int idx = m_tiledOrder.indexOf(key);
    if (idx > 0) {
        m_tiledOrder.swapItemsAt(0, idx);
        applyTilingLayoutPass();
    }
#endif
}

void LookManager::cycleTilingLayout() {
    setTilingLayout((m_tilingLayout + 1) % 4);
}

void LookManager::clearFloatingExceptions() {
    m_floatingHwnds.clear();
    applyTilingLayoutPass();
}

void LookManager::setSelectionColor(const QColor &c) {
    if (!c.isValid() || m_selectionColor == c) return;
    m_selectionColor = c;
    applySystemSelectionColors(m_selectionColor, m_selectionFillColor);
    emit stateChanged(); save();
}
void LookManager::setSelectionFillColor(const QColor &c) {
    if (!c.isValid() || m_selectionFillColor == c) return;
    m_selectionFillColor = c;
    applySystemSelectionColors(m_selectionColor, m_selectionFillColor);
    emit stateChanged(); save();
}
void LookManager::applySystemSelectionColors(const QColor &highlightCol, const QColor &hotTrackCol) {
    m_selectionColor = highlightCol;
    m_selectionFillColor = hotTrackCol;
#ifdef Q_OS_WIN
    double lum = 0.299 * highlightCol.redF() + 0.587 * highlightCol.greenF() + 0.114 * highlightCol.blueF();
    QColor textCol = (lum > 0.58) ? QColor(0, 0, 0) : QColor(255, 255, 255);
    INT elements[3] = { COLOR_HIGHLIGHT, COLOR_HOTLIGHT, COLOR_HIGHLIGHTTEXT };
    COLORREF colors[3] = {
        RGB(highlightCol.red(), highlightCol.green(), highlightCol.blue()),
        RGB(hotTrackCol.red(), hotTrackCol.green(), hotTrackCol.blue()),
        RGB(textCol.red(), textCol.green(), textCol.blue())
    };
    SetSysColors(3, elements, colors);
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Colors", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        auto writeRegCol = [&](const wchar_t *name, const QColor &c) {
            QString s = QString("%1 %2 %3").arg(c.red()).arg(c.green()).arg(c.blue());
            RegSetValueExW(hKey, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(s.utf16()), (s.length() + 1) * sizeof(wchar_t));
        };
        writeRegCol(L"Hilight", highlightCol);
        writeRegCol(L"HotTrackingColor", hotTrackCol);
        writeRegCol(L"HilightText", textCol);
        RegCloseKey(hKey);
    }
#endif
    emit stateChanged();
}
void LookManager::restoreDefaultSelectionColors() { applySystemSelectionColors(QColor(0, 120, 215), QColor(0, 102, 204)); save(); }
void LookManager::resetDefaults() {
    setEnabled(false); m_activeOpacity = 0.92; m_inactiveOpacity = 0.80; m_blurEnabled = false; m_blurMode = 0; m_ignoreFullscreen = true;
    m_tilingEnabled = false; m_tilingLayout = 0; m_gapsIn = 8; m_gapsOut = 10; m_masterRatio = 0.50; m_floatWinWal = true;
    m_floatingHwnds.clear(); m_tiledOrder.clear();
    restoreAllWindows(); emit stateChanged(); save();
}