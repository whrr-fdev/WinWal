#pragma once
#include <QObject>
#include <QColor>
#include <QTimer>
#include <QSet>
#include <QList>

class LookManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY stateChanged)
    Q_PROPERTY(qreal activeOpacity READ activeOpacity WRITE setActiveOpacity NOTIFY stateChanged)
    Q_PROPERTY(qreal inactiveOpacity READ inactiveOpacity WRITE setInactiveOpacity NOTIFY stateChanged)
    Q_PROPERTY(bool blurEnabled READ blurEnabled WRITE setBlurEnabled NOTIFY stateChanged)
    Q_PROPERTY(int blurMode READ blurMode WRITE setBlurMode NOTIFY stateChanged)
    Q_PROPERTY(qreal blurSize READ blurSize WRITE setBlurSize NOTIFY stateChanged)
    Q_PROPERTY(bool ignoreFullscreen READ ignoreFullscreen WRITE setIgnoreFullscreen NOTIFY stateChanged)
    Q_PROPERTY(QColor selectionColor READ selectionColor WRITE setSelectionColor NOTIFY stateChanged)
    Q_PROPERTY(QColor selectionFillColor READ selectionFillColor WRITE setSelectionFillColor NOTIFY stateChanged)

    // Тайлинг окон (Hyprland-style Tiling WM)
    Q_PROPERTY(bool tilingEnabled READ tilingEnabled WRITE setTilingEnabled NOTIFY stateChanged)
    Q_PROPERTY(int tilingLayout READ tilingLayout WRITE setTilingLayout NOTIFY stateChanged) // 0=Master-Stack, 1=Grid, 2=Columns, 3=Monocle
    Q_PROPERTY(int gapsIn READ gapsIn WRITE setGapsIn NOTIFY stateChanged)
    Q_PROPERTY(int gapsOut READ gapsOut WRITE setGapsOut NOTIFY stateChanged)
    Q_PROPERTY(qreal masterRatio READ masterRatio WRITE setMasterRatio NOTIFY stateChanged)
    Q_PROPERTY(bool floatWinWal READ floatWinWal WRITE setFloatWinWal NOTIFY stateChanged)

public:
    explicit LookManager(QObject *parent = nullptr);
    ~LookManager() override;

    bool enabled() const { return m_enabled; }
    void setEnabled(bool v);

    qreal activeOpacity() const { return m_activeOpacity; }
    void setActiveOpacity(qreal v);

    qreal inactiveOpacity() const { return m_inactiveOpacity; }
    void setInactiveOpacity(qreal v);

    bool blurEnabled() const { return m_blurEnabled; }
    void setBlurEnabled(bool v);

    int blurMode() const { return m_blurMode; }
    void setBlurMode(int v);

    qreal blurSize() const { return m_blurSize; }
    void setBlurSize(qreal v);

    bool ignoreFullscreen() const { return m_ignoreFullscreen; }
    void setIgnoreFullscreen(bool v);

    QColor selectionColor() const { return m_selectionColor; }
    void setSelectionColor(const QColor &c);

    QColor selectionFillColor() const { return m_selectionFillColor; }
    void setSelectionFillColor(const QColor &c);

    bool tilingEnabled() const { return m_tilingEnabled; }
    void setTilingEnabled(bool v);

    int tilingLayout() const { return m_tilingLayout; }
    void setTilingLayout(int v);

    int gapsIn() const { return m_gapsIn; }
    void setGapsIn(int v);

    int gapsOut() const { return m_gapsOut; }
    void setGapsOut(int v);

    qreal masterRatio() const { return m_masterRatio; }
    void setMasterRatio(qreal v);

    bool floatWinWal() const { return m_floatWinWal; }
    void setFloatWinWal(bool v);

    Q_INVOKABLE void applySystemSelectionColors(const QColor &highlightCol, const QColor &hotTrackCol);
    Q_INVOKABLE void restoreDefaultSelectionColors();
    Q_INVOKABLE void resetDefaults();
    Q_INVOKABLE void restoreAllWindows();
    Q_INVOKABLE void retileNow();
    Q_INVOKABLE void toggleActiveWindowFloating();
    Q_INVOKABLE void promoteActiveToMaster();
    Q_INVOKABLE void moveActiveTiledWindow(int dir);
    Q_INVOKABLE void cycleTilingLayout();
    Q_INVOKABLE void clearFloatingExceptions();

signals:
    void stateChanged();

private slots:
    void onTick();

private:
    QString configPath() const;
    void load();
    void save();
    void applyTilingLayoutPass();

    QTimer m_timer;
    bool m_enabled = true;
    qreal m_activeOpacity = 1.0;
    qreal m_inactiveOpacity = 1.0;
    bool m_blurEnabled = true;
    int m_blurMode = 0;
    qreal m_blurSize = 1.0;
    bool m_ignoreFullscreen = true;
    QColor m_selectionColor = QColor(0, 120, 215);
    QColor m_selectionFillColor = QColor(0, 102, 204);
    QSet<quintptr> m_managedHwnds;

    bool m_tilingEnabled = false;
    int m_tilingLayout = 0; // 0=Master-Stack, 1=Grid, 2=Columns, 3=Monocle
    int m_gapsIn = 8;
    int m_gapsOut = 10;
    qreal m_masterRatio = 0.54;
    bool m_floatWinWal = true;
    QList<quintptr> m_tiledOrder;
    QSet<quintptr> m_floatingHwnds;
};