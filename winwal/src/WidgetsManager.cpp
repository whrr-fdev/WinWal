#include <QFontDatabase>
#include "WidgetsManager.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QDateTime>
#include <QLocale>
#include <QGuiApplication>
#include <QScreen>
#include <QQuickWindow>
#include <QQuickPaintedItem>
#include <QPainter>
#include <QPainterPath>
#include <QStorageInfo>
#include <QSysInfo>
#include <deque>
#include <algorithm>
#include <cmath>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <iphlpapi.h>
#endif

static void makeWindowClickThrough(QQuickWindow *w, bool topMost) {
    if (!w) return;
    w->setProperty("isOverlayWindow", true);
    w->setColor(Qt::transparent);
    Qt::WindowFlags flags = Qt::Window | Qt::FramelessWindowHint | Qt::Tool | Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput;
    if (topMost) flags |= Qt::WindowStaysOnTopHint;
    
    w->setFlags(flags);
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(w->winId());
    if (hwnd) {
        LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
        ex |= WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex);
        SetWindowPos(hwnd, topMost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
#endif
}

// ============================================================================
// 1. ВСТРОЕННЫЙ CLOCK OVERLAY (1:1 порт clock_overlay)
// ============================================================================
class EmbeddedClockItem : public QQuickPaintedItem {
public:
    WidgetsManager *mgr = nullptr;
    QString anuratiFamily;

    explicit EmbeddedClockItem(WidgetsManager *m, QQuickItem *parent = nullptr)
        : QQuickPaintedItem(parent), mgr(m) {
        setAntialiasing(true);
        setRenderTarget(QQuickPaintedItem::FramebufferObject);
        setMipmap(true);
        int id1 = QFontDatabase::addApplicationFont(":/fonts/Anurati-Regular.otf");
        if (id1 < 0) {
            id1 = QFontDatabase::addApplicationFont(QCoreApplication::applicationDirPath() + "/assets/fonts/Anurati-Regular.otf");
        }
        if (id1 >= 0 && !QFontDatabase::applicationFontFamilies(id1).isEmpty()) {
            anuratiFamily = QFontDatabase::applicationFontFamilies(id1).first();
        } else {
            anuratiFamily = "Anurati";
        }
    }

    void paint(QPainter *p) override {
        if (!mgr) return;
        p->setRenderHint(QPainter::Antialiasing, true);
        p->setRenderHint(QPainter::TextAntialiasing, true);
        p->setRenderHint(QPainter::SmoothPixmapTransform, true);

        QDateTime now = QDateTime::currentDateTime();
        QLocale engLoc(QLocale::English, QLocale::UnitedStates);
        qreal alpha = std::clamp(mgr->clockOpacity(), 0.1, 1.0);
        QColor textCol(255, 255, 255, int(alpha * 255));
        QColor shadowCol(0, 0, 0, int(alpha * 105));

        int daySize = mgr->clockSize();
        int dateSize = std::max(11, daySize / 4);
        int timeSize = std::max(12, daySize / 4 + 1);

        qreal yCursor = 24;
        qreal w = width();

        auto drawCenteredText = [&](const QString &txt, QFont f, qreal yPos, qreal shadowDy) -> qreal {
            f.setStyleStrategy(static_cast<QFont::StyleStrategy>(QFont::PreferAntialias | QFont::PreferQuality));
            f.setHintingPreference(QFont::PreferNoHinting);
            QFontMetricsF fm(f);
            qreal textW = fm.horizontalAdvance(txt);
            qreal xPos = (w - textW) * 0.5;
            qreal baselineY = yPos + fm.ascent();

            QPainterPath textPath;
            textPath.addText(QPointF(xPos, baselineY), f, txt);

            p->setPen(Qt::NoPen);
            p->fillPath(textPath.translated(0.0, shadowDy), shadowCol);
            p->fillPath(textPath, textCol);
            return fm.height() + 10.0;
        };

        if (mgr->clockShowWeekday()) {
            QFont df(anuratiFamily.isEmpty() ? "Anurati" : anuratiFamily);
            df.setPixelSize(daySize);
            df.setWeight(QFont::Normal);
            df.setLetterSpacing(QFont::AbsoluteSpacing, std::max(10.0, daySize * 0.22));
            QString dayStr = engLoc.toString(now.date(), "dddd").toUpper();
            yCursor += drawCenteredText(dayStr, df, yCursor, 3.5);
        }

        if (mgr->clockShowDate()) {
            QFont f("Segoe UI", 10, QFont::DemiBold);
            f.setPixelSize(dateSize);
            f.setLetterSpacing(QFont::AbsoluteSpacing, 2.5);
            QString dateStr = engLoc.toString(now.date(), "dd MMMM, yyyy.").toUpper();
            yCursor += drawCenteredText(dateStr, f, yCursor, 2.0);
        }

        QFont tf("Segoe UI", 10, QFont::DemiBold);
        tf.setPixelSize(timeSize);
        tf.setLetterSpacing(QFont::AbsoluteSpacing, 3.0);
        QString fmt = mgr->clock24h()
            ? (mgr->clockShowSeconds() ? "- HH:mm:ss -" : "- HH:mm -")
            : (mgr->clockShowSeconds() ? "- hh:mm:ss AP -" : "- hh:mm AP -");
        drawCenteredText(engLoc.toString(now.time(), fmt), tf, yCursor, 2.0);
    }
};

// ============================================================================
// 2. ВСТРОЕННЫЙ SYSINFO OVERLAY (1:1 порт sysinfo_overlay)
// ============================================================================
class EmbeddedSysInfoItem : public QQuickPaintedItem {
public:
    WidgetsManager *mgr = nullptr;
    std::deque<double> cpuHist = std::deque<double>(60, 0.0);
    std::deque<double> ramHist = std::deque<double>(60, 0.0);
    std::deque<double> rxHist  = std::deque<double>(60, 0.0);
    std::deque<double> txHist  = std::deque<double>(60, 0.0);
    std::deque<double> tmpHist = std::deque<double>(60, 40.0);

    QString cpuText = "0.0%  •  3.60 GHz";
    QString ramText = "0.0 / 16.0 GB (0%)";
    QString netText = "↓ 0 KB/s   ↑ 0 KB/s";
    QString tmpText = "42.0 °C";
    QString diskText = "DISK (C:): ...";
    QString upText   = "UPTIME: 00h 00m";
    QString kernText = "KERNEL: Windows 10";

    quint64 prevIdle = 0, prevTotal = 0;
    quint64 prevRx = 0, prevTx = 0;
    int tickCount = 0;

    explicit EmbeddedSysInfoItem(WidgetsManager *m, QQuickItem *parent = nullptr)
        : QQuickPaintedItem(parent), mgr(m) {
        setAntialiasing(true);
        kernText = "KERNEL:   " + QSysInfo::prettyProductName();
        sampleMetrics();
    }

    void sampleMetrics() {
        tickCount++;
        double cpuPct = 0.0;
        double ghz = 3.60;
#ifdef Q_OS_WIN
        FILETIME idleFt, kernFt, userFt;
        if (GetSystemTimes(&idleFt, &kernFt, &userFt)) {
            auto toU64 = [](const FILETIME &ft) {
                return (quint64(ft.dwHighDateTime) << 32) | quint64(ft.dwLowDateTime);
            };
            quint64 idl = toU64(idleFt);
            quint64 tot = toU64(kernFt) + toU64(userFt);
            quint64 dTot = tot - prevTotal;
            quint64 dIdl = idl - prevIdle;
            prevTotal = tot;
            prevIdle = idl;
            if (dTot > 0 && tot != dTot) {
                cpuPct = std::clamp(double(dTot - dIdl) * 100.0 / double(dTot), 0.0, 100.0);
            }
        }
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD mhz = 0, sz = sizeof(DWORD);
            if (RegQueryValueExW(hKey, L"~MHz", nullptr, nullptr, reinterpret_cast<LPBYTE>(&mhz), &sz) == ERROR_SUCCESS && mhz > 0) {
                ghz = double(mhz) / 1000.0;
            }
            RegCloseKey(hKey);
        }

        MEMORYSTATUSEX mem = {};
        mem.dwLength = sizeof(mem);
        if (GlobalMemoryStatusEx(&mem)) {
            double totGb = double(mem.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0);
            double availGb = double(mem.ullAvailPhys) / (1024.0 * 1024.0 * 1024.0);
            double usedGb = std::max(0.0, totGb - availGb);
            double pct = totGb > 0.0 ? (usedGb / totGb) * 100.0 : 0.0;
            ramHist.pop_front();
            ramHist.push_back(pct);
            ramText = QString("%1 / %2 GB (%3%)").arg(usedGb, 0, 'f', 1).arg(totGb, 0, 'f', 1).arg(qRound(pct));
        }

        ULONG bufLen = 0;
        if (GetIfTable(nullptr, &bufLen, FALSE) == ERROR_INSUFFICIENT_BUFFER) {
            QByteArray buf(bufLen, 0);
            auto *pTable = reinterpret_cast<PMIB_IFTABLE>(buf.data());
            if (GetIfTable(pTable, &bufLen, FALSE) == NO_ERROR) {
                quint64 rx = 0, tx = 0;
                for (DWORD i = 0; i < pTable->dwNumEntries; ++i) {
                    const MIB_IFROW &row = pTable->table[i];
                    if (row.dwType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
                    rx += row.dwInOctets;
                    tx += row.dwOutOctets;
                }
                double dRx = (prevRx > 0 && rx >= prevRx) ? double(rx - prevRx) : 0.0;
                double dTx = (prevTx > 0 && tx >= prevTx) ? double(tx - prevTx) : 0.0;
                prevRx = rx;
                prevTx = tx;
                rxHist.pop_front(); rxHist.push_back(dRx);
                txHist.pop_front(); txHist.push_back(dTx);
                auto fmtSpd = [](double bps) {
                    if (bps >= 1048576.0) return QString("%1 MB/s").arg(bps / 1048576.0, 0, 'f', 1);
                    return QString("%1 KB/s").arg(bps / 1024.0, 0, 'f', 0);
                };
                netText = QString("↓ %1   ↑ %2").arg(fmtSpd(dRx), fmtSpd(dTx));
            }
        }

        quint64 upSecs = GetTickCount64() / 1000ULL;
        upText = QString("UPTIME:   %1h %2m").arg(upSecs / 3600, 2, 10, QChar('0')).arg((upSecs % 3600) / 60, 2, 10, QChar('0'));
#endif
        cpuHist.pop_front();
        cpuHist.push_back(cpuPct);
        cpuText = QString("%1%  •  %2 GHz").arg(cpuPct, 0, 'f', 1).arg(ghz, 0, 'f', 2);

        double estTemp = 38.0 + cpuPct * 0.42;
        tmpHist.pop_front();
        tmpHist.push_back(estTemp);
        tmpText = QString("%1 °C").arg(estTemp, 0, 'f', 1);

        if (tickCount % 5 == 1) {
            QStorageInfo storage = QStorageInfo::root();
            if (storage.isValid() && storage.bytesTotal() > 0) {
                double totG = double(storage.bytesTotal()) / (1024.0 * 1024.0 * 1024.0);
                double usedG = double(storage.bytesTotal() - storage.bytesAvailable()) / (1024.0 * 1024.0 * 1024.0);
                double pct = (usedG / totG) * 100.0;
                diskText = QString("DISK (%1): %2 / %3 GB (%4%)")
                    .arg(storage.rootPath())
                    .arg(usedG, 0, 'f', 1)
                    .arg(totG, 0, 'f', 1)
                    .arg(qRound(pct));
            }
        }
        update();
    }

    void drawGraph(QPainter *p, QRectF rect, const std::deque<double> &data, double maxVal, const QColor &col, double opacity, double scale) {
        QColor baseCol = col;
        baseCol.setAlphaF(std::clamp(0.22 * opacity, 0.0, 1.0));
        p->setPen(QPen(baseCol, 1.0 * std::clamp(scale, 0.6, 2.0)));
        p->drawLine(QPointF(rect.left(), rect.bottom() - 1.0), QPointF(rect.right(), rect.bottom() - 1.0));

        if (data.size() < 2) return;
        double effMax = std::max(1.0, maxVal);
        double step = rect.width() / double(data.size() - 1);

        QPainterPath fillPath;
        QPainterPath strokePath;
        fillPath.moveTo(rect.left(), rect.bottom() - 1.0);
        for (size_t i = 0; i < data.size(); ++i) {
            double x = rect.left() + i * step;
            double norm = std::clamp(data[i] / effMax, 0.0, 1.0);
            double y = (rect.bottom() - 2.0) - norm * (rect.height() - 4.0);
            fillPath.lineTo(x, y);
            if (i == 0) strokePath.moveTo(x, y);
            else strokePath.lineTo(x, y);
        }
        fillPath.lineTo(rect.right(), rect.bottom() - 1.0);
        fillPath.closeSubpath();

        QColor fillCol = col;
        fillCol.setAlphaF(std::clamp(0.16 * opacity, 0.0, 1.0));
        p->fillPath(fillPath, fillCol);

        QColor lineCol = col;
        lineCol.setAlphaF(std::clamp(0.95 * opacity, 0.0, 1.0));
        p->setPen(QPen(lineCol, 1.8 * std::clamp(scale, 0.6, 2.0)));
        p->drawPath(strokePath);
    }

    void paint(QPainter *p) override {
        if (!mgr) return;
        p->setRenderHint(QPainter::Antialiasing, true);
        p->setRenderHint(QPainter::TextAntialiasing, true);

        double sc = mgr->sysScale();
        double op = mgr->sysOpacity();
        QColor txtCol(mgr->sysTextColor());
        if (!txtCol.isValid()) txtCol = Qt::white;
        txtCol.setAlphaF(op);

        QColor grCol(mgr->sysGraphColor());
        if (!grCol.isValid()) grCol = Qt::white;

        int titlePx = std::max(6, int(std::round(mgr->sysTitleSize() * sc)));
        int extraPx = std::max(6, int(std::round(mgr->sysExtraSize() * sc)));
        double maxFont = std::max(mgr->sysTitleSize(), mgr->sysExtraSize());
        double gw = std::max(90.0, std::round(260.0 * sc * std::max(0.85, maxFont / 12.0)));
        double gh = std::max(14.0, std::round(36.0 * sc));
        double pad = std::max(4.0, std::round(10.0 * sc));
        double y = pad;

        QFont tf("Consolas", 10, QFont::Bold);
        tf.setPixelSize(titlePx);

        auto drawSection = [&](const QString &label, const QString &val, const std::deque<double> &hist, double maxV, const std::deque<double> *hist2 = nullptr) {
            p->setFont(tf);
            QRectF hdrRect(pad, y, gw, titlePx + 6);
            p->setPen(QColor(0, 0, 0, int(115 * op)));
            p->drawText(hdrRect.translated(0, 1), Qt::AlignLeft | Qt::AlignVCenter, label);
            p->drawText(hdrRect.translated(0, 1), Qt::AlignRight | Qt::AlignVCenter, val);
            p->setPen(txtCol);
            p->drawText(hdrRect, Qt::AlignLeft | Qt::AlignVCenter, label);
            p->drawText(hdrRect, Qt::AlignRight | Qt::AlignVCenter, val);
            y += titlePx + 6 + std::max(1.0, std::round(4.0 * sc));

            QRectF gRect(pad, y, gw, gh);
            drawGraph(p, gRect, hist, maxV, grCol, op, sc);
            if (hist2) {
                drawGraph(p, gRect, *hist2, maxV, grCol, op * 0.45, sc);
            }
            y += gh + std::max(2.0, std::round(10.0 * sc));
        };

        if (mgr->sysShowCpu()) drawSection("󰻠  CPU", cpuText, cpuHist, 100.0);
        if (mgr->sysShowRam()) drawSection("󰍛  RAM", ramText, ramHist, 100.0);
        if (mgr->sysShowNet()) {
            double maxNet = 10240.0;
            for (double v : rxHist) maxNet = std::max(maxNet, v);
            for (double v : txHist) maxNet = std::max(maxNet, v);
            drawSection("󰛳  NET", netText, rxHist, maxNet, &txHist);
        }
        if (mgr->sysShowTemp()) drawSection("  TEMP", tmpText, tmpHist, 100.0);

        if (mgr->sysShowExtra()) {
            QFont ef("Consolas", 10, QFont::Bold);
            ef.setPixelSize(extraPx);
            p->setFont(ef);
            QColor subCol = txtCol;
            subCol.setAlphaF(std::clamp(op * 0.92, 0.1, 1.0));
            for (const QString &line : {diskText, upText, kernText}) {
                QRectF lr(pad, y, gw, extraPx + 5);
                p->setPen(QColor(0, 0, 0, int(115 * op)));
                p->drawText(lr.translated(0, 1), Qt::AlignLeft | Qt::AlignVCenter, line);
                p->setPen(subCol);
                p->drawText(lr, Qt::AlignLeft | Qt::AlignVCenter, line);
                y += extraPx + 4 + std::max(1.0, std::round(3.5 * sc));
            }
        }
    }
};

// ============================================================================
// 3. ВСТРОЕННЫЙ KEYBOARD & MOUSE OVERLAY (1:1 порт kb_overlay.py)
// ============================================================================
struct KeyCapDef {
    const char *en;
    const char *ru;
    int vk;
    double widthU;
};

static const std::vector<std::vector<KeyCapDef>> S_KB_ROWS = {
    {{"Esc","Esc",0x1B,1.0},{"F1","F1",0x70,1.0},{"F2","F2",0x71,1.0},{"F3","F3",0x72,1.0},
     {"F4","F4",0x73,1.0},{"F5","F5",0x74,1.0},{"F6","F6",0x75,1.0},{"F7","F7",0x76,1.0},
     {"F8","F8",0x77,1.0},{"F9","F9",0x78,1.0},{"F10","F10",0x79,1.0},{"F11","F11",0x7A,1.0},
     {"F12","F12",0x7B,1.0},{"Prt","Prt",0x2C,1.0},{"Ins","Ins",0x2D,1.0},{"Del","Del",0x2E,1.0}},
    {{"`","Ё",0xC0,1.0},{"1","1",'1',1.0},{"2","2",'2',1.0},{"3","3",'3',1.0},
     {"4","4",'4',1.0},{"5","5",'5',1.0},{"6","6",'6',1.0},{"7","7",'7',1.0},
     {"8","8",'8',1.0},{"9","9",'9',1.0},{"0","0",'0',1.0},{"-","-",0xBD,1.0},
     {"=","=",0xBB,1.0},{"Bksp","Bksp",0x08,2.0},{"Hm","Hm",0x24,1.0}},
    {{"Tab","Tab",0x09,1.5},{"Q","Й",'Q',1.0},{"W","Ц",'W',1.0},{"E","У",'E',1.0},
     {"R","К",'R',1.0},{"T","Е",'T',1.0},{"Y","Н",'Y',1.0},{"U","Г",'U',1.0},
     {"I","Ш",'I',1.0},{"O","Щ",'O',1.0},{"P","З",'P',1.0},{"[","Х",0xDB,1.0},
     {"]","Ъ",0xDD,1.0},{"\\","\\",0xDC,1.5},{"PU","PU",0x21,1.0}},
    {{"Caps","Caps",0x14,1.75},{"A","Ф",'A',1.0},{"S","Ы",'S',1.0},{"D","В",'D',1.0},
     {"F","А",'F',1.0},{"G","П",'G',1.0},{"H","Р",'H',1.0},{"J","О",'J',1.0},
     {"K","Л",'K',1.0},{"L","Д",'L',1.0},{";","Ж",0xBA,1.0},{"'","Э",0xDE,1.0},
     {"Ent","Ent",0x0D,2.25},{"PD","PD",0x22,1.0}},
    {{"Shift","Shift",0xA0,2.25},{"Z","Я",'Z',1.0},{"X","Ч",'X',1.0},{"C","С",'C',1.0},
     {"V","М",'V',1.0},{"B","И",'B',1.0},{"N","Т",'N',1.0},{"M","Ь",'M',1.0},
     {",","Б",0xBC,1.0},{".","Ю",0xBE,1.0},{"/",".",0xBF,1.0},{"Shift","Shift",0xA1,1.75},
     {"↑","↑",0x26,1.0},{"End","End",0x23,1.0}},
    {{"Ctrl","Ctrl",0xA2,1.25},{"Win","Win",0x5B,1.25},{"Alt","Alt",0xA4,1.25},
     {"Space","Space",0x20,4.25},
     {"Alt","Alt",0xA5,1.25},{"Fn","Fn",0xFF,1.25},{"Win","Win",0x5C,1.25},{"Ctrl","Ctrl",0xA3,1.25},
     {"←","←",0x25,1.0},{"↓","↓",0x28,1.0},{"→","→",0x27,1.0}}
};

class EmbeddedKbItem : public QQuickPaintedItem {
public:
    WidgetsManager *mgr = nullptr;
    QSet<int> activeVks;
    bool isRu = false;
    int wheelFlashTicks = 0;

    explicit EmbeddedKbItem(WidgetsManager *m, QQuickItem *parent = nullptr)
        : QQuickPaintedItem(parent), mgr(m) {
        setAntialiasing(true);
    }

    void pollKeys() {
#ifdef Q_OS_WIN
        QSet<int> nextVks;
        bool modHeld = (GetAsyncKeyState(VK_CONTROL) & 0x8000) ||
                       (GetAsyncKeyState(VK_MENU) & 0x8000) ||
                       (GetAsyncKeyState(VK_SHIFT) & 0x8000) ||
                       (GetAsyncKeyState(VK_LWIN) & 0x8000) ||
                       (GetAsyncKeyState(VK_RWIN) & 0x8000);

        for (const auto &row : S_KB_ROWS) {
            for (const auto &k : row) {
                if (GetAsyncKeyState(k.vk) & 0x8000) {
                    if (!mgr->kbOnlyCombos() || modHeld) {
                        nextVks.insert(k.vk);
                    }
                }
            }
        }
        for (int mvk : {VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2}) {
            if (GetAsyncKeyState(mvk) & 0x8000) nextVks.insert(mvk);
        }

        HWND fg = GetForegroundWindow();
        DWORD tid = fg ? GetWindowThreadProcessId(fg, nullptr) : 0;
        HKL hkl = GetKeyboardLayout(tid);
        bool nextRu = (LOWORD(reinterpret_cast< quintptr >(hkl)) == 0x0419);

        if (wheelFlashTicks > 0) wheelFlashTicks--;

        if (nextVks != activeVks || nextRu != isRu || wheelFlashTicks > 0) {
            activeVks = nextVks;
            isRu = nextRu;
            update();
        }
#endif
    }

    void paint(QPainter *p) override {
        if (!mgr) return;
        p->setRenderHint(QPainter::Antialiasing, true);
        p->setRenderHint(QPainter::TextAntialiasing, true);

        const double cell = 21.0;
        const double gap = 3.0;
        const double keyH = 18.0;
        double xOffset = 6.0;

        QFont kf("Segoe UI", 7, QFont::Bold);
        kf.setPixelSize(8);
        p->setFont(kf);

        if (mgr->kbShowKeyboard()) {
            double y = 6.0;
            for (const auto &row : S_KB_ROWS) {
                double x = xOffset;
                for (const auto &k : row) {
                    double kw = std::round(k.widthU * cell) - gap;
                    QRectF kr(x, y, kw, keyH);
                    bool act = activeVks.contains(k.vk);
                    p->setBrush(act ? QColor(255, 255, 255, 165) : QColor(30, 30, 30, 75));
                    p->setPen(QPen(act ? QColor(255, 255, 255, 200) : QColor(255, 255, 255, 22), 1.0));
                    p->drawRoundedRect(kr, 4.0, 4.0);

                    p->setPen(act ? QColor(0, 0, 0, 225) : QColor(220, 220, 220, 130));
                    p->drawText(kr, Qt::AlignCenter, isRu ? QString::fromUtf8(k.ru) : QString::fromUtf8(k.en));
                    x += kw + gap;
                }
                y += keyH + gap;
            }
            xOffset += 16.0 * cell + 12.0;
        }

        if (mgr->kbShowMouse()) {
            double palmW = std::round(cell * 2.8);
            double topH = std::round(keyH * 1.6);
            double palmH = std::round(keyH * 3.4);
            double totalH = topH + palmH;
            double my = 6.0 + (6.0 * (keyH + gap) - totalH) * 0.5;

            QRectF bodyRect(xOffset, my, palmW, totalH);
            QPainterPath bodyPath;
            bodyPath.addRoundedRect(bodyRect, 28.0, 28.0);
            p->save();
            p->setClipPath(bodyPath);

            p->fillRect(bodyRect, QColor(30, 30, 30, 75));
            QRectF lmbRect(xOffset, my, palmW * 0.5, topH);
            QRectF rmbRect(xOffset + palmW * 0.5, my, palmW * 0.5, topH);
            if (activeVks.contains(0x01)) p->fillRect(lmbRect, QColor(255, 255, 255, 165));
            if (activeVks.contains(0x02)) p->fillRect(rmbRect, QColor(255, 255, 255, 165));
            p->setPen(QPen(QColor(255, 255, 255, 25), 1.0));
            p->drawLine(QPointF(xOffset + palmW * 0.5, my), QPointF(xOffset + palmW * 0.5, my + topH));
            p->drawLine(QPointF(xOffset, my + topH), QPointF(xOffset + palmW, my + topH));
            p->restore();

            p->setBrush(Qt::NoBrush);
            p->setPen(QPen(QColor(255, 255, 255, 28), 1.0));
            p->drawRoundedRect(bodyRect, 28.0, 28.0);

            if (mgr->kbShowWheel()) {
                double ww = std::round(cell * 0.4);
                double wh = std::round(keyH * 0.9);
                QRectF wr(xOffset + (palmW - ww) * 0.5, my + 6.0, ww, wh);
                bool wAct = activeVks.contains(0x04) || wheelFlashTicks > 0;
                p->setBrush(wAct ? QColor(235, 235, 235) : QColor(75, 75, 75));
                p->setPen(QPen(wAct ? QColor(255, 255, 255) : QColor(105, 105, 105), 1.0));
                p->drawRoundedRect(wr, 3.0, 3.0);
            }

            if (mgr->kbShowSide()) {
                QRectF m5Rect(xOffset + 1.0, my + topH + 2.0, 4.0, 14.0);
                QRectF m4Rect(xOffset + 1.0, my + topH + 20.0, 4.0, 14.0);
                p->setPen(Qt::NoPen);
                p->setBrush(activeVks.contains(0x06) ? QColor(255, 255, 255, 210) : QColor(255, 255, 255, 45));
                p->drawRoundedRect(m5Rect, 2.0, 2.0);
                p->setBrush(activeVks.contains(0x05) ? QColor(255, 255, 255, 210) : QColor(255, 255, 255, 45));
                p->drawRoundedRect(m4Rect, 2.0, 2.0);
            }
        }
    }
};

static QQuickWindow *s_kbWin = nullptr;
static EmbeddedKbItem *s_kbItem = nullptr;
static QTimer *s_kbPollTimer = nullptr;

static QQuickWindow *s_clockWin = nullptr;
static EmbeddedClockItem *s_clockItem = nullptr;
static QTimer *s_clockTickTimer = nullptr;

static QQuickWindow *s_sysWin = nullptr;
static EmbeddedSysInfoItem *s_sysItem = nullptr;
static QTimer *s_sysTickTimer = nullptr;

#ifdef Q_OS_WIN
static HHOOK s_mouseWheelHook = nullptr;
static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && (wParam == WM_MOUSEWHEEL || wParam == WM_MOUSEHWHEEL)) {
        if (s_kbItem) {
            s_kbItem->wheelFlashTicks = 5;
        }
    }
    return CallNextHookEx(s_mouseWheelHook, nCode, wParam, lParam);
}
#endif

static void repositionKbWindow(WidgetsManager *mgr) {
    if (!s_kbWin || !s_kbItem || !mgr) return;
    QScreen *scr = QGuiApplication::primaryScreen();
    if (!scr) return;
    QRect sg = scr->geometry();
    int w = (mgr->kbShowKeyboard() ? 352 : 0) + (mgr->kbShowMouse() ? 76 : 0) + 16;
    int h = 138;
    s_kbWin->resize(w, h);
    s_kbItem->setSize(QSizeF(w, h));

    QString pos = mgr->kbPosition();
    int x = sg.right() - w - 24;
    int y = sg.bottom() - h - 48;
    if (pos.contains("left")) x = sg.left() + 24;
    else if (pos.contains("center")) x = sg.left() + (sg.width() - w) / 2;
    if (pos.startsWith("top")) y = sg.top() + 24;
    s_kbWin->setPosition(x, y);
    makeWindowClickThrough(s_kbWin, true);
}

static void repositionClockWindow(WidgetsManager *mgr) {
    if (!s_clockWin || !s_clockItem || !mgr) return;
    QScreen *scr = QGuiApplication::primaryScreen();
    if (!scr) return;
    QRect sg = scr->geometry();
    int w = std::max(600, mgr->clockSize() * 11);
    int h = mgr->clockSize() * 3 + 80;
    s_clockWin->resize(w, h);
    s_clockItem->setSize(QSizeF(w, h));
    s_clockWin->setPosition(sg.left() + (sg.width() - w) / 2 + mgr->clockOffsetX(), sg.top() + (sg.height() - h) / 2 + mgr->clockOffsetY());
    makeWindowClickThrough(s_clockWin, false);
    s_clockItem->update();
}

static void repositionSysWindow(WidgetsManager *mgr) {
    if (!s_sysWin || !s_sysItem || !mgr) return;
    QScreen *scr = QGuiApplication::primaryScreen();
    if (!scr) return;
    QRect sg = scr->geometry();
    double sc = mgr->sysScale();
    double maxFont = std::max(mgr->sysTitleSize(), mgr->sysExtraSize());
    int gw = std::max(90, int(std::round(260.0 * sc * std::max(0.85, maxFont / 12.0))));
    int w = gw + int(std::round(24.0 * sc));
    int h = int(std::round(360.0 * sc));
    s_sysWin->resize(w, h);
    s_sysItem->setSize(QSizeF(w, h));

    QString pos = mgr->sysPosition();
    int x = pos.endsWith("left") ? (sg.left() + mgr->sysMarginX()) : (sg.right() - w - mgr->sysMarginX());
    int y = pos.startsWith("bottom") ? (sg.bottom() - h - mgr->sysMarginY()) : (sg.top() + mgr->sysMarginY());
    s_sysWin->setPosition(x, y);
    makeWindowClickThrough(s_sysWin, mgr->sysTopLayer());
    s_sysItem->update();
}

WidgetsManager::WidgetsManager(QObject *parent) : QObject(parent) {
    QDir().mkpath(QDir::homePath() + "/.config/hypr/scripts");
    loadKbConfig();
    loadClockConfig();
    loadSysConfig();

    auto readEnabledConf = [](const QString &name) -> int {
        QFile f(QDir::homePath() + "/.config/hypr/scripts/" + name);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString val = QString::fromUtf8(f.readAll()).trimmed();
            if (val == "1" || val == "true") return 1;
            if (val == "0" || val == "false") return 0;
        }
        return -1;
    };

    if (readEnabledConf("kb.conf") == 1) {
        QTimer::singleShot(400, this, [this]() { toggleKbProcess(true); });
    }
    if (readEnabledConf("clock.conf") != 0) {
        QTimer::singleShot(450, this, [this]() { toggleClockProcess(true); });
    }
    if (readEnabledConf("sysinfo.conf") == 1) {
        QTimer::singleShot(500, this, [this]() { toggleSysProcess(true); });
    }
}

void WidgetsManager::loadKbConfig() {
    QString p = QDir::homePath() + "/.config/hypr/scripts/kb_overlay_config.json";
    QFile f(p);
    if (f.open(QIODevice::ReadOnly)) {
        auto doc = QJsonDocument::fromJson(f.readAll());
        if (doc.isObject()) {
            auto obj = doc.object();
            if (obj.contains("show_keyboard")) m_kbShowKeyboard = obj["show_keyboard"].toBool();
            if (obj.contains("show_mouse")) m_kbShowMouse = obj["show_mouse"].toBool();
            if (obj.contains("show_mouse_side")) m_kbShowSide = obj["show_mouse_side"].toBool();
            if (obj.contains("show_mouse_wheel")) m_kbShowWheel = obj["show_mouse_wheel"].toBool();
            if (obj.contains("only_combos")) m_kbOnlyCombos = obj["only_combos"].toBool();
            if (obj.contains("position") && !obj["position"].toString().isEmpty()) m_kbPosition = obj["position"].toString();
        }
    }
}

void WidgetsManager::saveKbConfig() {
    QDir().mkpath(QDir::homePath() + "/.config/hypr/scripts");
    QString p = QDir::homePath() + "/.config/hypr/scripts/kb_overlay_config.json";
    QJsonObject obj;
    obj["show_keyboard"] = m_kbShowKeyboard;
    obj["show_mouse"] = m_kbShowMouse;
    obj["show_mouse_side"] = m_kbShowSide;
    obj["show_mouse_wheel"] = m_kbShowWheel;
    obj["only_combos"] = m_kbOnlyCombos;
    obj["position"] = m_kbPosition;

    QFile f(p);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(obj).toJson());
        f.close();
    }
    emit kbChanged();
    if (m_kbRunning) repositionKbWindow(this);
}

void WidgetsManager::setKbShowKeyboard(bool v) { if (m_kbShowKeyboard != v) { m_kbShowKeyboard = v; saveKbConfig(); } }
void WidgetsManager::setKbShowMouse(bool v) { if (m_kbShowMouse != v) { m_kbShowMouse = v; saveKbConfig(); } }
void WidgetsManager::setKbShowSide(bool v) { if (m_kbShowSide != v) { m_kbShowSide = v; saveKbConfig(); } }
void WidgetsManager::setKbShowWheel(bool v) { if (m_kbShowWheel != v) { m_kbShowWheel = v; saveKbConfig(); } }
void WidgetsManager::setKbOnlyCombos(bool v) { if (m_kbOnlyCombos != v) { m_kbOnlyCombos = v; saveKbConfig(); } }
void WidgetsManager::setKbPosition(const QString &v) { if (m_kbPosition != v && !v.isEmpty()) { m_kbPosition = v; saveKbConfig(); } }

void WidgetsManager::toggleKbProcess(bool start) {
    m_kbRunning = start;
    QFile cf(QDir::homePath() + "/.config/hypr/scripts/kb.conf");
    if (cf.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream(&cf) << (start ? "1" : "0") << "\n";
        cf.close();
    }
    if (start) {
        if (!s_kbWin) {
            s_kbWin = new QQuickWindow();
            s_kbItem = new EmbeddedKbItem(this, s_kbWin->contentItem());
            s_kbPollTimer = new QTimer(this);
            s_kbPollTimer->setInterval(16);
            connect(s_kbPollTimer, &QTimer::timeout, this, []() {
                if (s_kbItem && s_kbWin && s_kbWin->isVisible()) s_kbItem->pollKeys();
            });
        }
        repositionKbWindow(this);
        s_kbWin->show();
        makeWindowClickThrough(s_kbWin, true);
        s_kbPollTimer->start();
#ifdef Q_OS_WIN
        if (!s_mouseWheelHook) {
            s_mouseWheelHook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, GetModuleHandleW(nullptr), 0);
        }
#endif
    } else {
        if (s_kbPollTimer) s_kbPollTimer->stop();
        if (s_kbWin) s_kbWin->hide();
#ifdef Q_OS_WIN
        if (s_mouseWheelHook) {
            UnhookWindowsHookEx(s_mouseWheelHook);
            s_mouseWheelHook = nullptr;
        }
#endif
    }
    emit kbChanged();
}

void WidgetsManager::toggleKbVisibility() {
    if (s_kbWin) {
        if (s_kbWin->isVisible()) s_kbWin->hide();
        else { s_kbWin->show(); makeWindowClickThrough(s_kbWin, true); }
    }
}

void WidgetsManager::restartKbProcess() {
    toggleKbProcess(true);
}

void WidgetsManager::loadClockConfig() {
    QString p = QDir::homePath() + "/.config/hypr/scripts/clock_style.conf";
    QFile f(p);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&f);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            int eq = line.indexOf('=');
            if (eq == -1) continue;
            QString k = line.left(eq).trimmed();
            QString v = line.mid(eq + 1).trimmed();
            if (k == "size" || k == "day_size") m_clockSize = std::clamp(qRound(v.toDouble()), 24, 140);
            else if (k == "opacity") m_clockOpacity = std::clamp(v.toDouble(), 0.1, 1.0);
            else if (k == "is_24h") m_clock24h = (v == "1" || v == "true");
            else if (k == "show_seconds") m_clockShowSeconds = (v == "1" || v == "true");
            else if (k == "show_date") m_clockShowDate = (v == "1" || v == "true");
            else if (k == "show_weekday") m_clockShowWeekday = (v == "1" || v == "true");
            else if (k == "pos_x") m_clockOffsetX = std::clamp(v.toInt(), -1200, 1200);
            else if (k == "pos_y") m_clockOffsetY = std::clamp(v.toInt(), -800, 800);
        }
    }
}

void WidgetsManager::saveClockConfig() {
    QDir().mkpath(QDir::homePath() + "/.config/hypr/scripts");
    QString p = QDir::homePath() + "/.config/hypr/scripts/clock_style.conf";
    QFile f(p);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream out(&f);
        out << "size=" << m_clockSize << "\n"
            << "day_size=" << m_clockSize << "\n"
            << "opacity=" << QString::number(m_clockOpacity, 'f', 2) << "\n"
            << "is_24h=" << (m_clock24h ? 1 : 0) << "\n"
            << "show_seconds=" << (m_clockShowSeconds ? 1 : 0) << "\n"
            << "show_date=" << (m_clockShowDate ? 1 : 0) << "\n"
            << "show_weekday=" << (m_clockShowWeekday ? 1 : 0) << "\n"
            << "pos_x=" << m_clockOffsetX << "\n"
            << "pos_y=" << m_clockOffsetY << "\n";
        f.close();
    }
    if (m_clockRunning) repositionClockWindow(this);
}

void WidgetsManager::setClock24h(bool v) { if (m_clock24h != v) { m_clock24h = v; saveClockConfig(); emit clockChanged(); } }
void WidgetsManager::setClockShowSeconds(bool v) { if (m_clockShowSeconds != v) { m_clockShowSeconds = v; saveClockConfig(); emit clockChanged(); } }
void WidgetsManager::setClockShowDate(bool v) { if (m_clockShowDate != v) { m_clockShowDate = v; saveClockConfig(); emit clockChanged(); } }
void WidgetsManager::setClockShowWeekday(bool v) { if (m_clockShowWeekday != v) { m_clockShowWeekday = v; saveClockConfig(); emit clockChanged(); } }
void WidgetsManager::setClockSize(int v) { v = std::clamp(v, 24, 140); if (m_clockSize != v) { m_clockSize = v; saveClockConfig(); emit clockChanged(); } }
void WidgetsManager::setClockOpacity(qreal v) { v = std::clamp(v, 0.1, 1.0); if (!qFuzzyCompare(m_clockOpacity, v)) { m_clockOpacity = v; saveClockConfig(); emit clockChanged(); } }

void WidgetsManager::saveClock() {
    saveClockConfig();
}

void WidgetsManager::toggleClockProcess(bool start) {
    m_clockRunning = start;
    QFile f(QDir::homePath() + "/.config/hypr/scripts/clock.conf");
    if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream(&f) << (start ? "1" : "0") << "\n";
        f.close();
    }
    if (start) {
        if (!s_clockWin) {
            s_clockWin = new QQuickWindow();
            s_clockWin->setObjectName("clockOverlayWin");
            s_clockItem = new EmbeddedClockItem(this, s_clockWin->contentItem());
            s_clockTickTimer = new QTimer(this);
            s_clockTickTimer->setInterval(1000);
            connect(s_clockTickTimer, &QTimer::timeout, this, []() {
                if (s_clockItem && s_clockWin && s_clockWin->isVisible()) {
                    s_clockItem->update();
                }
            });
        }
        repositionClockWindow(this);
        s_clockWin->show();
        makeWindowClickThrough(s_clockWin, false);
        if (HWND hClk = reinterpret_cast<HWND>(s_clockWin->winId())) {
            SetWindowPos(hClk, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
        }
        s_clockTickTimer->start();
    } else {
        if (s_clockTickTimer) s_clockTickTimer->stop();
        if (s_clockWin) s_clockWin->hide();
    }
    emit clockChanged();
}

void WidgetsManager::restartClockProcess() {
    toggleClockProcess(true);
}

void WidgetsManager::launchPeaclock() {}
void WidgetsManager::launchFastfetch() {}
void WidgetsManager::launchBtop() {}
QString WidgetsManager::findSysBinary() const { return QString(); }

void WidgetsManager::loadSysConfig() {
    QFile f(QDir::homePath() + "/.config/hypr/scripts/sysinfo_style.conf");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QTextStream in(&f);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        int eq = line.indexOf('=');
        if (line.isEmpty() || line.startsWith('#') || eq == -1) continue;
        QString k = line.left(eq).trimmed(), v = line.mid(eq + 1).trimmed();
        auto pb = [](const QString &s) { return s == "1" || s.compare("true", Qt::CaseInsensitive) == 0 || s == "yes"; };
        if (k == "show_cpu") m_sysShowCpu = pb(v);
        else if (k == "show_ram") m_sysShowRam = pb(v);
        else if (k == "show_net") m_sysShowNet = pb(v);
        else if (k == "show_temp") m_sysShowTemp = pb(v);
        else if (k == "show_extra") m_sysShowExtra = pb(v);
        else if (k == "top_layer") m_sysTopLayer = pb(v);
        else if (k == "text_color" && !v.isEmpty()) m_sysTextColor = v;
        else if (k == "graph_color" && !v.isEmpty()) m_sysGraphColor = v;
        else if (k == "opacity") m_sysOpacity = std::clamp(v.toDouble(), 0.1, 1.0);
        else if (k == "scale") m_sysScale = std::clamp(v.toDouble(), 0.4, 2.5);
        else if (k == "title_size") m_sysTitleSize = std::clamp(qRound(v.toDouble()), 6, 36);
        else if (k == "extra_size") m_sysExtraSize = std::clamp(qRound(v.toDouble()), 6, 36);
        else if (k == "margin_x") m_sysMarginX = qRound(v.toDouble());
        else if (k == "margin_y") m_sysMarginY = qRound(v.toDouble());
        else if (k == "position" && !v.isEmpty()) m_sysPosition = v;
    }
}

void WidgetsManager::saveSysConfig() {
    QDir().mkpath(QDir::homePath() + "/.config/hypr/scripts");
    QFile f(QDir::homePath() + "/.config/hypr/scripts/sysinfo_style.conf");
    if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream out(&f);
        out << "show_cpu=" << (m_sysShowCpu ? "true" : "false") << "\n"
            << "show_ram=" << (m_sysShowRam ? "true" : "false") << "\n"
            << "show_net=" << (m_sysShowNet ? "true" : "false") << "\n"
            << "show_temp=" << (m_sysShowTemp ? "true" : "false") << "\n"
            << "show_extra=" << (m_sysShowExtra ? "true" : "false") << "\n"
            << "top_layer=" << (m_sysTopLayer ? "true" : "false") << "\n"
            << "text_color=" << m_sysTextColor << "\n"
            << "graph_color=" << m_sysGraphColor << "\n"
            << "opacity=" << QString::number(m_sysOpacity, 'f', 2) << "\n"
            << "scale=" << QString::number(m_sysScale, 'f', 2) << "\n"
            << "title_size=" << m_sysTitleSize << "\n"
            << "extra_size=" << m_sysExtraSize << "\n"
            << "margin_x=" << m_sysMarginX << "\n"
            << "margin_y=" << m_sysMarginY << "\n"
            << "position=" << m_sysPosition << "\n";
        f.close();
    }
    emit sysStateChanged();
    if (m_sysRunning) repositionSysWindow(this);
}

void WidgetsManager::toggleSysProcess(bool run) {
    m_sysRunning = run;
    QFile cf(QDir::homePath() + "/.config/hypr/scripts/sysinfo.conf");
    if (cf.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream(&cf) << (run ? "1" : "0") << "\n";
        cf.close();
    }
    if (run) {
        if (!s_sysWin) {
            s_sysWin = new QQuickWindow();
            s_sysItem = new EmbeddedSysInfoItem(this, s_sysWin->contentItem());
            s_sysTickTimer = new QTimer(this);
            s_sysTickTimer->setInterval(1000);
            connect(s_sysTickTimer, &QTimer::timeout, this, []() {
                if (s_sysItem && s_sysWin && s_sysWin->isVisible()) s_sysItem->sampleMetrics();
            });
        }
        repositionSysWindow(this);
        s_sysWin->show();
        makeWindowClickThrough(s_sysWin, m_sysTopLayer);
        s_sysTickTimer->start();
    } else {
        if (s_sysTickTimer) s_sysTickTimer->stop();
        if (s_sysWin) s_sysWin->hide();
    }
    emit sysStateChanged();
}

void WidgetsManager::restartSysProcess() {
    toggleSysProcess(true);
}

void WidgetsManager::resetSysDefaults() {
    m_sysShowCpu = m_sysShowRam = m_sysShowNet = m_sysShowTemp = m_sysShowExtra = m_sysTopLayer = true;
    m_sysTextColor = "#ffffff"; m_sysGraphColor = "#ffffff";
    m_sysOpacity = 0.85; m_sysScale = 1.0;
    m_sysTitleSize = 13; m_sysExtraSize = 14;
    m_sysMarginX = 24; m_sysMarginY = 44;
    m_sysPosition = "top-right";
    saveSysConfig();
}

void WidgetsManager::setSysShowCpu(bool v) { if (m_sysShowCpu != v) { m_sysShowCpu = v; saveSysConfig(); } }
void WidgetsManager::setSysShowRam(bool v) { if (m_sysShowRam != v) { m_sysShowRam = v; saveSysConfig(); } }
void WidgetsManager::setSysShowNet(bool v) { if (m_sysShowNet != v) { m_sysShowNet = v; saveSysConfig(); } }
void WidgetsManager::setSysShowTemp(bool v) { if (m_sysShowTemp != v) { m_sysShowTemp = v; saveSysConfig(); } }
void WidgetsManager::setSysShowExtra(bool v) { if (m_sysShowExtra != v) { m_sysShowExtra = v; saveSysConfig(); } }
void WidgetsManager::setSysTopLayer(bool v) { if (m_sysTopLayer != v) { m_sysTopLayer = v; saveSysConfig(); } }
void WidgetsManager::setSysTextColor(const QString &v) { if (m_sysTextColor != v && !v.isEmpty()) { m_sysTextColor = v; saveSysConfig(); } }
void WidgetsManager::setSysGraphColor(const QString &v) { if (m_sysGraphColor != v && !v.isEmpty()) { m_sysGraphColor = v; saveSysConfig(); } }
void WidgetsManager::setSysOpacity(qreal v) { v = std::clamp(v, 0.1, 1.0); if (!qFuzzyCompare(m_sysOpacity, v)) { m_sysOpacity = v; saveSysConfig(); } }
void WidgetsManager::setSysScale(qreal v) { v = std::clamp(v, 0.4, 2.5); if (!qFuzzyCompare(m_sysScale, v)) { m_sysScale = v; saveSysConfig(); } }
void WidgetsManager::setSysTitleSize(int v) { v = std::clamp(v, 6, 36); if (m_sysTitleSize != v) { m_sysTitleSize = v; saveSysConfig(); } }
void WidgetsManager::setSysExtraSize(int v) { v = std::clamp(v, 6, 36); if (m_sysExtraSize != v) { m_sysExtraSize = v; saveSysConfig(); } }
void WidgetsManager::setSysMarginX(int v) { if (m_sysMarginX != v) { m_sysMarginX = v; saveSysConfig(); } }
void WidgetsManager::setSysMarginY(int v) { if (m_sysMarginY != v) { m_sysMarginY = v; saveSysConfig(); } }
void WidgetsManager::setSysPosition(const QString &v) { if (m_sysPosition != v && !v.isEmpty()) { m_sysPosition = v; saveSysConfig(); } }

void WidgetsManager::setClockOffsetX(int v) { v = std::clamp(v, -1200, 1200); if (m_clockOffsetX != v) { m_clockOffsetX = v; saveClockConfig(); emit clockChanged(); } }
void WidgetsManager::setClockOffsetY(int v) { v = std::clamp(v, -800, 800); if (m_clockOffsetY != v) { m_clockOffsetY = v; saveClockConfig(); emit clockChanged(); } }
void WidgetsManager::shutdownWidgets() {
    if (s_kbPollTimer) s_kbPollTimer->stop();
    if (s_kbWin) s_kbWin->hide();
    if (s_clockTickTimer) s_clockTickTimer->stop();
    if (s_clockWin) s_clockWin->hide();
    if (s_sysTickTimer) s_sysTickTimer->stop();
    if (s_sysWin) s_sysWin->hide();
#ifdef Q_OS_WIN
    if (s_mouseWheelHook) {
        UnhookWindowsHookEx(s_mouseWheelHook);
        s_mouseWheelHook = nullptr;
    }
#endif
}