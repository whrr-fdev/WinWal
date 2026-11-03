#include <atomic>
extern std::atomic<bool> g_introCurtainOpaque;
#include "TaskbarManager.h"
#include <QGuiApplication>
#include <QWindow>
#include <QScreen>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QSet>
#include <QMap>
#include <QVector>
#include <QElapsedTimer>
#include <algorithm>
#include <cmath>
#include <vector>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#include <oleacc.h>
#include <uiautomation.h>

static bool s_isShuttingDown = false;
static HWND topZOrderTarget() { HWND hC = FindWindowW(L"WinWalStartupCurtainThreadCls", nullptr); return (hC && IsWindowVisible(hC)) ? hC : HWND_TOPMOST; }

struct TB_ACCENT_POLICY {
    int AccentState;
    int AccentFlags;
    DWORD GradientColor;
    int AnimationId;
};
struct TB_WINCOMPATTRDATA {
    DWORD Attrib;
    PVOID pvData;
    SIZE_T cbData;
};
typedef BOOL(WINAPI *pfnSetWindowCompositionAttribute)(HWND, TB_WINCOMPATTRDATA *);

static void setHwndAccent(HWND hwnd, int styleMode, const QColor &col, qreal opacity, bool showBorder) {
    if (!hwnd || !IsWindow(hwnd)) return;
    static pfnSetWindowCompositionAttribute setAttr = nullptr;
    if (!setAttr) {
        if (HMODULE hUser = GetModuleHandleW(L"user32.dll")) {
            setAttr = reinterpret_cast<pfnSetWindowCompositionAttribute>(
                GetProcAddress(hUser, "SetWindowCompositionAttribute"));
        }
    }
    if (!setAttr) return;

    TB_ACCENT_POLICY policy = {};
    int r = col.red(), g = col.green(), b = col.blue();
    int a = std::clamp(int(std::round(opacity * 255.0)), 0, 255);

    if (styleMode == 4) {
        policy.AccentState = 0;
    } else if (styleMode == 0) {
        policy.AccentState = 2; // Прозрачная
        policy.AccentFlags = 2;
        policy.GradientColor = (DWORD(a) << 24) | (DWORD(b) << 16) | (DWORD(g) << 8) | DWORD(r);
    } else if (styleMode == 1) {
        policy.AccentState = 3; // Размытие (Aero Blur)
        policy.AccentFlags = 2;
        policy.GradientColor = (DWORD(a) << 24) | (DWORD(b) << 16) | (DWORD(g) << 8) | DWORD(r);
    } else if (styleMode == 2) {
        policy.AccentState = 4; // Акрил (Fluent Acrylic)
        policy.AccentFlags = 2;
        int effA = std::max(25, a);
        policy.GradientColor = (DWORD(effA) << 24) | (DWORD(b) << 16) | (DWORD(g) << 8) | DWORD(r);
    } else if (styleMode == 3) {
        policy.AccentState = 1; // Сплошной цвет
        policy.AccentFlags = 2;
        policy.GradientColor = (0xFFU << 24) | (DWORD(b) << 16) | (DWORD(g) << 8) | DWORD(r);
    }

    if (showBorder && policy.AccentState != 0) {
        policy.AccentFlags |= (0x20 | 0x40 | 0x80 | 0x100);
    }

    TB_WINCOMPATTRDATA data = {19, &policy, sizeof(policy)};
    setAttr(hwnd, &data);
}

static void moveChildIfChanged(HWND hwnd, HWND hInsertAfter, int x, int y, int w, int h) {
    if (!hwnd || !IsWindow(hwnd)) return;
    HWND parent = GetParent(hwnd);
    RECT r = {};
    GetWindowRect(hwnd, &r);
    POINT pt = { r.left, r.top };
    if (parent) ScreenToClient(parent, &pt);
    int curW = r.right - r.left;
    int curH = r.bottom - r.top;
    if (std::abs(int(pt.x) - x) > 1 || std::abs(int(pt.y) - y) > 1 ||
        std::abs(curW - w) > 2 || std::abs(curH - h) > 2) {
        UINT flags = SWP_NOACTIVATE | SWP_NOSENDCHANGING;
        if (!hInsertAfter) flags |= SWP_NOZORDER;
        SetWindowPos(hwnd, hInsertAfter, x, y, w, h, flags);
    }
}

static void invokeNativeTrayIconByKeyword(const wchar_t *kw1, const wchar_t *kw2) {
    HWND hTray = FindWindowW(L"Shell_TrayWnd", nullptr);
    HWND hNotify = hTray ? FindWindowExW(hTray, nullptr, L"TrayNotifyWnd", nullptr) : nullptr;
    HWND hPager = hNotify ? FindWindowExW(hNotify, nullptr, L"SysPager", nullptr) : nullptr;
    HWND hTb = hPager ? FindWindowExW(hPager, nullptr, L"ToolbarWindow32", nullptr) : nullptr;
    if (!hTb) return;

    IUIAutomation *pUia = nullptr;
    if (FAILED(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_IUIAutomation, reinterpret_cast<void **>(&pUia))) || !pUia)
        return;

    IUIAutomationElement *pRoot = nullptr;
    if (SUCCEEDED(pUia->ElementFromHandle(hTb, &pRoot)) && pRoot) {
        IUIAutomationCondition *pCond = nullptr;
        pUia->CreateTrueCondition(&pCond);
        IUIAutomationElementArray *pArr = nullptr;
        if (pCond) {
            pRoot->FindAll(TreeScope_Children, pCond, &pArr);
            pCond->Release();
        }
        if (pArr) {
            int len = 0; pArr->get_Length(&len);
            for (int i = 0; i < len; ++i) {
                IUIAutomationElement *pEl = nullptr;
                if (SUCCEEDED(pArr->GetElement(i, &pEl)) && pEl) {
                    BSTR bName = nullptr;
                    pEl->get_CurrentName(&bName);
                    QString nm = bName ? QString::fromWCharArray(bName) : QString();
                    if (bName) SysFreeString(bName);
                    if ((kw1 && nm.contains(QString::fromWCharArray(kw1), Qt::CaseInsensitive)) ||
                        (kw2 && nm.contains(QString::fromWCharArray(kw2), Qt::CaseInsensitive))) {
                        IUIAutomationLegacyIAccessiblePattern *pLeg = nullptr;
                        if (SUCCEEDED(pEl->GetCurrentPattern(UIA_LegacyIAccessiblePatternId, reinterpret_cast<IUnknown **>(&pLeg))) && pLeg) {
                            pLeg->DoDefaultAction();
                            pLeg->Release();
                            pEl->Release();
                            break;
                        }
                    }
                    pEl->Release();
                }
            }
            pArr->Release();
        }
        pRoot->Release();
    }
    pUia->Release();
}

static void sendWinKeyShortcut(WORD vkMod, WORD vkKey = 0) {
    if (vkKey == 0) {
        INPUT in[2] = {};
        in[0].type = INPUT_KEYBOARD; in[0].ki.wVk = vkMod;
        in[1].type = INPUT_KEYBOARD; in[1].ki.wVk = vkMod; in[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(2, in, sizeof(INPUT));
    } else {
        INPUT in[4] = {};
        in[0].type = INPUT_KEYBOARD; in[0].ki.wVk = vkMod;
        in[1].type = INPUT_KEYBOARD; in[1].ki.wVk = vkKey;
        in[2].type = INPUT_KEYBOARD; in[2].ki.wVk = vkKey; in[2].ki.dwFlags = KEYEVENTF_KEYUP;
        in[3].type = INPUT_KEYBOARD; in[3].ki.wVk = vkMod; in[3].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(4, in, sizeof(INPUT));
    }
}

// ============================================================================
// ГЛАДКАЯ ARGB-ПОДЛОЖКА КАПСУЛ ПАНЕЛИ ЗАДАЧ + НАСТОЯЩИЙ DWM БЛЮР / АКРИЛ
// ============================================================================
static HWND s_hPillBackdrop = nullptr;
static QString s_lastBackdropKey;

static void destroyPillBackdrop() {
    if (s_hPillBackdrop && IsWindow(s_hPillBackdrop)) DestroyWindow(s_hPillBackdrop);
    s_hPillBackdrop = nullptr;
    s_lastBackdropKey.clear();
}

static void updatePillBackdrop(HWND hTray, int barY, int barH, const QVector<QRect> &localPills, int radiusPx, int effStyle, const QColor &col, qreal opacity, bool showBorder) {
    int scrW = GetSystemMetrics(SM_CXSCREEN);
    if (localPills.isEmpty() || scrW <= 100 || barH <= 10) {
        destroyPillBackdrop();
        return;
    }
    static bool s_reg = false;
    if (!s_reg) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"WinWalPillBackdropStrip";
        RegisterClassExW(&wc);
        s_reg = true;
    }
    if (!s_hPillBackdrop || !IsWindow(s_hPillBackdrop)) {
        s_hPillBackdrop = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST,
            L"WinWalPillBackdropStrip", L"", WS_POPUP,
            0, barY, scrW, barH, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        s_lastBackdropKey.clear();
    }

    // Родной фон Shell_TrayWnd всегда держим на 100% прозрачным и БЕЗ SetWindowRgn,
    // чтобы выделение иконок никогда не резалось пиксельной лесенкой GDI и прямоугольный блюр не вылезал наружу!
    SetWindowRgn(hTray, nullptr, FALSE);
    setHwndAccent(hTray, 0, Qt::black, 0.0, false);

    QString key = QString("%1_%2_%3_%4_%5_%6_%7").arg(barY).arg(barH).arg(radiusPx).arg(effStyle).arg(col.name()).arg(int(opacity * 100)).arg(showBorder ? 1 : 0);
    for (const QRect &r : localPills) key += QString("_%1_%2_%3_%4").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());

    if (key != s_lastBackdropKey) {
        s_lastBackdropKey = key;
        QImage img(scrW, barH, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);

        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);

        QColor fillCol = col.isValid() ? col : QColor(10, 11, 15);
        double fillAlpha = double(opacity);
        if (effStyle == 3) fillAlpha = std::max(0.94, fillAlpha);
        else if (effStyle == 2) fillAlpha = std::clamp(fillAlpha, 0.48, 0.94);
        else if (effStyle == 1) fillAlpha = std::clamp(fillAlpha, 0.34, 0.92);
        else fillAlpha = std::clamp(fillAlpha, 0.0, 0.96);

        fillCol.setAlphaF(fillAlpha);
        QColor borderCol = showBorder ? QColor(255, 255, 255, 95) : (fillAlpha > 0.02 || effStyle > 0 ? QColor(255, 255, 255, 28) : Qt::transparent);

        p.setBrush(fillAlpha > 0.005 ? QBrush(fillCol) : Qt::NoBrush);
        p.setPen(borderCol.alpha() > 0 ? QPen(borderCol, 1.0) : Qt::NoPen);

        for (const QRect &pr : localPills) {
            if (pr.width() <= 6 || pr.height() <= 6) continue;
            double rad = std::min({double(radiusPx), pr.width() * 0.5, pr.height() * 0.5});
            p.drawRoundedRect(QRectF(pr).adjusted(0.5, 0.5, -0.5, -0.5), rad, rad);
        }
        p.end();

        HDC hdcScreen = GetDC(nullptr);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = scrW;
        bmi.bmiHeader.biHeight = -barH;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        void *pvBits = nullptr;
        HBITMAP hBmp = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pvBits, nullptr, 0);
        if (hBmp && pvBits) {
            memcpy(pvBits, img.constBits(), size_t(scrW * barH * 4));
            HGDIOBJ old = SelectObject(hdcMem, hBmp);
            POINT ptDst = { 0, barY };
            SIZE szDst = { scrW, barH };
            POINT ptSrc = { 0, 0 };
            BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
            UpdateLayeredWindow(s_hPillBackdrop, hdcScreen, &ptDst, &szDst, hdcMem, &ptSrc, 0, &bf, ULW_ALPHA);
            SelectObject(hdcMem, old);
            DeleteObject(hBmp);
        }
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);

        // Включаем настоящее аппаратное DWM-размытие СТРОГО внутри контура наших капсул (с отступом 1px внутрь гладкой рамки!)
        if (effStyle == 1 || effStyle == 2) {
            HRGN hBlurRgn = nullptr;
            int d = std::max(0, radiusPx - 1) * 2;
            for (const QRect &pr : localPills) {
                QRect inR = pr.adjusted(1, 1, -1, -1);
                if (inR.width() <= 4 || inR.height() <= 4) continue;
                HRGN hR = (d > 0)
                    ? CreateRoundRectRgn(inR.left(), inR.top(), inR.right() + 1, inR.bottom() + 1, d, d)
                    : CreateRectRgn(inR.left(), inR.top(), inR.right() + 1, inR.bottom() + 1);
                if (!hBlurRgn) hBlurRgn = hR;
                else { CombineRgn(hBlurRgn, hBlurRgn, hR, RGN_OR); DeleteObject(hR); }
            }
            DWM_BLURBEHIND bb = {};
            bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
            bb.fEnable = (hBlurRgn != nullptr) ? TRUE : FALSE;
            bb.hRgnBlur = hBlurRgn;
            DwmEnableBlurBehindWindow(s_hPillBackdrop, &bb);
            if (hBlurRgn) DeleteObject(hBlurRgn);
        } else {
            DWM_BLURBEHIND bb = {};
            bb.dwFlags = DWM_BB_ENABLE;
            bb.fEnable = FALSE;
            DwmEnableBlurBehindWindow(s_hPillBackdrop, &bb);
        }
    }

    if (!IsWindowVisible(s_hPillBackdrop)) ShowWindow(s_hPillBackdrop, SW_SHOWNA);
    if (GetWindow(hTray, GW_HWNDNEXT) != s_hPillBackdrop) {
        SetWindowPos(s_hPillBackdrop, hTray, 0, barY, scrW, barH, SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING);
    }
}
// ============================================================================
// ВИРТУАЛЬНЫЕ РАБОЧИЕ СТОЛЫ (1..5) + ПЛАВНЫЙ 120 FPS ПЕРЕХОД + ПРИВЯЗКА WINWAL
// ============================================================================
static int s_currentWorkspace = 1;
static bool s_workspacesEnabledGlobal = true;
static QMap<int, QSet<quintptr>> s_workspaceWindows;
static int s_winWalPinnedWorkspace = -1;
static HWND s_hWsBar = nullptr;
static QString s_lastWsRenderKey;
static HHOOK s_kbWorkspaceHook = nullptr;
static bool s_topSplitActive = false;
static int s_topRightPillLeft = 1650;

std::atomic<bool> g_workspaceGlideActive{false};
struct GlidingWinEntry {
    HWND hwnd;
    RECT origRect;
};
static QVector<GlidingWinEntry> s_glidingWindows;
static QTimer *s_slideBannerTimer = nullptr;
static QElapsedTimer s_slideClock;
static int s_slideDir = 1;
static int s_slideTargetWs = 1;
static double s_wsAnimFrom = 1.0;
static double s_wsAnimCurrent = 1.0;
static double s_wsAnimStretch = 0.0;

static int s_lastWsX = 0;
static int s_lastWsY = 0;
static int s_lastWsH = 30;
static int s_lastWsRadius = 0;
static int s_lastWsStyle = 1;
static QColor s_lastWsColor = QColor("#0a0a0c");
static qreal s_lastWsOpacity = 0.0;
static bool s_lastWsBorder = false;

static void updateWsOrTopBarOverlay(bool fullTopBar, int x, int y, int h, int radiusPx, int effStyle, const QColor &bgCol, qreal opacity, bool showBorder);

static QWindow *findMainWinWalWindow() {
    for (QWindow *w : QGuiApplication::topLevelWindows()) {
        if (w && w->objectName() == "mainControlWindow") {
            return w;
        }
    }
    return nullptr;
}

static HWND findMainWinWalHwnd() {
    if (QWindow *w = findMainWinWalWindow()) {
        return reinterpret_cast<HWND>(w->winId());
    }
    return nullptr;
}

static void finishWindowGlideInstant() {
    if (s_glidingWindows.isEmpty()) {
        g_workspaceGlideActive = false;
        return;
    }
    HDWP hdwp = BeginDeferWindowPos(s_glidingWindows.size());
    for (const GlidingWinEntry &e : s_glidingWindows) {
        if (IsWindow(e.hwnd) && !IsZoomed(e.hwnd)) {
            int w = e.origRect.right - e.origRect.left;
            int h = e.origRect.bottom - e.origRect.top;
            if (hdwp) {
                hdwp = DeferWindowPos(hdwp, e.hwnd, nullptr, e.origRect.left, e.origRect.top, w, h,
                                      SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
            } else {
                SetWindowPos(e.hwnd, nullptr, e.origRect.left, e.origRect.top, w, h,
                             SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
            }
        }
    }
    if (hdwp) EndDeferWindowPos(hdwp);
    s_glidingWindows.clear();
    g_workspaceGlideActive = false;
}

static void updateSlideBannerFrame() {
    if (s_isShuttingDown) return;
    const double durationMs = 240.0;
    double t = std::clamp(double(s_slideClock.elapsed()) / durationMs, 0.0, 1.0);

    double inv = 1.0 - t;
    double ease = 1.0 - (inv * inv * inv * inv);

    s_wsAnimCurrent = s_wsAnimFrom + (double(s_currentWorkspace) - s_wsAnimFrom) * ease;
    s_wsAnimStretch = std::sin(t * 3.14159265) * 0.42 * std::min(2.0, std::abs(double(s_currentWorkspace) - s_wsAnimFrom));
    if (s_hWsBar && IsWindow(s_hWsBar)) {
        updateWsOrTopBarOverlay(s_topSplitActive, s_lastWsX, s_lastWsY, s_lastWsH,
                                s_lastWsRadius, s_lastWsStyle, s_lastWsColor, s_lastWsOpacity, s_lastWsBorder);
    }

    if (t >= 1.0) {
        s_wsAnimCurrent = double(s_currentWorkspace);
        s_wsAnimStretch = 0.0;
        if (s_slideBannerTimer) s_slideBannerTimer->stop();
        finishWindowGlideInstant();
        if (s_hWsBar && IsWindow(s_hWsBar)) {
            updateWsOrTopBarOverlay(s_topSplitActive, s_lastWsX, s_lastWsY, s_lastWsH,
                                    s_lastWsRadius, s_lastWsStyle, s_lastWsColor, s_lastWsOpacity, s_lastWsBorder);
        }
        return;
    }

    // Плавно скользят ТОЛЬКО сами окна нового рабочего стола (обои и часы стоят 100% неподвижно!)
    if (!s_glidingWindows.isEmpty()) {
        int offsetX = int(std::round(s_slideDir * 96.0 * (1.0 - ease)));
        HDWP hdwp = BeginDeferWindowPos(s_glidingWindows.size());
        for (const GlidingWinEntry &e : s_glidingWindows) {
            if (IsWindow(e.hwnd) && !IsZoomed(e.hwnd)) {
                int w = e.origRect.right - e.origRect.left;
                int h = e.origRect.bottom - e.origRect.top;
                if (hdwp) {
                    hdwp = DeferWindowPos(hdwp, e.hwnd, nullptr, e.origRect.left + offsetX, e.origRect.top, w, h,
                                          SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
                } else {
                    SetWindowPos(e.hwnd, nullptr, e.origRect.left + offsetX, e.origRect.top, w, h,
                                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
                }
            }
        }
        if (hdwp) EndDeferWindowPos(hdwp);
    }
}

static bool isManageableWorkspaceWindow(HWND hwnd) {
    if (!IsWindow(hwnd) || !IsWindowVisible(hwnd) || IsIconic(hwnd)) return false;
    if (hwnd == findMainWinWalHwnd()) return false;

    INT cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked != 0)
        return false;
    LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if ((ex & WS_EX_TOOLWINDOW) || (ex & WS_EX_TRANSPARENT) || (ex & WS_EX_NOACTIVATE) || (ex & WS_EX_TOPMOST))
        return false;
    wchar_t cls[128] = {};
    GetClassNameW(hwnd, cls, 127);
    if (wcscmp(cls, L"Progman") == 0 || wcscmp(cls, L"WorkerW") == 0 ||
        wcscmp(cls, L"Shell_TrayWnd") == 0 || wcscmp(cls, L"Shell_SecondaryTrayWnd") == 0 ||
        wcscmp(cls, L"Windows.UI.Core.CoreWindow") == 0 || wcscmp(cls, L"WinWalDwmGlassSheet") == 0)
        return false;
    wchar_t title[128] = {};
    GetWindowTextW(hwnd, title, 127);
    if (wcslen(title) == 0 || wcscmp(title, L"WinWalDesktopWallpaper") == 0 || wcscmp(title, L"WinWalAtmosphereOverlay") == 0)
        return false;
    return true;
}

static void restoreAllWorkspacesOnExit() {
    if (s_slideBannerTimer) s_slideBannerTimer->stop();
    finishWindowGlideInstant();
    for (auto mapIt = s_workspaceWindows.begin(); mapIt != s_workspaceWindows.end(); ++mapIt) {
        for (quintptr raw : mapIt.value()) {
            HWND h = reinterpret_cast<HWND>(raw);
            if (IsWindow(h)) ShowWindow(h, SW_SHOWNA);
        }
    }
    s_workspaceWindows.clear();
    s_winWalPinnedWorkspace = -1;
    s_currentWorkspace = 1;
    s_wsAnimFrom = 1.0;
    s_wsAnimCurrent = 1.0;
    s_wsAnimStretch = 0.0;
}

static void switchVirtualWorkspace(int targetWs) {
    if (!s_workspacesEnabledGlobal || s_isShuttingDown) return;
    targetWs = std::clamp(targetWs, 1, 5);
    if (targetWs == s_currentWorkspace) return;

    finishWindowGlideInstant();

    s_slideDir = (targetWs > s_currentWorkspace) ? 1 : -1;
    s_slideTargetWs = targetWs;
    s_wsAnimFrom = s_wsAnimCurrent;

    if (QWindow *mainWin = findMainWinWalWindow()) {
        if (mainWin->isVisible() && mainWin->visibility() != QWindow::Minimized) {
            s_winWalPinnedWorkspace = s_currentWorkspace;
            mainWin->setProperty("hiddenByWorkspace", true);
            mainWin->hide();
        } else if (s_winWalPinnedWorkspace == targetWs) {
            s_winWalPinnedWorkspace = -1;
            mainWin->setProperty("hiddenByWorkspace", false);
            mainWin->show();
            mainWin->raise();
            mainWin->requestActivate();
        }
    }

    QSet<quintptr> curSet;
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        if (isManageableWorkspaceWindow(hwnd)) {
            reinterpret_cast<QSet<quintptr>*>(lParam)->insert(reinterpret_cast<quintptr>(hwnd));
            ShowWindowAsync(hwnd, SW_HIDE);
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&curSet));
    s_workspaceWindows[s_currentWorkspace] = curSet;

    s_currentWorkspace = targetWs;
    s_lastWsRenderKey.clear();

    g_workspaceGlideActive = true;
    HWND lastShown = nullptr;
    for (quintptr raw : s_workspaceWindows.value(targetWs)) {
        HWND h = reinterpret_cast<HWND>(raw);
        if (IsWindow(h)) {
            if (!IsZoomed(h)) {
                RECT r = {};
                GetWindowRect(h, &r);
                s_glidingWindows.append(GlidingWinEntry{h, r});
                SetWindowPos(h, nullptr, r.left + s_slideDir * 96, r.top, r.right - r.left, r.bottom - r.top,
                             SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
            }
            ShowWindowAsync(h, SW_SHOWNA);
            lastShown = h;
        }
    }
    s_workspaceWindows[targetWs].clear();
    if (lastShown) SetForegroundWindow(lastShown);

    if (!s_slideBannerTimer) {
        s_slideBannerTimer = new QTimer(qApp);
        s_slideBannerTimer->setTimerType(Qt::PreciseTimer);
        s_slideBannerTimer->setInterval(8);
        QObject::connect(s_slideBannerTimer, &QTimer::timeout, &updateSlideBannerFrame);
    }
    s_slideClock.restart();
    updateSlideBannerFrame();
    s_slideBannerTimer->start();
}

static void moveActiveWindowToWorkspace(int targetWs) {
    if (!s_workspacesEnabledGlobal || s_isShuttingDown) return;
    targetWs = std::clamp(targetWs, 1, 5);
    if (targetWs == s_currentWorkspace) return;
    finishWindowGlideInstant();
    HWND fg = GetForegroundWindow();
    if (!fg) return;

    if (fg == findMainWinWalHwnd()) {
        if (QWindow *mainWin = findMainWinWalWindow()) {
            s_winWalPinnedWorkspace = targetWs;
            mainWin->setProperty("hiddenByWorkspace", true);
            mainWin->hide();
        }
        return;
    }

    if (!isManageableWorkspaceWindow(fg)) return;
    ShowWindowAsync(fg, SW_HIDE);
    s_workspaceWindows[targetWs].insert(reinterpret_cast<quintptr>(fg));
}
static LRESULT CALLBACK LowLevelKbWorkspaceProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (s_workspacesEnabledGlobal && !s_isShuttingDown && nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        auto *kb = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
        if (kb && kb->vkCode >= '1' && kb->vkCode <= '5') {
            bool winHeld = ((GetAsyncKeyState(VK_LWIN) & 0x8000) != 0) || ((GetAsyncKeyState(VK_RWIN) & 0x8000) != 0);
            if (winHeld) {
                int ws = int(kb->vkCode - '0');
                bool shiftHeld = ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
                INPUT dummy = {};
                dummy.type = INPUT_KEYBOARD;
                dummy.ki.wVk = 0xFF;
                SendInput(1, &dummy, sizeof(INPUT));
                dummy.ki.dwFlags = KEYEVENTF_KEYUP;
                SendInput(1, &dummy, sizeof(INPUT));

                QMetaObject::invokeMethod(qApp, [ws, shiftHeld]() {
                    if (shiftHeld) moveActiveWindowToWorkspace(ws);
                    else switchVirtualWorkspace(ws);
                }, Qt::QueuedConnection);
                return 1;
            }
        }
    }
    return CallNextHookEx(s_kbWorkspaceHook, nCode, wParam, lParam);
}

static LRESULT CALLBACK WsBarWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (msg == WM_LBUTTONDOWN) {
        int x = LOWORD(lParam);
        if (s_topSplitActive) {
            if (x >= 12 && x <= 48) {
                sendWinKeyShortcut(VK_LWIN);
                return 0;
            }
            if (s_workspacesEnabledGlobal && x > 48 && x < 48 + 5 * 24 + 8) {
                int idx = (x - 50) / 24 + 1;
                if (idx >= 1 && idx <= 5) switchVirtualWorkspace(idx);
                return 0;
            }
            if (x >= s_topRightPillLeft) {
                int relRight = x - s_topRightPillLeft;
                if (relRight < 34) {
                    if (HWND hOv = FindWindowW(L"NotifyIconOverflowWindow", nullptr)) {
                        ShowWindow(hOv, IsWindowVisible(hOv) ? SW_HIDE : SW_SHOWNA);
                    }
                } else if (relRight < 62) {
                    ShellExecuteW(nullptr, L"open", L"ms-availablenetworks:", nullptr, nullptr, SW_SHOWNORMAL);
                } else if (relRight < 90) {
                    invokeNativeTrayIconByKeyword(L"Динамики", L"Speakers");
                } else if (relRight < 130) {
                    sendWinKeyShortcut(VK_LWIN, VK_SPACE);
                } else if (relRight < 186) {
                    INPUT in[6] = {};
                    in[0].type = INPUT_KEYBOARD; in[0].ki.wVk = VK_LWIN;
                    in[1].type = INPUT_KEYBOARD; in[1].ki.wVk = VK_MENU;
                    in[2].type = INPUT_KEYBOARD; in[2].ki.wVk = 'D';
                    in[3].type = INPUT_KEYBOARD; in[3].ki.wVk = 'D'; in[3].ki.dwFlags = KEYEVENTF_KEYUP;
                    in[4].type = INPUT_KEYBOARD; in[4].ki.wVk = VK_MENU; in[4].ki.dwFlags = KEYEVENTF_KEYUP;
                    in[5].type = INPUT_KEYBOARD; in[5].ki.wVk = VK_LWIN; in[5].ki.dwFlags = KEYEVENTF_KEYUP;
                    SendInput(6, in, sizeof(INPUT));
                } else {
                    sendWinKeyShortcut(VK_LWIN, 'A');
                }
                return 0;
            }
        } else {
            int idx = (x - 6) / 24 + 1;
            if (idx >= 1 && idx <= 5) switchVirtualWorkspace(idx);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void destroyWsOverlay() {
    if (s_hWsBar && IsWindow(s_hWsBar)) DestroyWindow(s_hWsBar);
    s_hWsBar = nullptr;
    s_lastWsRenderKey.clear();
}

static void drawWorkspacesButtonsAndPill(QPainter &p, double baseX, int h) {
    QFont f("Segoe UI", 9, QFont::Bold);
    f.setPixelSize(11);
    p.setFont(f);

    double itemY = (h - 20.0) * 0.5;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 32));
    for (int i = 1; i <= 5; ++i) {
        QRectF br(baseX + (i - 1) * 24.0, itemY, 20.0, 20.0);
        p.drawRoundedRect(br, 6.0, 6.0);
    }

    double pillCenter = baseX + (s_wsAnimCurrent - 1.0) * 24.0 + 10.0;
    double pillW = 20.0 + s_wsAnimStretch * 14.0;
    QRectF activePill(pillCenter - pillW * 0.5, itemY, pillW, 20.0);
    p.setBrush(QColor(255, 255, 255, 238));
    p.drawRoundedRect(activePill, 6.0, 6.0);

    for (int i = 1; i <= 5; ++i) {
        QRectF br(baseX + (i - 1) * 24.0, itemY, 20.0, 20.0);
        double dist = std::clamp(std::abs(double(i) - s_wsAnimCurrent), 0.0, 1.0);
        int r = int(std::round(12.0 + (215.0 - 12.0) * dist));
        int g = int(std::round(14.0 + (225.0 - 14.0) * dist));
        int b = int(std::round(20.0 + (240.0 - 20.0) * dist));
        int a = int(std::round(255.0 - 35.0 * dist));
        p.setPen(QColor(r, g, b, a));
        p.drawText(br, Qt::AlignCenter, QString::number(i));
    }
}

static void updateWsOrTopBarOverlay(bool fullTopBar, int x, int y, int h, int radiusPx, int effStyle, const QColor &bgCol, qreal opacity, bool showBorder) {
    s_topSplitActive = fullTopBar;
    s_lastWsX = x;
    s_lastWsY = y;
    s_lastWsH = h;
    s_lastWsRadius = radiusPx;
    s_lastWsStyle = effStyle;
    s_lastWsColor = bgCol;
    s_lastWsOpacity = opacity;
    s_lastWsBorder = showBorder;

    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int w = fullTopBar ? scrW : (5 * 24 + 12);
    int winX = fullTopBar ? 0 : x;

    static bool s_reg = false;
    if (!s_reg) {
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = WsBarWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_HAND);
        wc.lpszClassName = L"WinWalWorkspacePill";
        RegisterClassExW(&wc);
        s_reg = true;
    }
    if (!s_hWsBar || !IsWindow(s_hWsBar)) {
        s_hWsBar = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST,
            L"WinWalWorkspacePill", L"", WS_POPUP,
            winX, y, w, h, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        s_lastWsRenderKey.clear();
    }

    QString timeStr = QDateTime::currentDateTime().toString("HH:mm");
    HWND fg = GetForegroundWindow();
    DWORD tid = fg ? GetWindowThreadProcessId(fg, nullptr) : 0;
    bool isRu = (LOWORD(reinterpret_cast<quintptr>(GetKeyboardLayout(tid))) == 0x0419);
    QString langStr = isRu ? "РУС" : "ENG";

    int animStep = int(std::round(s_wsAnimCurrent * 100.0));
    int stretchStep = int(std::round(s_wsAnimStretch * 100.0));
    QString renderKey = QString("%1_%2_%3_%4_%5_%6_%7_%8_%9_%10_%11_%12")
        .arg(fullTopBar ? 1 : 0).arg(s_workspacesEnabledGlobal ? animStep : 0).arg(stretchStep)
        .arg(winX).arg(y).arg(w).arg(h).arg(radiusPx).arg(effStyle).arg(bgCol.name()).arg(int(opacity * 100))
        .arg(fullTopBar ? (langStr + timeStr) : QString());

    if (renderKey != s_lastWsRenderKey) {
        s_lastWsRenderKey = renderKey;
        QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);

        QColor fillCol = bgCol.isValid() ? bgCol : QColor(10, 11, 15);
        fillCol.setAlphaF(std::clamp(double(opacity), 0.30, 0.96));
        QColor borderCol = showBorder ? QColor(255, 255, 255, 75) : QColor(255, 255, 255, 24);

        if (fullTopBar) {
            int wsW = s_workspacesEnabledGlobal ? (5 * 24 + 8) : 0;
            int leftPillW = 38 + wsW + 6;
            QRectF rLeft(12.5, 0.5, leftPillW, h - 1.0);
            double rad = std::min({double(radiusPx), leftPillW * 0.5, h * 0.5});
            p.setBrush(fillCol);
            p.setPen(borderCol.alpha() > 0 ? QPen(borderCol, 1.0) : Qt::NoPen);
            p.drawRoundedRect(rLeft, rad, rad);

            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 255, 255, 240));
            int cy = h / 2;
            p.drawRect(24, cy - 6, 5, 5);
            p.drawRect(31, cy - 6, 5, 5);
            p.drawRect(24, cy + 1, 5, 5);
            p.drawRect(31, cy + 1, 5, 5);

            if (s_workspacesEnabledGlobal) {
                drawWorkspacesButtonsAndPill(p, 48.0, h);
            }

            int rightPillW = 228;
            s_topRightPillLeft = scrW - rightPillW - 12;
            QRectF rRight(s_topRightPillLeft + 0.5, 0.5, rightPillW, h - 1.0);
            p.setBrush(fillCol);
            p.setPen(borderCol.alpha() > 0 ? QPen(borderCol, 1.0) : Qt::NoPen);
            p.drawRoundedRect(rRight, rad, rad);

            QFont iconFont("Segoe MDL2 Assets");
            iconFont.setPixelSize(13);
            p.setFont(iconFont);
            p.setPen(QColor(250, 252, 255, 245));
            p.drawText(QRectF(s_topRightPillLeft + 8, 0, 24, h), Qt::AlignCenter, QString(QChar(0xE70E)));
            p.drawText(QRectF(s_topRightPillLeft + 34, 0, 26, h), Qt::AlignCenter, QString(QChar(0xE839)));
            p.drawText(QRectF(s_topRightPillLeft + 62, 0, 26, h), Qt::AlignCenter, QString(QChar(0xE767)));
            p.drawText(QRectF(s_topRightPillLeft + 188, 0, 30, h), Qt::AlignCenter, QString(QChar(0xE91C)));

            QFont tf("Segoe UI", 9, QFont::DemiBold);
            tf.setPixelSize(12);
            p.setFont(tf);
            p.drawText(QRectF(s_topRightPillLeft + 90, 0, 38, h), Qt::AlignCenter, langStr);
            p.drawText(QRectF(s_topRightPillLeft + 130, 0, 54, h), Qt::AlignCenter, timeStr);
        } else {
            drawWorkspacesButtonsAndPill(p, 6.0, h);
        }
        p.end();

        HDC hdcScreen = GetDC(nullptr);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = -h;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        void *pvBits = nullptr;
        HBITMAP hBmp = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pvBits, nullptr, 0);
        if (hBmp && pvBits) {
            memcpy(pvBits, img.constBits(), size_t(w * h * 4));
            HGDIOBJ old = SelectObject(hdcMem, hBmp);
            POINT ptDst = { winX, y };
            SIZE szDst = { w, h };
            POINT ptSrc = { 0, 0 };
            BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
            UpdateLayeredWindow(s_hWsBar, hdcScreen, &ptDst, &szDst, hdcMem, &ptSrc, 0, &bf, ULW_ALPHA);
            SelectObject(hdcMem, old);
            DeleteObject(hBmp);
        }
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
    }

    if (!IsWindowVisible(s_hWsBar)) ShowWindow(s_hWsBar, SW_SHOWNA);
    SetWindowPos(s_hWsBar, topZOrderTarget(), winX, y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_NOSENDCHANGING);
}

static void applyFlyoutGlassEffect(HWND hwnd, bool isUwpCoreWindow, bool enable, int styleMode, const QColor &col, qreal opacity) {
    if (!hwnd || !IsWindow(hwnd)) return;
    if (!enable) {
        if (!isUwpCoreWindow) setHwndAccent(hwnd, 4, Qt::black, 0.0, false);
        LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        if (ex & WS_EX_LAYERED) SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
        return;
    }
    if (!isUwpCoreWindow) {
        setHwndAccent(hwnd, styleMode, col, opacity, true);
    } else {
        setHwndAccent(hwnd, 4, Qt::black, 0.0, false);
    }
    LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (!(ex & WS_EX_LAYERED)) SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex | WS_EX_LAYERED);
    BYTE alpha = static_cast<BYTE>(std::clamp(int(std::round((0.30 + opacity * 0.70) * 255.0)), 85, 255));
    SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA);
}

static bool hasAnyMaximizedWindow() {
    bool foundMax = false;
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        if (!IsWindowVisible(hwnd) || IsIconic(hwnd) || !IsZoomed(hwnd)) return TRUE;
        INT cloaked = 0;
        if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked != 0)
            return TRUE;
        LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        if ((ex & WS_EX_TRANSPARENT) || (ex & WS_EX_TOOLWINDOW)) return TRUE;
        wchar_t cls[128] = {};
        GetClassNameW(hwnd, cls, 127);
        QString cName = QString::fromWCharArray(cls);
        if (cName == "Progman" || cName == "WorkerW" || cName == "Shell_TrayWnd" ||
            cName == "Windows.UI.Core.CoreWindow") return TRUE;
        *reinterpret_cast<bool *>(lParam) = true;
        return FALSE;
    }, reinterpret_cast<LPARAM>(&foundMax));
    return foundMax;
}

// ТОЧНЫЙ ЗАМЕР ВСЕХ ВИДИМЫХ ИКОНОК ТАСКБАРА (0px лишнего места справа + 0% обрезки на любых ПК)
static int measureTaskListIcons(HWND hTaskList, int *outFirstOffset) {
    if (outFirstOffset) *outFirstOffset = 0;
    if (!hTaskList || !IsWindow(hTaskList)) return 240;

    static bool s_coInit = false;
    if (!s_coInit) {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        s_coInit = true;
    }

    int uiaSpan = 0;
    static IUIAutomation *s_pUia = nullptr;
    if (!s_pUia) {
        CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                         IID_IUIAutomation, reinterpret_cast<void **>(&s_pUia));
    }
    if (s_pUia) {
        IUIAutomationElement *pRoot = nullptr;
        if (SUCCEEDED(s_pUia->ElementFromHandle(hTaskList, &pRoot)) && pRoot) {
            IUIAutomationCondition *pTrueCond = nullptr;
            s_pUia->CreateTrueCondition(&pTrueCond);
            IUIAutomationElementArray *pArr = nullptr;
            if (pTrueCond) {
                pRoot->FindAll(TreeScope_Children, pTrueCond, &pArr);
                pTrueCond->Release();
            }
            if (pArr) {
                int len = 0;
                pArr->get_Length(&len);
                for (int i = 0; i < len; ++i) {
                    IUIAutomationElement *pChild = nullptr;
                    if (SUCCEEDED(pArr->GetElement(i, &pChild)) && pChild) {
                        CONTROLTYPEID ctrlType = 0;
                        pChild->get_CurrentControlType(&ctrlType);
                        BSTR bName = nullptr;
                        pChild->get_CurrentName(&bName);
                        bool hasName = (bName != nullptr && SysStringLen(bName) > 0);
                        if (bName) SysFreeString(bName);

                        if (hasName && (ctrlType == UIA_ButtonControlTypeId || ctrlType == UIA_MenuItemControlTypeId)) {
                            RECT br = {};
                            if (SUCCEEDED(pChild->get_CurrentBoundingRectangle(&br))) {
                                int w = br.right - br.left;
                                int h = br.bottom - br.top;
                                if (w >= 16 && w < 320 && h >= 16 && h < 320) {
                                    uiaSpan += w;
                                }
                            }
                        }
                        pChild->Release();
                    }
                }
                pArr->Release();
            }
            pRoot->Release();
        }
    }

    // Проверяем через MSAA общее число реальных кнопок задач (чтобы при открытии нового приложения капсула сразу расширялась!)
    IAccessible *pAcc = nullptr;
    if (SUCCEEDED(AccessibleObjectFromWindow(hTaskList, DWORD(OBJID_CLIENT), __uuidof(IAccessible), reinterpret_cast<void **>(&pAcc))) && pAcc) {
        long childCount = 0;
        pAcc->get_accChildCount(&childCount);
        if (childCount > 0 && childCount < 128) {
            std::vector<VARIANT> vars(childCount);
            for (long i = 0; i < childCount; ++i) VariantInit(&vars[i]);
            long obtained = 0;
            if (SUCCEEDED(AccessibleChildren(pAcc, 0, childCount, vars.data(), &obtained))) {
                int msaaVisBtns = 0;
                int singleBtnW = 40;
                for (long i = 0; i < obtained; ++i) {
                    if (vars[i].vt == VT_I4) {
                        VARIANT vRole, vState;
                        VariantInit(&vRole); VariantInit(&vState);
                        pAcc->get_accRole(vars[i], &vRole);
                        pAcc->get_accState(vars[i], &vState);
                        long roleVal = (vRole.vt == VT_I4) ? vRole.lVal : 0;
                        long stateVal = (vState.vt == VT_I4) ? vState.lVal : 0;
                        VariantClear(&vRole); VariantClear(&vState);
                        if ((stateVal & STATE_SYSTEM_INVISIBLE) == 0 &&
                            (roleVal == ROLE_SYSTEM_PUSHBUTTON || roleVal == ROLE_SYSTEM_BUTTONMENU || roleVal == 0x2B)) {
                            long l = 0, t = 0, w = 0, h = 0;
                            if (SUCCEEDED(pAcc->accLocation(&l, &t, &w, &h, vars[i])) && h >= 16) {
                                msaaVisBtns++;
                                if (w >= 24 && w <= 64) singleBtnW = int(w);
                            }
                        }
                    }
                    VariantClear(&vars[i]);
                }
                if (msaaVisBtns * singleBtnW > uiaSpan) {
                    uiaSpan = msaaVisBtns * singleBtnW;
                }
            }
        }
        pAcc->Release();
    }

    if (uiaSpan > 0) {
        return std::clamp(uiaSpan + 4, 36, 2400);
    }
    return 160;
}
static int measureTrayNotifyWidth(HWND hNotify, bool hideShowDesktopLine = true) {
    if (!hNotify || !IsWindow(hNotify)) return 210;
    if (HWND hDeskBtn = FindWindowExW(hNotify, nullptr, L"TrayShowDesktopButtonWClass", nullptr)) {
        ShowWindow(hDeskBtn, SW_SHOWNA);
    }
    int sumW = 0;
    HWND hChild = nullptr;
    while ((hChild = FindWindowExW(hNotify, hChild, nullptr, nullptr)) != nullptr) {
        if (!IsWindowVisible(hChild)) continue;
        wchar_t cls[64] = {};
        GetClassNameW(hChild, cls, 63);
        
        RECT cr = {};
        GetWindowRect(hChild, &cr);
        int cw = cr.right - cr.left;
        if (cw > 0 && cw < 400) sumW += cw;
    }
    if (sumW >= 60) return std::clamp(sumW + 4, 96, 520);
    RECT nr = {};
    GetWindowRect(hNotify, &nr);
    return std::clamp(int(nr.right - nr.left), 110, 480);
}

static RECT s_lastAppliedWorkArea = { -1, -1, -1, -1 };

struct WorkAreaResizeCtx {
    RECT newWa;
    RECT oldWa;
};

static void applyDesktopWorkArea(int topReserve, int bottomReserve) {
    // Не дёргаем SPI_SETWORKAREA, пока зажата левая кнопка мыши на ползунке (применяем сразу при отпускании!)
    if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 && s_lastAppliedWorkArea.bottom != -1) {
        return;
    }
    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);
    RECT wa = { 0, topReserve, scrW, scrH - bottomReserve };
    if (wa.left == s_lastAppliedWorkArea.left && wa.top == s_lastAppliedWorkArea.top &&
        wa.right == s_lastAppliedWorkArea.right && wa.bottom == s_lastAppliedWorkArea.bottom) {
        return;
    }
    RECT prevWa = s_lastAppliedWorkArea;
    if (prevWa.bottom <= 0) prevWa = wa;
    s_lastAppliedWorkArea = wa;

    QWindow *mw = findMainWinWalWindow();
    bool mwWasVis = (mw && mw->isVisible());
    SystemParametersInfoW(SPI_SETWORKAREA, 0, &wa, 0);

    WorkAreaResizeCtx ctx{wa, prevWa};
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        if (!isManageableWorkspaceWindow(hwnd)) return TRUE;
        const auto *c = reinterpret_cast<const WorkAreaResizeCtx *>(lParam);
        const RECT *wArea = &c->newWa;
        const RECT *oldArea = &c->oldWa;

        RECT r = {};
        GetWindowRect(hwnd, &r);
        int w = r.right - r.left, h = r.bottom - r.top;
        int scrW = GetSystemMetrics(SM_CXSCREEN), scrH = GetSystemMetrics(SM_CYSCREEN);
        if (w >= scrW && h >= scrH - 1 && !(GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_CAPTION)) return TRUE;

        if (IsZoomed(hwnd)) {
            ShowWindow(hwnd, SW_RESTORE);
            ShowWindow(hwnd, SW_SHOWMAXIMIZED);
        } else {
            // Учитываем невидимую теневую рамку Windows 10 снизу (обычно +7px)
            RECT frameR = r;
            int padB = 0;
            if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &frameR, sizeof(frameR)))) {
                padB = std::clamp(int(r.bottom - frameR.bottom), 0, 16);
            }
            int visTop = r.top;
            int visBot = r.bottom - padB;

            int newTop = visTop;
            int newVisBot = visBot;

            if (newTop < wArea->top) {
                int shift = wArea->top - newTop;
                newTop = wArea->top;
                newVisBot = std::min(int(wArea->bottom), newVisBot + shift);
            } else if (oldArea->top > wArea->top && std::abs(visTop - oldArea->top) <= 24) {
                newTop = wArea->top;
            }

            if (newVisBot > wArea->bottom) {
                // Зазор увеличился -> поджимаем нижний край окна вверх
                newVisBot = wArea->bottom;
            } else if (wArea->bottom > oldArea->bottom && std::abs(visBot - oldArea->bottom) <= 28) {
                // Зазор уменьшился -> опускаем нижний край окна обратно вниз!
                newVisBot = wArea->bottom;
            }

            int finalBot = newVisBot + padB;
            if (newTop != r.top || finalBot != r.bottom) {
                SetWindowPos(hwnd, nullptr, r.left, newTop, w, std::max(120, finalBot - newTop),
                             SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));

    if (mwWasVis && mw) {
        HWND hMw = reinterpret_cast<HWND>(mw->winId());
        if (hMw) {
            ShowWindow(hMw, SW_SHOWNA);
            SetWindowPos(hMw, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }
    }
}
static void processTaskbarFlyouts(
    int barH,
    int screenEdge,
    int startScreenLeft,
    int notifyScreenLeft,
    int notifyScreenRight,
    bool isDockMode,
    int startPopupMode,
    int trayPopupMode,
    bool glassEnabled,
    int glassStyle,
    const QColor &glassColor,
    qreal glassOpacity)
{
    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);
    if (scrW <= 200) return;
    int screenCenterX = scrW / 2;
    bool startAndTrayAtTop = (screenEdge == 1 || screenEdge == 2);

    if (HWND hOverflow = FindWindowW(L"NotifyIconOverflowWindow", nullptr)) {
        if (IsWindowVisible(hOverflow)) {
            applyFlyoutGlassEffect(hOverflow, false, glassEnabled, glassStyle, glassColor, glassOpacity);
            RECT r = {};
            GetWindowRect(hOverflow, &r);
            int w = r.right - r.left, h = r.bottom - r.top;
            if (w > 20 && h > 20 && (isDockMode || trayPopupMode != 2 || startAndTrayAtTop)) {
                int targetX = int(r.left);
                int targetY = startAndTrayAtTop ? (barH + 8) : (scrH - barH - h - 6);
                if (trayPopupMode == 1) targetX = screenCenterX - w / 2;
                else if (trayPopupMode == 0 && isDockMode) targetX = notifyScreenLeft - w / 2 + 16;
                else if (trayPopupMode == 2) targetX = scrW - w - 120;

                targetX = std::clamp(targetX, 8, scrW - w - 8);
                targetY = std::clamp(targetY, 8, scrH - h - 8);
                if (std::abs(int(r.left) - targetX) > 2 || std::abs(int(r.top) - targetY) > 2) {
                    SetWindowPos(hOverflow, HWND_TOPMOST, targetX, targetY, 0, 0,
                                 SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
                }
            }
        }
    }

    static QMap<DWORD, QString> s_pidExeCache;
    HWND hCore = nullptr;
    while ((hCore = FindWindowExW(nullptr, hCore, L"Windows.UI.Core.CoreWindow", nullptr)) != nullptr) {
        if (!IsWindowVisible(hCore)) continue;
        // КРИТИЧНО: Сразу пропускаем свёрнутые/скрытые UWP-окна (DWMWA_CLOAKED), чтобы не дёргать OpenProcess и DWM в покое!
        INT cloaked = 0;
        if (SUCCEEDED(DwmGetWindowAttribute(hCore, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked != 0)
            continue;

        RECT r = {};
        GetWindowRect(hCore, &r);
        int w = r.right - r.left, h = r.bottom - r.top;
        if (w <= 40 || h <= 40) continue;

        DWORD pid = 0;
        GetWindowThreadProcessId(hCore, &pid);
        QString exeName = s_pidExeCache.value(pid);
        if (exeName.isEmpty() && pid != 0) {
            if (HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) {
                wchar_t pathBuf[MAX_PATH] = {};
                DWORD sz = MAX_PATH;
                if (QueryFullProcessImageNameW(hProc, 0, pathBuf, &sz)) {
                    exeName = QString::fromWCharArray(pathBuf);
                    s_pidExeCache.insert(pid, exeName);
                }
                CloseHandle(hProc);
            }
        }

        bool isStartOrSearch = exeName.contains("StartMenuExperienceHost", Qt::CaseInsensitive) ||
                               exeName.contains("SearchApp", Qt::CaseInsensitive) ||
                               exeName.contains("SearchUI", Qt::CaseInsensitive) ||
                               exeName.contains("SearchHost", Qt::CaseInsensitive);
        bool isShellExp = exeName.contains("ShellExperienceHost", Qt::CaseInsensitive);

        if (isStartOrSearch || isShellExp) {
            applyFlyoutGlassEffect(hCore, true, glassEnabled, glassStyle, glassColor, glassOpacity);
        }

        if (isStartOrSearch) {
            int maxLeft = std::max(8, scrW - 360);
            int targetX = 0;
            if (startPopupMode == 1) {
                int estStartMenuW = (w < scrW - 200) ? w : 620;
                targetX = std::clamp(screenCenterX - estStartMenuW / 2, 8, maxLeft);
            } else if (startPopupMode == 0 && isDockMode) {
                targetX = std::clamp(startScreenLeft, 8, maxLeft);
            }
            int targetY = startAndTrayAtTop ? (barH + 2) : (scrH - barH - h);
            if (h >= scrH - 60) {
                targetY = startAndTrayAtTop ? std::min(barH + 2, -(h - 680)) : (scrH - barH - h);
            }
            if (std::abs(int(r.left) - targetX) > 2 || std::abs(int(r.top) - targetY) > 4) {
                SetWindowPos(hCore, nullptr, targetX, targetY, 0, 0,
                             SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
            }
        } else if (isShellExp) {
            INT cloaked = 0;
            if (SUCCEEDED(DwmGetWindowAttribute(hCore, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked != 0)
                continue;
            wchar_t cTitleBuf[128] = {};
            GetWindowTextW(hCore, cTitleBuf, 127);
            QString cTitle = QString::fromWCharArray(cTitleBuf);
            bool isJumpList = cTitle.contains("переход", Qt::CaseInsensitive) ||
                              cTitle.contains("Jump", Qt::CaseInsensitive) ||
                              (w <= 320 && r.left > 60 && r.right < scrW - 260 && std::abs(int(r.right) - notifyScreenRight) > 40);
            if (isJumpList) {
                if ( screenEdge == 1 || screenEdge == 3 ) {
                    int jlY = barH + 2;
                    if (std::abs(int(r.top) - jlY) > 4) {
                        SetWindowPos(hCore, nullptr, r.left, jlY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
                    }
                }
                continue;
            }
            if (r.top > 15 && w < scrW - 100) {
                int effW = std::min(w, 420);
                int maxLeft = std::max(8, scrW - effW - 8);
                int targetX = scrW - effW;
                int targetY = startAndTrayAtTop ? (barH + 4) : int(r.top);
                if (trayPopupMode == 1) targetX = std::clamp(screenCenterX - effW / 2, 8, maxLeft);
                else if (trayPopupMode == 0 && isDockMode) targetX = std::clamp(notifyScreenRight - effW, 8, maxLeft);
                if (std::abs(int(r.left) - targetX) > 2 || std::abs(int(r.top) - targetY) > 2) {
                    SetWindowPos(hCore, nullptr, targetX, targetY, 0, 0,
                                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
                }
            }
        }
    }
}
#endif

static int s_savedBarH = 30;

TaskbarManager::TaskbarManager(QObject *parent) : QObject(parent) {
#ifdef Q_OS_WIN
    if (HWND hTray = FindWindowW(L"Shell_TrayWnd", nullptr)) {
        RECT r = {}; GetWindowRect(hTray, &r);
        int h = r.bottom - r.top;
        if (h >= 28 && h <= 56) s_savedBarH = h;
    }
#endif
    restoreNativeTaskbar();
    s_isShuttingDown = false;
    load();
    s_workspacesEnabledGlobal = m_showWorkspaces;
#ifdef Q_OS_WIN
    if (!s_kbWorkspaceHook) {
        s_kbWorkspaceHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKbWorkspaceProc, GetModuleHandleW(nullptr), 0);
    }
#endif
    m_timer.setInterval(55);
    connect(&m_timer, &QTimer::timeout, this, &TaskbarManager::onTick);
    if (m_enabled) {
        applyVisibilityFlags();
        m_timer.start();
    }
}

TaskbarManager::~TaskbarManager() {
    s_isShuttingDown = true;
    m_timer.stop();
#ifdef Q_OS_WIN
    if (s_kbWorkspaceHook) {
        UnhookWindowsHookEx(s_kbWorkspaceHook);
        s_kbWorkspaceHook = nullptr;
    }
    restoreAllWorkspacesOnExit();
#endif
    restoreNativeTaskbar();
}

QString TaskbarManager::configPath() const {
    return QDir::homePath() + "/.config/hypr/scripts/taskbar_win.conf";
}

void TaskbarManager::load() {
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
        else if (k == "screen_edge") m_screenEdge = std::clamp(v.toInt(), 0, 3);
        else if (k == "style") m_style = std::clamp(v.toInt(), 0, 4);
        else if (k == "tint_color" && QColor::isValidColorName(v)) m_tintColor = QColor(v);
        else if (k == "tint_opacity") m_tintOpacity = std::clamp(v.toDouble(), 0.0, 1.0);
        else if (k == "show_border") m_showBorder = pb(v);
        else if (k == "max_trigger_enabled") m_maxTriggerEnabled = pb(v);
        else if (k == "max_style") m_maxStyle = std::clamp(v.toInt(), 0, 4);
        else if (k == "max_tint_color" && QColor::isValidColorName(v)) m_maxTintColor = QColor(v);
        else if (k == "max_tint_opacity") m_maxTintOpacity = std::clamp(v.toDouble(), 0.0, 1.0);
        else if (k == "center_icons") m_centerIcons = pb(v);
        else if (k == "smooth_animation") m_smoothAnimation = pb(v);
        else if (k == "anim_speed") m_animSpeed = std::clamp(v.toInt(), 50, 800);
        else if (k == "center_offset") m_centerOffset = std::clamp(v.toInt(), -600, 600);
        else if (k == "hide_start") m_hideStart = pb(v);
        else if (k == "hide_search") m_hideSearch = pb(v);
        else if (k == "hide_tray") m_hideTray = pb(v);
        else if (k == "dock_mode") m_dockMode = pb(v);
        else if (k == "split_three_pills") m_splitThreePills = pb(v);
        else if (k == "show_workspaces") m_showWorkspaces = pb(v);
        else if (k == "auto_hide_dock") m_autoHideDock = pb(v);
        else if (k == "dynamic_dock_width") m_dynamicDockWidth = pb(v);
        else if (k == "dock_radius") m_dockRadius = std::clamp(v.toInt(), 0, 28);
        else if (k == "dock_margin_x") m_dockMarginX = std::clamp(v.toInt(), 0, 120);
        else if (k == "dock_margin_y") m_dockMarginY = std::clamp(v.toInt(), 0, 16);
        else if (k == "work_area_gap") m_workAreaGap = std::clamp(v.toInt(), -16, 28);
        else if (k == "start_popup_pos") m_startPopupPos = std::clamp(v.toInt(), 0, 2);
        else if (k == "tray_popup_pos") m_trayPopupPos = std::clamp(v.toInt(), 0, 2);
        else if (k == "flyout_glass_enabled") m_flyoutGlassEnabled = pb(v);
        else if (k == "flyout_style") m_flyoutStyle = std::clamp(v.toInt(), 0, 3);
        else if (k == "flyout_tint_color" && QColor::isValidColorName(v)) m_flyoutTintColor = QColor(v);
        else if (k == "flyout_opacity") m_flyoutOpacity = std::clamp(v.toDouble(), 0.0, 1.0);
    }
    s_workspacesEnabledGlobal = m_showWorkspaces;
}

void TaskbarManager::save() {
    QDir().mkpath(QDir::homePath() + "/.config/hypr/scripts");
    QFile f(configPath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream out(&f);
        out << "enabled=" << (m_enabled ? 1 : 0) << "\n"
            << "screen_edge=" << m_screenEdge << "\n"
            << "style=" << m_style << "\n"
            << "tint_color=" << m_tintColor.name(QColor::HexRgb) << "\n"
            << "tint_opacity=" << QString::number(m_tintOpacity, 'f', 2) << "\n"
            << "show_border=" << (m_showBorder ? 1 : 0) << "\n"
            << "max_trigger_enabled=" << (m_maxTriggerEnabled ? 1 : 0) << "\n"
            << "max_style=" << m_maxStyle << "\n"
            << "max_tint_color=" << m_maxTintColor.name(QColor::HexRgb) << "\n"
            << "max_tint_opacity=" << QString::number(m_maxTintOpacity, 'f', 2) << "\n"
            << "center_icons=" << (m_centerIcons ? 1 : 0) << "\n"
            << "smooth_animation=" << (m_smoothAnimation ? 1 : 0) << "\n"
            << "anim_speed=" << m_animSpeed << "\n"
            << "center_offset=" << m_centerOffset << "\n"
            << "hide_start=" << (m_hideStart ? 1 : 0) << "\n"
            << "hide_search=" << (m_hideSearch ? 1 : 0) << "\n"
            << "hide_tray=" << (m_hideTray ? 1 : 0) << "\n"
            << "dock_mode=" << (m_dockMode ? 1 : 0) << "\n"
            << "split_three_pills=" << (m_splitThreePills ? 1 : 0) << "\n"
            << "show_workspaces=" << (m_showWorkspaces ? 1 : 0) << "\n"
            << "auto_hide_dock=" << (m_autoHideDock ? 1 : 0) << "\n"
            << "dynamic_dock_width=" << (m_dynamicDockWidth ? 1 : 0) << "\n"
            << "dock_radius=" << m_dockRadius << "\n"
            << "dock_margin_x=" << m_dockMarginX << "\n"
            << "dock_margin_y=" << m_dockMarginY << "\n"
            << "work_area_gap=" << m_workAreaGap << "\n"
            << "start_popup_pos=" << m_startPopupPos << "\n"
            << "tray_popup_pos=" << m_trayPopupPos << "\n"
            << "flyout_glass_enabled=" << (m_flyoutGlassEnabled ? 1 : 0) << "\n"
            << "flyout_style=" << m_flyoutStyle << "\n"
            << "flyout_tint_color=" << m_flyoutTintColor.name(QColor::HexRgb) << "\n"
            << "flyout_opacity=" << QString::number(m_flyoutOpacity, 'f', 2) << "\n";
    }
}

void TaskbarManager::applyVisibilityFlags() {
#ifdef Q_OS_WIN
    HWND hTray = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!hTray) return;
    if (HWND hStart = FindWindowExW(hTray, nullptr, L"Start", nullptr)) {
        ShowWindow(hStart, (m_enabled && m_hideStart) ? SW_HIDE : SW_SHOWNA);
    }
    if (HWND hNotify = FindWindowExW(hTray, nullptr, L"TrayNotifyWnd", nullptr)) {
        ShowWindow(hNotify, (m_enabled && m_hideTray) ? SW_HIDE : SW_SHOWNA);
    }
    HWND hChild = nullptr;
    while ((hChild = FindWindowExW(hTray, hChild, nullptr, nullptr)) != nullptr) {
        wchar_t cls[128] = {};
        GetClassNameW(hChild, cls, 127);
        if (wcscmp(cls, L"TrayDummySearchControl") == 0) {
            ShowWindow(hChild, (m_enabled && (m_hideSearch || m_dockMode)) ? SW_HIDE : SW_SHOWNA);
        }
    }
#endif
}

// 100% ПОЛНЫЙ СИНХРОННЫЙ ВОЗВРАТ СТАНДАРТНОЙ ПАНЕЛИ ЗАДАЧ WINDOWS 10 И РАБОЧИХ СТОЛОВ
void TaskbarManager::restoreNativeTaskbar() {
    m_timer.stop();
#ifdef Q_OS_WIN
    restoreAllWorkspacesOnExit();
    destroyWsOverlay();
    destroyPillBackdrop();

    HWND hTray = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!hTray) return;

    SetWindowRgn(hTray, nullptr, TRUE);
    setHwndAccent(hTray, 3, QColor(16, 16, 18), 0.90, false);
    setHwndAccent(hTray, 4, Qt::black, 0.0, false);

    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);
    int barH = s_savedBarH;
    int nativeStartW = (barH <= 34) ? 36 : 48;

    s_lastAppliedWorkArea = { -1, -1, -1, -1 };
    applyDesktopWorkArea(0, barH);

    ShowWindow(hTray, SW_SHOWNA);
    SetWindowPos(hTray, HWND_TOPMOST, 0, scrH - barH, scrW, barH,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);

    if (HWND hStart = FindWindowExW(hTray, nullptr, L"Start", nullptr)) {
        ShowWindow(hStart, SW_SHOWNA);
        SetWindowPos(hStart, nullptr, 0, 0, nativeStartW, barH, SWP_NOZORDER | SWP_NOACTIVATE);
    }

    int notifyW = 224;
    if (HWND hNotify = FindWindowExW(hTray, nullptr, L"TrayNotifyWnd", nullptr)) {
        ShowWindow(hNotify, SW_SHOWNA);
        notifyW = measureTrayNotifyWidth(hNotify, false);
        SetWindowPos(hNotify, nullptr, std::max(0, scrW - notifyW), 0, notifyW, barH, SWP_NOZORDER | SWP_NOACTIVATE);
    }

    int fullBarW = std::max(400, scrW - nativeStartW - notifyW);
    HWND hReBar = FindWindowExW(hTray, nullptr, L"ReBarWindow32", nullptr);
    HWND hTaskSw = hReBar ? FindWindowExW(hReBar, nullptr, L"MSTaskSwWClass", nullptr) : nullptr;
    HWND hTaskList = hTaskSw ? FindWindowExW(hTaskSw, nullptr, L"MSTaskListWClass", nullptr) : nullptr;

    if (hReBar) {
        ShowWindow(hReBar, SW_SHOWNA);
        SetWindowPos(hReBar, nullptr, nativeStartW, 0, fullBarW, barH, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (hTaskSw) {
        ShowWindow(hTaskSw, SW_SHOWNA);
        SetWindowPos(hTaskSw, nullptr, 0, 0, fullBarW, barH, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (hTaskList) {
        ShowWindow(hTaskList, SW_SHOWNA);
        SetWindowPos(hTaskList, nullptr, 0, 0, fullBarW, barH, SWP_NOZORDER | SWP_NOACTIVATE);
    }

    SendMessageTimeoutW(hTray, WM_THEMECHANGED, 0, 0, SMTO_ABORTIFHUNG, 150, nullptr);
    SendMessageTimeoutW(hTray, WM_DWMCOMPOSITIONCHANGED, 0, 0, SMTO_ABORTIFHUNG, 150, nullptr);
    SendMessageTimeoutW(hTray, WM_SIZE, SIZE_RESTORED, MAKELPARAM(scrW, barH), SMTO_ABORTIFHUNG, 150, nullptr);
    if (hReBar) SendMessageTimeoutW(hReBar, WM_SIZE, SIZE_RESTORED, MAKELPARAM(fullBarW, barH), SMTO_ABORTIFHUNG, 150, nullptr);
    RedrawWindow(hTray, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);

    processTaskbarFlyouts(barH, 0, 0, scrW - notifyW, scrW, false, 2, 2, false, 4, Qt::black, 0.0);
#endif
}

void TaskbarManager::onTick() {
#ifdef Q_OS_WIN
    if (!m_enabled || s_isShuttingDown || g_introCurtainOpaque.load()) return;
    m_tickCounter++;

    if (s_winWalPinnedWorkspace != -1 && s_winWalPinnedWorkspace != s_currentWorkspace) {
        if (QWindow *mainWin = findMainWinWalWindow()) {
            if (mainWin->isVisible() && !mainWin->property("hiddenByWorkspace").toBool()) {
                s_winWalPinnedWorkspace = -1;
            }
        }
    }

    HWND hTray = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!hTray) return;

    static bool s_cachedMax = false;
    if (m_tickCounter % 15 == 1) {
        s_cachedMax = m_maxTriggerEnabled ? hasAnyMaximizedWindow() : false;
    }

    int effStyle = s_cachedMax ? m_maxStyle : m_style;
    QColor effColor = s_cachedMax ? m_maxTintColor : m_tintColor;
    qreal effOp = s_cachedMax ? m_maxTintOpacity : m_tintOpacity;

    HWND hReBar = FindWindowExW(hTray, nullptr, L"ReBarWindow32", nullptr);
    HWND hTaskSw = hReBar ? FindWindowExW(hReBar, nullptr, L"MSTaskSwWClass", nullptr) : nullptr;
    HWND hTaskList = hTaskSw ? FindWindowExW(hTaskSw, nullptr, L"MSTaskListWClass", nullptr) : nullptr;
    HWND hStartBtn = FindWindowExW(hTray, nullptr, L"Start", nullptr);
    HWND hNotify = FindWindowExW(hTray, nullptr, L"TrayNotifyWnd", nullptr);

    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);
    RECT trayRect = {};
    GetWindowRect(hTray, &trayRect);
    int curH = trayRect.bottom - trayRect.top;
    if (curH >= 28 && curH <= 56) s_savedBarH = curH;
    int barH = s_savedBarH;

    bool dockAtTop = (m_screenEdge == 1 || m_screenEdge == 3);
    bool topStartAndTray = (m_screenEdge == 1 || (m_dockMode && m_splitThreePills && m_screenEdge == 2));
    int topReserve = (dockAtTop || topStartAndTray) ? std::max(0, barH + m_workAreaGap) : 0;
    int botReserve = m_autoHideDock ? 0 : ((!dockAtTop || (m_dockMode && m_splitThreePills && m_screenEdge == 3)) ? std::max(0, barH + m_workAreaGap) : 0);
    if (m_tickCounter % 10 == 1) {
        applyDesktopWorkArea(topReserve, botReserve);
    }

    static bool s_dockHoverVisible = true;
    if (m_autoHideDock) {
        POINT curPt = {};
        GetCursorPos(&curPt);
        if (dockAtTop) {
            if (curPt.y <= 8) s_dockHoverVisible = true;
            else if (curPt.y > barH + 18) s_dockHoverVisible = false;
        } else {
            if (curPt.y >= scrH - 8) s_dockHoverVisible = true;
            else if (curPt.y < scrH - barH - 18) s_dockHoverVisible = false;
        }
    } else {
        s_dockHoverVisible = true;
    }

        int floatY = m_dockMode ? std::clamp(m_dockMarginY, 0, 14) : 0;
    int targetTrayY = dockAtTop ? floatY : (scrH - barH - floatY);
    if (std::abs(int(trayRect.top) - targetTrayY) > 1 || (trayRect.right - trayRect.left) != scrW) {
        SetWindowPos(hTray, topZOrderTarget(), 0, targetTrayY, scrW, barH, SWP_NOACTIVATE | SWP_NOSENDCHANGING);
        GetWindowRect(hTray, &trayRect);
    }

    static int s_firstIconOffset = 0;
    if (hTaskList && (m_tickCounter % 10 == 1 || m_lastIconsSpan <= 0)) {
        int measured = measureTaskListIcons(hTaskList, &s_firstIconOffset);
        if (measured > 0) m_lastIconsSpan = measured;
    }

    int notifyW = hNotify ? measureTrayNotifyWidth(hNotify, m_dockMode) : 210;
    int fullIconsContainerW = std::max(1600, scrW);
    int nativeStartW = (barH <= 34) ? 36 : 48;

    if (m_dockMode && hTaskSw && hTaskList && scrW > 200) {
        int extraMargin = m_dynamicDockWidth ? 0 : std::max(0, m_dockMarginX);
        int rawIconsW = std::max(40, m_lastIconsSpan - 4); // чистая ширина иконок без лишнего хвоста справа
        int padSide = 6 + extraMargin / 2;

        if (m_splitThreePills) {
            bool topBarMode = (m_screenEdge == 2 || m_screenEdge == 3);
            int wsW = m_showWorkspaces ? (5 * 24 + 12) : 0;
            int leftPillX = 12;
            int startW = (!m_hideStart && hStartBtn) ? nativeStartW : 0;
            int leftPillW = startW + wsW + 8;

            int iconsW = std::clamp(rawIconsW, 40, std::max(240, scrW - leftPillW - notifyW - 60));
            int pillW = iconsW + padSide * 2;
            int centerPillLeft = std::max(leftPillX + leftPillW + 12, (scrW / 2) - (pillW / 2) + m_centerOffset);
            int centerPillRight = centerPillLeft + pillW;

            // Обрезаем hReBar ровно по iconsW, чтобы выделение крайней правой кнопки не вылезало за скругление капсулы!
            moveChildIfChanged(hReBar, HWND_BOTTOM, centerPillLeft + padSide, 0, iconsW, barH);
            moveChildIfChanged(hTaskSw, nullptr, 0, 0, fullIconsContainerW, barH);
            moveChildIfChanged(hTaskList, nullptr, 0, 0, fullIconsContainerW, barH);

            int rightPillRight = scrW - 12;
            int rightPillLeft = rightPillRight - notifyW - 12;

            ShowWindow(hReBar, s_dockHoverVisible ? SW_SHOWNA : SW_HIDE);
            QVector<QRect> bottomPills;
            if (s_dockHoverVisible) {
                bottomPills.append(QRect(centerPillLeft, 0, centerPillRight - centerPillLeft, barH));
            }

            if (!topBarMode) {
                if (hStartBtn && !m_hideStart) {
                    ShowWindow(hStartBtn, SW_SHOWNA);
                    moveChildIfChanged(hStartBtn, HWND_TOP, leftPillX + 4, 0, startW, barH);
                }
                if (hNotify && !m_hideTray) {
                    ShowWindow(hNotify, SW_SHOWNA);
                    moveChildIfChanged(hNotify, HWND_TOP, rightPillLeft + 6, 0, notifyW, barH);
                }
                if (!m_hideStart || m_showWorkspaces) {
                    bottomPills.append(QRect(leftPillX, 0, leftPillW, barH));
                }
                if (!m_hideTray && hNotify) {
                    bottomPills.append(QRect(rightPillLeft, 0, rightPillRight - rightPillLeft, barH));
                }
                if (m_showWorkspaces) {
                    updateWsOrTopBarOverlay(false, leftPillX + 4 + startW, int(trayRect.top), barH, m_dockRadius, effStyle, effColor, effOp, m_showBorder);
                    SetWindowPos(hTray, s_hWsBar, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
                } else {
                    destroyWsOverlay();
                    SetWindowPos(hTray, topZOrderTarget(), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
                }
            } else {
                if (hStartBtn) ShowWindow(hStartBtn, SW_HIDE);
                if (hNotify) ShowWindow(hNotify, SW_HIDE);
                int oppBarY = (m_screenEdge == 2) ? (2 + floatY) : (scrH - barH - floatY);
                updateWsOrTopBarOverlay(true, 0, oppBarY, barH, m_dockRadius, effStyle, effColor, effOp, m_showBorder);
                SetWindowPos(hTray, topZOrderTarget(), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
                rightPillLeft = s_topRightPillLeft;
                rightPillRight = scrW - 12;
            }

            updatePillBackdrop(hTray, int(trayRect.top), barH, bottomPills, m_dockRadius, effStyle, effColor, effOp, m_showBorder);

            processTaskbarFlyouts(barH + floatY, m_screenEdge, leftPillX, rightPillLeft, rightPillRight,
                                  true, m_startPopupPos, m_trayPopupPos,
                                  m_flyoutGlassEnabled, m_flyoutStyle, m_flyoutTintColor, m_flyoutOpacity);
            return;
        }

        // РЕЖИМ ЕДИНОЙ КАПСУЛЫ (splitThreePills == false)
        int startW = (!m_hideStart && hStartBtn) ? nativeStartW : 0;
        int wsW = m_showWorkspaces ? (5 * 24 + 12) : 0;
        int effNotifyW = (!m_hideTray && hNotify) ? notifyW : 0;
        int maxAllowedIconsW = std::max(140, scrW - startW - wsW - effNotifyW - 44);
        int iconsW = std::clamp(rawIconsW, 40, maxAllowedIconsW);
        int padX = 8 + extraMargin / 2;
        int gapAfterStart = (startW > 0 || wsW > 0) ? 6 : 0;
        int gapBeforeTray = (effNotifyW > 0) ? 8 : 0;
        int totalPillW = padX + startW + wsW + gapAfterStart + iconsW + gapBeforeTray + effNotifyW + padX;

        int centerScreenX = scrW / 2 + m_centerOffset;
        int pillLeft = std::max(6, centerScreenX - totalPillW / 2);
        int pillRight = std::min(scrW - 6, pillLeft + totalPillW);

        int cursorX = pillLeft + padX;
        int startScreenLeft = cursorX;
        if (hStartBtn && !m_hideStart) {
            ShowWindow(hStartBtn, SW_SHOWNA);
            moveChildIfChanged(hStartBtn, HWND_TOP, cursorX, 0, startW, barH);
            cursorX += startW;
        }
        if (m_showWorkspaces) {
            updateWsOrTopBarOverlay(false, cursorX + 2, int(trayRect.top), barH, m_dockRadius, effStyle, effColor, effOp, m_showBorder);
            cursorX += wsW;
        } else {
            destroyWsOverlay();
        }
        cursorX += gapAfterStart;

        moveChildIfChanged(hReBar, HWND_BOTTOM, cursorX, 0, iconsW, barH);
        moveChildIfChanged(hTaskSw, nullptr, 0, 0, fullIconsContainerW, barH);
        moveChildIfChanged(hTaskList, nullptr, 0, 0, fullIconsContainerW, barH);
        cursorX += iconsW + gapBeforeTray;

        int notifyScreenLeft = cursorX;
        int notifyScreenRight = notifyScreenLeft + effNotifyW;
        if (hNotify && !m_hideTray) {
            ShowWindow(hNotify, SW_SHOWNA);
            moveChildIfChanged(hNotify, HWND_TOP, cursorX, 0, effNotifyW, barH);
        }

        ShowWindow(hReBar, s_dockHoverVisible ? SW_SHOWNA : SW_HIDE);
        ShowWindow(hTray, s_dockHoverVisible ? SW_SHOWNA : SW_HIDE);
        if (m_showWorkspaces && s_hWsBar) {
            SetWindowPos(hTray, s_hWsBar, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
        } else {
            SetWindowPos(hTray, topZOrderTarget(), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
        }

        QVector<QRect> singlePill;
        if (s_dockHoverVisible) singlePill.append(QRect(pillLeft, 0, pillRight - pillLeft, barH));
        updatePillBackdrop(hTray, int(trayRect.top), barH, singlePill, m_dockRadius, effStyle, effColor, effOp, m_showBorder);

        processTaskbarFlyouts(barH + floatY, 0, startScreenLeft, notifyScreenLeft, notifyScreenRight,
                              true, m_startPopupPos, m_trayPopupPos,
                              m_flyoutGlassEnabled, m_flyoutStyle, m_flyoutTintColor, m_flyoutOpacity);
        return;
    }
// ПОЛНОШИРИННЫЙ РЕЖИМ (dockMode == false)
    destroyWsOverlay();
    destroyPillBackdrop();
    SetWindowRgn(hTray, nullptr, TRUE);
    setHwndAccent(hTray, effStyle, effColor, effOp, m_showBorder);
    ShowWindow(hTray, SW_SHOWNA);
    if (hReBar) ShowWindow(hReBar, SW_SHOWNA);
    SetWindowPos(hTray, topZOrderTarget(), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOSENDCHANGING);

    if (hStartBtn && !m_hideStart) {
        ShowWindow(hStartBtn, SW_SHOWNA);
        moveChildIfChanged(hStartBtn, nullptr, 0, 0, nativeStartW, barH);
    }
    if (hNotify && !m_hideTray) {
        ShowWindow(hNotify, SW_SHOWNA);
        moveChildIfChanged(hNotify, nullptr, std::max(0, scrW - notifyW), 0, notifyW, barH);
    }
    int fullBarW = std::max(400, scrW - nativeStartW - notifyW);
    if (hReBar) moveChildIfChanged(hReBar, nullptr, nativeStartW, 0, fullBarW, barH);
    if (hTaskSw) moveChildIfChanged(hTaskSw, nullptr, 0, 0, fullBarW, barH);

    if (hTaskList && hTaskSw) {
        if (m_centerIcons) {
            RECT swRect = {};
            GetWindowRect(hTaskSw, &swRect);
            int swLeftRel = swRect.left - trayRect.left;
            double desiredFirstScreenX = (scrW / 2.0) - (m_lastIconsSpan / 2.0) + m_centerOffset;
            double targetRelX = desiredFirstScreenX - swLeftRel;

            if (m_currentTaskListX < -9000.0 || !m_smoothAnimation) {
                m_currentTaskListX = targetRelX;
            } else {
                double factor = std::clamp(35.0 / std::max(45.0, double(m_animSpeed)) * 2.5, 0.08, 0.55);
                m_currentTaskListX += (targetRelX - m_currentTaskListX) * factor;
            }
            moveChildIfChanged(hTaskList, nullptr, int(std::round(m_currentTaskListX)), 0, fullIconsContainerW, barH);
        } else if (m_currentTaskListX != 0.0) {
            m_currentTaskListX = 0.0;
            moveChildIfChanged(hTaskList, nullptr, 0, 0, fullIconsContainerW, barH);
        }
    }

    processTaskbarFlyouts(barH, 0, int(trayRect.left), int(trayRect.right) - notifyW, int(trayRect.right),
                          false, m_startPopupPos, m_trayPopupPos,
                          m_flyoutGlassEnabled, m_flyoutStyle, m_flyoutTintColor, m_flyoutOpacity);
#endif
}

bool TaskbarManager::autostart() const {
#ifdef Q_OS_WIN
    HKEY hKey = nullptr;
    bool exists = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t buf[MAX_PATH * 2] = {};
        DWORD sz = sizeof(buf);
        if (RegQueryValueExW(hKey, L"WinWal", nullptr, nullptr, reinterpret_cast<LPBYTE>(buf), &sz) == ERROR_SUCCESS) {
            exists = (wcslen(buf) > 0);
        }
        RegCloseKey(hKey);
    }
    return exists;
#else
    return false;
#endif
}

void TaskbarManager::setAutostart(bool v) {
#ifdef Q_OS_WIN
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (v) {
            QString exePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
            QString cmd = QString("\"%1\" --daemon").arg(exePath);
            RegSetValueExW(hKey, L"WinWal", 0, REG_SZ, reinterpret_cast<const BYTE *>(cmd.utf16()), DWORD((cmd.length() + 1) * sizeof(wchar_t)));
        } else {
            RegDeleteValueW(hKey, L"WinWal");
        }
        RegCloseKey(hKey);
    }
    emit stateChanged();
#else
    Q_UNUSED(v);
#endif
}

void TaskbarManager::setEnabled(bool v) {
    if (m_enabled == v) return;
    m_enabled = v;
    if (v) { s_isShuttingDown = false; applyVisibilityFlags(); m_timer.start(); }
    else { m_timer.stop(); restoreNativeTaskbar(); }
    emit stateChanged(); save();
}
void TaskbarManager::setScreenEdge(int v) {
    v = std::clamp(v, 0, 3);
    if (m_screenEdge != v) {
        m_screenEdge = v;
        m_lastIconsSpan = 0;
        s_lastBackdropKey.clear();
        s_lastWsRenderKey.clear();
        applyVisibilityFlags();
        onTick();
        emit stateChanged();
        save();
    }
}
void TaskbarManager::setStyle(int v) { v = std::clamp(v, 0, 4); if (m_style != v) { m_style = v; s_lastBackdropKey.clear(); s_lastWsRenderKey.clear(); onTick(); emit stateChanged(); save(); } }
void TaskbarManager::setTintColor(const QColor &c) { if (c.isValid() && m_tintColor != c) { m_tintColor = c; s_lastBackdropKey.clear(); s_lastWsRenderKey.clear(); onTick(); emit stateChanged(); save(); } }
void TaskbarManager::setTintOpacity(qreal v) { v = std::clamp(v, 0.0, 1.0); if (!qFuzzyCompare(m_tintOpacity, v)) { m_tintOpacity = v; s_lastBackdropKey.clear(); s_lastWsRenderKey.clear(); onTick(); emit stateChanged(); save(); } }
void TaskbarManager::setShowBorder(bool v) { if (m_showBorder != v) { m_showBorder = v; s_lastBackdropKey.clear(); s_lastWsRenderKey.clear(); onTick(); emit stateChanged(); save(); } }
void TaskbarManager::setMaxTriggerEnabled(bool v) { if (m_maxTriggerEnabled != v) { m_maxTriggerEnabled = v; s_lastBackdropKey.clear(); onTick(); emit stateChanged(); save(); } }
void TaskbarManager::setMaxStyle(int v) { v = std::clamp(v, 0, 4); if (m_maxStyle != v) { m_maxStyle = v; s_lastBackdropKey.clear(); onTick(); emit stateChanged(); save(); } }
void TaskbarManager::setMaxTintColor(const QColor &c) { if (c.isValid() && m_maxTintColor != c) { m_maxTintColor = c; s_lastBackdropKey.clear(); onTick(); emit stateChanged(); save(); } }
void TaskbarManager::setMaxTintOpacity(qreal v) { v = std::clamp(v, 0.0, 1.0); if (!qFuzzyCompare(m_maxTintOpacity, v)) { m_maxTintOpacity = v; s_lastBackdropKey.clear(); onTick(); emit stateChanged(); save(); } }
void TaskbarManager::setCenterIcons(bool v) { if (m_centerIcons != v) { m_centerIcons = v; if (!v) m_currentTaskListX = -9999.0; onTick(); emit stateChanged(); save(); } }
void TaskbarManager::setSmoothAnimation(bool v) { if (m_smoothAnimation != v) { m_smoothAnimation = v; emit stateChanged(); save(); } }
void TaskbarManager::setAnimSpeed(int v) { v = std::clamp(v, 50, 800); if (m_animSpeed != v) { m_animSpeed = v; emit stateChanged(); save(); } }
void TaskbarManager::setCenterOffset(int v) { v = std::clamp(v, -600, 600); if (m_centerOffset != v) { m_centerOffset = v; s_lastBackdropKey.clear(); onTick(); emit stateChanged(); save(); } }
void TaskbarManager::setHideStart(bool v) { if (m_hideStart != v) { m_hideStart = v; s_lastBackdropKey.clear(); applyVisibilityFlags(); onTick(); emit stateChanged(); save(); } }
void TaskbarManager::setHideSearch(bool v) { if (m_hideSearch != v) { m_hideSearch = v; applyVisibilityFlags(); onTick(); emit stateChanged(); save(); } }
void TaskbarManager::setHideTray(bool v) { if (m_hideTray != v) { m_hideTray = v; s_lastBackdropKey.clear(); applyVisibilityFlags(); onTick(); emit stateChanged(); save(); } }
void TaskbarManager::setDockMode(bool v) {
    if (m_dockMode != v) {
        m_dockMode = v;
        m_lastIconsSpan = 0;
        s_lastBackdropKey.clear();
        if (!v) restoreNativeTaskbar();
        applyVisibilityFlags();
        onTick();
        emit stateChanged();
        save();
    }
}
void TaskbarManager::setSplitThreePills(bool v) {
    if (m_splitThreePills != v) {
        m_splitThreePills = v;
        m_lastIconsSpan = 0;
        s_lastBackdropKey.clear();
        s_lastWsRenderKey.clear();
        onTick();
        emit stateChanged();
        save();
    }
}
void TaskbarManager::setAutoHideDock(bool v) {
    if (m_autoHideDock != v) {
        m_autoHideDock = v;
        s_lastAppliedWorkArea = { -1, -1, -1, -1 };
        s_lastBackdropKey.clear();
        onTick();
        emit stateChanged();
        save();
    }
}
void TaskbarManager::setShowWorkspaces(bool v) {
    if (m_showWorkspaces != v) {
        m_showWorkspaces = v;
        s_workspacesEnabledGlobal = v;
        if (!v) {
            restoreAllWorkspacesOnExit();
            destroyWsOverlay();
        }
        s_lastBackdropKey.clear();
        onTick();
        emit stateChanged();
        save();
    }
}
void TaskbarManager::setDynamicDockWidth(bool v) { if (m_dynamicDockWidth != v) { m_dynamicDockWidth = v; s_lastBackdropKey.clear(); onTick(); emit stateChanged(); save(); } }
void TaskbarManager::setDockRadius(int v) { v = std::clamp(v, 0, 28); if (m_dockRadius != v) { m_dockRadius = v; s_lastBackdropKey.clear(); s_lastWsRenderKey.clear(); onTick(); emit stateChanged(); save(); } }
void TaskbarManager::setDockMarginX(int v) { v = std::clamp(v, 0, 120); if (m_dockMarginX != v) { m_dockMarginX = v; s_lastBackdropKey.clear(); onTick(); emit stateChanged(); save(); } }
void TaskbarManager::setWorkAreaGap(int v) {
    v = std::clamp(v, -16, 28);
    if (m_workAreaGap != v) {
        m_workAreaGap = v;
        emit stateChanged();
        save();
    }
}
void TaskbarManager::setDockMarginY(int v) { v = std::clamp(v, 0, 16); if (m_dockMarginY != v) { m_dockMarginY = v; s_lastBackdropKey.clear(); s_lastWsRenderKey.clear(); onTick(); emit stateChanged(); save(); } }
void TaskbarManager::setStartPopupPos(int v) { v = std::clamp(v, 0, 2); if (m_startPopupPos != v) { m_startPopupPos = v; emit stateChanged(); save(); } }
void TaskbarManager::setTrayPopupPos(int v) { v = std::clamp(v, 0, 2); if (m_trayPopupPos != v) { m_trayPopupPos = v; emit stateChanged(); save(); } }
void TaskbarManager::setFlyoutGlassEnabled(bool v) { if (m_flyoutGlassEnabled != v) { m_flyoutGlassEnabled = v; emit stateChanged(); save(); } }
void TaskbarManager::setFlyoutStyle(int v) { v = std::clamp(v, 0, 3); if (m_flyoutStyle != v) { m_flyoutStyle = v; emit stateChanged(); save(); } }
void TaskbarManager::setFlyoutTintColor(const QColor &c) { if (c.isValid() && m_flyoutTintColor != c) { m_flyoutTintColor = c; emit stateChanged(); save(); } }
void TaskbarManager::setFlyoutOpacity(qreal v) { v = std::clamp(v, 0.0, 1.0); if (!qFuzzyCompare(m_flyoutOpacity, v)) { m_flyoutOpacity = v; emit stateChanged(); save(); } }

// 5 ГОТОВЫХ ПРЕСЕТОВ ПАНЕЛИ ЗАДАЧ
void TaskbarManager::applyPreset(int presetIdx) {
    m_enabled = true;
    m_hideStart = false;
    m_hideSearch = false;
    m_hideTray = false;
    m_autoHideDock = false;
    m_dynamicDockWidth = true;
    m_centerOffset = 0;

    if (presetIdx == 0) {
        // 0: WinWal (3 плитки, радиус 0px, Размытие Aero) — стиль по умолчанию
        m_screenEdge = 0;
        m_dockMode = true;
        m_splitThreePills = true;
        m_showWorkspaces = true;
        m_style = 1; // Размытие
        m_tintColor = QColor("#0a0a0c");
        m_tintOpacity = 0.0;
        m_showBorder = false;
        m_dockRadius = 0;
        m_dockMarginX = 0;
        m_dockMarginY = 0;
        m_workAreaGap = -4;
        m_maxTriggerEnabled = true;
        m_maxStyle = 3;
        m_maxTintOpacity = 0.96;
    } else if (presetIdx == 1) {
        // 1: Hyprland (3 скруглённые капсулы + Акрил + Рамка)
        m_screenEdge = 0;
        m_dockMode = true;
        m_splitThreePills = true;
        m_showWorkspaces = true;
        m_style = 2; // Акрил
        m_tintColor = QColor("#0a0a0c");
        m_tintOpacity = 0.45;
        m_showBorder = true;
        m_dockRadius = 12;
        m_dockMarginX = 8;
        m_dockMarginY = 2;
        m_workAreaGap = 0;
        m_maxTriggerEnabled = true;
        m_maxStyle = 2;
        m_maxTintOpacity = 0.78;
    } else if (presetIdx == 2) {
        // 2: Windows 11 (Единая скруглённая капсула по центру, без разделения на 3 части)
        m_screenEdge = 0;
        m_dockMode = true;
        m_splitThreePills = false;
        m_showWorkspaces = false;
        m_style = 1; // Размытие
        m_tintColor = QColor("#0a0a0c");
        m_tintOpacity = 0.35;
        m_showBorder = true;
        m_dockRadius = 12;
        m_dockMarginX = 12;
        m_dockMarginY = 2;
        m_workAreaGap = -2;
    } else if (presetIdx == 3) {
        // 3: macOS / GNOME (Пуск + Воркспейсы + Трей сверху, Док с приложениями снизу)
        m_screenEdge = 2;
        m_dockMode = true;
        m_splitThreePills = true;
        m_showWorkspaces = true;
        m_style = 1; // Размытие
        m_tintColor = QColor("#0a0a0c");
        m_tintOpacity = 0.38;
        m_showBorder = true;
        m_dockRadius = 10;
        m_dockMarginX = 6;
        m_dockMarginY = 1;
        m_workAreaGap = 0;
    } else if (presetIdx == 4) {
        // 4: Призрак (Полностью прозрачная панель без капсул, парящие иконки по центру)
        m_screenEdge = 0;
        m_dockMode = false;
        m_splitThreePills = false;
        m_showWorkspaces = false;
        m_centerIcons = true;
        m_smoothAnimation = true;
        m_style = 0; // Прозрачная
        m_tintOpacity = 0.0;
        m_showBorder = false;
        m_maxTriggerEnabled = true;
        m_maxStyle = 1;
        m_maxTintOpacity = 0.45;
    }

    s_workspacesEnabledGlobal = m_showWorkspaces;
    if (!m_showWorkspaces) {
        restoreAllWorkspacesOnExit();
        destroyWsOverlay();
    }
    if (!m_dockMode) {
        restoreNativeTaskbar();
    }
    m_lastIconsSpan = 0;
    s_lastBackdropKey.clear();
    s_lastWsRenderKey.clear();
    s_lastAppliedWorkArea = { -1, -1, -1, -1 };
    applyVisibilityFlags();
    if (!m_timer.isActive()) m_timer.start();
    onTick();
    emit stateChanged();
    save();
}

void TaskbarManager::resetDefaults() {
    applyPreset(0);
}