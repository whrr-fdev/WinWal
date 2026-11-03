#pragma once
#include <QObject>
#include <QVariantMap>

class WidgetsManager : public QObject {
    Q_OBJECT

    // KB Overlay
    Q_PROPERTY(bool kbShowKeyboard READ kbShowKeyboard WRITE setKbShowKeyboard NOTIFY kbChanged)
    Q_PROPERTY(bool kbShowMouse READ kbShowMouse WRITE setKbShowMouse NOTIFY kbChanged)
    Q_PROPERTY(bool kbShowSide READ kbShowSide WRITE setKbShowSide NOTIFY kbChanged)
    Q_PROPERTY(bool kbShowWheel READ kbShowWheel WRITE setKbShowWheel NOTIFY kbChanged)
    Q_PROPERTY(bool kbOnlyCombos READ kbOnlyCombos WRITE setKbOnlyCombos NOTIFY kbChanged)
    Q_PROPERTY(QString kbPosition READ kbPosition WRITE setKbPosition NOTIFY kbChanged)
    Q_PROPERTY(bool kbRunning READ kbRunning NOTIFY kbChanged)

    // Clock Overlay
    Q_PROPERTY(bool clock24h READ clock24h WRITE setClock24h NOTIFY clockChanged)
    Q_PROPERTY(bool clockShowSeconds READ clockShowSeconds WRITE setClockShowSeconds NOTIFY clockChanged)
    Q_PROPERTY(bool clockShowDate READ clockShowDate WRITE setClockShowDate NOTIFY clockChanged)
    Q_PROPERTY(bool clockShowWeekday READ clockShowWeekday WRITE setClockShowWeekday NOTIFY clockChanged)
    Q_PROPERTY(int clockSize READ clockSize WRITE setClockSize NOTIFY clockChanged)
    Q_PROPERTY(qreal clockOpacity READ clockOpacity WRITE setClockOpacity NOTIFY clockChanged)
    Q_PROPERTY(int clockOffsetX READ clockOffsetX WRITE setClockOffsetX NOTIFY clockChanged)
    Q_PROPERTY(int clockOffsetY READ clockOffsetY WRITE setClockOffsetY NOTIFY clockChanged)
    Q_PROPERTY(bool clockRunning READ clockRunning NOTIFY clockChanged)

    Q_PROPERTY(bool sysRunning READ sysRunning NOTIFY sysStateChanged)
    Q_PROPERTY(bool sysShowCpu READ sysShowCpu WRITE setSysShowCpu NOTIFY sysStateChanged)
    Q_PROPERTY(bool sysShowRam READ sysShowRam WRITE setSysShowRam NOTIFY sysStateChanged)
    Q_PROPERTY(bool sysShowNet READ sysShowNet WRITE setSysShowNet NOTIFY sysStateChanged)
    Q_PROPERTY(bool sysShowTemp READ sysShowTemp WRITE setSysShowTemp NOTIFY sysStateChanged)
    Q_PROPERTY(bool sysShowExtra READ sysShowExtra WRITE setSysShowExtra NOTIFY sysStateChanged)
    Q_PROPERTY(bool sysTopLayer READ sysTopLayer WRITE setSysTopLayer NOTIFY sysStateChanged)
    Q_PROPERTY(QString sysTextColor READ sysTextColor WRITE setSysTextColor NOTIFY sysStateChanged)
    Q_PROPERTY(QString sysGraphColor READ sysGraphColor WRITE setSysGraphColor NOTIFY sysStateChanged)
    Q_PROPERTY(qreal sysOpacity READ sysOpacity WRITE setSysOpacity NOTIFY sysStateChanged)
    Q_PROPERTY(qreal sysScale READ sysScale WRITE setSysScale NOTIFY sysStateChanged)
    Q_PROPERTY(int sysTitleSize READ sysTitleSize WRITE setSysTitleSize NOTIFY sysStateChanged)
    Q_PROPERTY(int sysExtraSize READ sysExtraSize WRITE setSysExtraSize NOTIFY sysStateChanged)
    Q_PROPERTY(int sysMarginX READ sysMarginX WRITE setSysMarginX NOTIFY sysStateChanged)
    Q_PROPERTY(int sysMarginY READ sysMarginY WRITE setSysMarginY NOTIFY sysStateChanged)
    Q_PROPERTY(QString sysPosition READ sysPosition WRITE setSysPosition NOTIFY sysStateChanged)

public:
    explicit WidgetsManager(QObject *parent = nullptr);

    bool kbShowKeyboard() const { return m_kbShowKeyboard; }
    void setKbShowKeyboard(bool v);

    bool kbShowMouse() const { return m_kbShowMouse; }
    void setKbShowMouse(bool v);

    bool kbShowSide() const { return m_kbShowSide; }
    void setKbShowSide(bool v);

    bool kbShowWheel() const { return m_kbShowWheel; }
    void setKbShowWheel(bool v);

    bool kbOnlyCombos() const { return m_kbOnlyCombos; }
    void setKbOnlyCombos(bool v);

    QString kbPosition() const { return m_kbPosition; }
    void setKbPosition(const QString &v);

    bool kbRunning() const { return m_kbRunning; }
    bool m_kbRunning = false;
    bool m_clockRunning = false;

    // Clock
    bool clock24h() const { return m_clock24h; }
    void setClock24h(bool v);

    bool clockShowSeconds() const { return m_clockShowSeconds; }
    void setClockShowSeconds(bool v);

    bool clockShowDate() const { return m_clockShowDate; }
    void setClockShowDate(bool v);

    bool clockShowWeekday() const { return m_clockShowWeekday; }
    void setClockShowWeekday(bool v);

    int clockSize() const { return m_clockSize; }
    void setClockSize(int v);

    qreal clockOpacity() const { return m_clockOpacity; }
    void setClockOpacity(qreal v);
    int clockOffsetX() const { return m_clockOffsetX; }
    void setClockOffsetX(int v);
    int clockOffsetY() const { return m_clockOffsetY; }
    void setClockOffsetY(int v);

    bool clockRunning() const { return m_clockRunning; }

    Q_INVOKABLE void toggleKbProcess(bool start);
    Q_INVOKABLE void toggleKbVisibility();
    Q_INVOKABLE void restartKbProcess();

    Q_INVOKABLE void toggleClockProcess(bool start);
    Q_INVOKABLE void restartClockProcess();
    Q_INVOKABLE void saveClock();

    Q_INVOKABLE void launchPeaclock();
    Q_INVOKABLE void launchFastfetch();
    Q_INVOKABLE void launchBtop();

    bool sysRunning() const { return m_sysRunning; }
    bool sysShowCpu() const { return m_sysShowCpu; }
    void setSysShowCpu(bool v);
    bool sysShowRam() const { return m_sysShowRam; }
    void setSysShowRam(bool v);
    bool sysShowNet() const { return m_sysShowNet; }
    void setSysShowNet(bool v);
    bool sysShowTemp() const { return m_sysShowTemp; }
    void setSysShowTemp(bool v);
    bool sysShowExtra() const { return m_sysShowExtra; }
    void setSysShowExtra(bool v);
    bool sysTopLayer() const { return m_sysTopLayer; }
    void setSysTopLayer(bool v);
    QString sysTextColor() const { return m_sysTextColor; }
    void setSysTextColor(const QString &v);
    QString sysGraphColor() const { return m_sysGraphColor; }
    void setSysGraphColor(const QString &v);
    qreal sysOpacity() const { return m_sysOpacity; }
    void setSysOpacity(qreal v);
    qreal sysScale() const { return m_sysScale; }
    void setSysScale(qreal v);
    int sysTitleSize() const { return m_sysTitleSize; }
    void setSysTitleSize(int v);
    int sysExtraSize() const { return m_sysExtraSize; }
    void setSysExtraSize(int v);
    int sysMarginX() const { return m_sysMarginX; }
    void setSysMarginX(int v);
    int sysMarginY() const { return m_sysMarginY; }
    void setSysMarginY(int v);
    QString sysPosition() const { return m_sysPosition; }
    void setSysPosition(const QString &v);

    Q_INVOKABLE void toggleSysProcess(bool run);
    Q_INVOKABLE void restartSysProcess();
    Q_INVOKABLE void resetSysDefaults();
    void shutdownWidgets();

signals:
    void sysStateChanged();
    void kbChanged();
    void clockChanged();

private:
    void loadKbConfig();
    void saveKbConfig();
    void loadClockConfig();
    void saveClockConfig();

    bool m_kbShowKeyboard = true;
    bool m_kbShowMouse = false;
    bool m_kbShowSide = false;
    bool m_kbShowWheel = false;
    bool m_kbOnlyCombos = false;
    QString m_kbPosition = "bottom-right";

    bool m_clock24h = true;
    bool m_clockShowSeconds = false;
    bool m_clockShowDate = true;
    bool m_clockShowWeekday = true;
    int m_clockSize = 64;
    qreal m_clockOpacity = 0.85;
    int m_clockOffsetX = 0;
    int m_clockOffsetY = -40;
    void loadSysConfig();
    void saveSysConfig();
    QString findSysBinary() const;
    bool m_sysRunning = false;
    bool m_sysShowCpu = true, m_sysShowRam = true, m_sysShowNet = true, m_sysShowTemp = true, m_sysShowExtra = true, m_sysTopLayer = true;
    QString m_sysTextColor = "#ffffff", m_sysGraphColor = "#ffffff", m_sysPosition = "top-right";
    qreal m_sysOpacity = 0.85, m_sysScale = 1.0;
    int m_sysTitleSize = 13, m_sysExtraSize = 14, m_sysMarginX = 24, m_sysMarginY = 44;
};
