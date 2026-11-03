#include "ThemeEngine.h"
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QGuiApplication>
#include <QWindow>
#include <QQuickWindow>
#include <algorithm>
#include <cmath>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

struct ACCENT_POLICY {
    int AccentState;
    int AccentFlags;
    DWORD GradientColor;
    int AnimationId;
};
struct WINDOWCOMPOSITIONATTRIBDATA {
    DWORD Attrib;
    PVOID pvData;
    SIZE_T cbData;
};
typedef BOOL(WINAPI *pfnSetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBDATA *);

static void applyWin32WindowBlur(qreal val) {
    static pfnSetWindowCompositionAttribute setAttr = nullptr;
    if (!setAttr) {
        if (HMODULE hUser = GetModuleHandleW(L"user32.dll")) {
            setAttr = reinterpret_cast<pfnSetWindowCompositionAttribute>(
                GetProcAddress(hUser, "SetWindowCompositionAttribute"));
        }
    }
    if (!setAttr) return;

    for (QWindow *w : QGuiApplication::topLevelWindows()) {
        if (w->objectName() != "mainControlWindow") continue;
        HWND hwnd = reinterpret_cast<HWND>(w->winId()); if (hwnd) { SetWindowTextW(hwnd, L""); LONG_PTR st = GetWindowLongPtrW(hwnd, GWL_STYLE); SetWindowLongPtrW(hwnd, GWL_STYLE, st & ~(WS_CAPTION | WS_THICKFRAME | WS_SYSMENU)); }
        if (!hwnd) continue;

        ACCENT_POLICY policy = {};
        if (val <= 0.01) {
            policy.AccentState = 0; // ACCENT_DISABLED
        } else {
            // Используем ACCENT_ENABLE_BLURBEHIND (3), т.к. он не вызывает лагов перетаскивания на Windows 10!
            policy.AccentState = 3;
            policy.AccentFlags = 2;
            int alpha = std::clamp(int(val * 210.0), 15, 220);
            policy.GradientColor = (DWORD(alpha) << 24) | 0x000A0A0CU;
        }
        WINDOWCOMPOSITIONATTRIBDATA data = {19, &policy, sizeof(policy)};
        setAttr(hwnd, &data);
    }
}
#endif

static QString ccConfPath() {
    return QDir::homePath() + "/.config/hypr/scripts/control_center.conf";
}

static void updateConfKey(const QString &key, const QString &val) {
    QDir().mkpath(QDir::homePath() + "/.config/hypr/scripts");
    QFile file(ccConfPath());
    QStringList lines;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) lines.append(in.readLine());
        file.close();
    }
    bool found = false;
    for (int i = 0; i < lines.size(); ++i) {
        if (lines[i].trimmed().startsWith(key + "=")) {
            lines[i] = key + "=" + val;
            found = true;
            break;
        }
    }
    if (!found) lines.append(key + "=" + val);

    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        for (const auto &l : lines) out << l << "\n";
    }
}

ThemeEngine::ThemeEngine(QObject *parent) : QObject(parent) {
    m_muteTimer = new QTimer(this);
    m_muteTimer->setSingleShot(true);
    m_muteTimer->setInterval(650);
    connect(m_muteTimer, &QTimer::timeout, this, [this]() { m_ignoreWatcher = false; });

    m_blurTimer = new QTimer(this);
    m_blurTimer->setSingleShot(true);
    m_blurTimer->setInterval(45);
    connect(m_blurTimer, &QTimer::timeout, this, [this]() {
#ifdef Q_OS_WIN
        applyWin32WindowBlur(m_pendingBlur);
#endif
    });

    QFile f(ccConfPath());
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&f);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            int eq = line.indexOf('=');
            if (eq == -1) continue;
            QString k = line.left(eq).trimmed();
            QString v = line.mid(eq + 1).trimmed();
            if (k == "window_blur") m_windowBlur = std::clamp(v.toDouble(), 0.0, 1.0);
            else if (k == "glass_refraction") m_glassRefraction = std::clamp(v.toDouble(), 0.0, 1.0);
            else if (k == "controls_liquid_glass") m_controlsLiquidGlass = (v == "1" || v == "true");
        }
        f.close();
    }

    m_watcher = new QFileSystemWatcher(this);
    QString scriptsDir = QDir::homePath() + "/.config/hypr/scripts";
    QDir().mkpath(scriptsDir);
    m_watcher->addPath(scriptsDir);
    checkThemeFile();
}

void ThemeEngine::checkThemeFile() {
    if (m_ignoreWatcher) return;
    QFile file(QDir::homePath() + "/.config/hypr/scripts/theme_state.conf");
    int newRadius = 8;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString state = QTextStream(&file).readLine().trimmed();
        newRadius = (state == "square") ? 0 : 8;
    }
    if (m_cornerRadius != newRadius) {
        m_cornerRadius = newRadius;
        emit cornerRadiusChanged();
        emit isRoundedChanged();
    }
}

void ThemeEngine::applyEffectiveBlur(qreal val) {
    m_pendingBlur = std::clamp(val, 0.0, 1.0);
    m_blurTimer->start();
}

void ThemeEngine::setWindowBlur(qreal val) {
    val = std::clamp(val, 0.0, 1.0);
    if (qFuzzyCompare(m_windowBlur, val)) return;
    m_windowBlur = val;
    updateConfKey("window_blur", QString::number(m_windowBlur, 'f', 2));
    emit windowBlurChanged();
    applyEffectiveBlur(val);
}

void ThemeEngine::setGlassRefraction(qreal val) {
    val = std::clamp(val, 0.0, 1.0);
    if (qFuzzyCompare(m_glassRefraction, val)) return;
    m_glassRefraction = val;
    updateConfKey("glass_refraction", QString::number(m_glassRefraction, 'f', 2));
    emit glassRefractionChanged();
}

void ThemeEngine::setControlsLiquidGlass(bool val) {
    if (m_controlsLiquidGlass == val) return;
    m_controlsLiquidGlass = val;
    updateConfKey("controls_liquid_glass", m_controlsLiquidGlass ? "1" : "0");
    emit controlsLiquidGlassChanged();
}

void ThemeEngine::setRoundedState(bool rounded) {
    int nextRadius = rounded ? 8 : 0;
    if (m_cornerRadius == nextRadius) return;
    m_ignoreWatcher = true;
    m_muteTimer->start();
    m_cornerRadius = nextRadius;
    emit cornerRadiusChanged();
    emit isRoundedChanged();
    QFile file(QDir::homePath() + "/.config/hypr/scripts/theme_state.conf");
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream(&file) << (rounded ? "rounded" : "square") << "\n";
    }
}

void ThemeEngine::toggleThemeState() { setRoundedState(m_cornerRadius == 0); }
void ThemeEngine::reloadTheme() { checkThemeFile(); }