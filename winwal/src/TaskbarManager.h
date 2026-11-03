#pragma once
#include <QObject>
#include <QColor>
#include <QTimer>

class TaskbarManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool autostart READ autostart WRITE setAutostart NOTIFY stateChanged)
    Q_PROPERTY(int screenEdge READ screenEdge WRITE setScreenEdge NOTIFY stateChanged) // 0=Все снизу, 1=Все сверху, 2=Пуск/Трей сверху + Док снизу, 3=Док сверху + Пуск/Трей снизу
    Q_PROPERTY(int style READ style WRITE setStyle NOTIFY stateChanged)
    Q_PROPERTY(QColor tintColor READ tintColor WRITE setTintColor NOTIFY stateChanged)
    Q_PROPERTY(qreal tintOpacity READ tintOpacity WRITE setTintOpacity NOTIFY stateChanged)
    Q_PROPERTY(bool showBorder READ showBorder WRITE setShowBorder NOTIFY stateChanged)

    Q_PROPERTY(bool maxTriggerEnabled READ maxTriggerEnabled WRITE setMaxTriggerEnabled NOTIFY stateChanged)
    Q_PROPERTY(int maxStyle READ maxStyle WRITE setMaxStyle NOTIFY stateChanged)
    Q_PROPERTY(QColor maxTintColor READ maxTintColor WRITE setMaxTintColor NOTIFY stateChanged)
    Q_PROPERTY(qreal maxTintOpacity READ maxTintOpacity WRITE setMaxTintOpacity NOTIFY stateChanged)

    Q_PROPERTY(bool centerIcons READ centerIcons WRITE setCenterIcons NOTIFY stateChanged)
    Q_PROPERTY(bool smoothAnimation READ smoothAnimation WRITE setSmoothAnimation NOTIFY stateChanged)
    Q_PROPERTY(int animSpeed READ animSpeed WRITE setAnimSpeed NOTIFY stateChanged)
    Q_PROPERTY(int centerOffset READ centerOffset WRITE setCenterOffset NOTIFY stateChanged)
    Q_PROPERTY(bool hideStart READ hideStart WRITE setHideStart NOTIFY stateChanged)
    Q_PROPERTY(bool hideSearch READ hideSearch WRITE setHideSearch NOTIFY stateChanged)
    Q_PROPERTY(bool hideTray READ hideTray WRITE setHideTray NOTIFY stateChanged)

    Q_PROPERTY(bool dockMode READ dockMode WRITE setDockMode NOTIFY stateChanged)
    Q_PROPERTY(bool splitThreePills READ splitThreePills WRITE setSplitThreePills NOTIFY stateChanged)
    Q_PROPERTY(bool showWorkspaces READ showWorkspaces WRITE setShowWorkspaces NOTIFY stateChanged)
    Q_PROPERTY(bool autoHideDock READ autoHideDock WRITE setAutoHideDock NOTIFY stateChanged)
    Q_PROPERTY(bool dynamicDockWidth READ dynamicDockWidth WRITE setDynamicDockWidth NOTIFY stateChanged)
    Q_PROPERTY(int dockRadius READ dockRadius WRITE setDockRadius NOTIFY stateChanged)
    Q_PROPERTY(int dockMarginX READ dockMarginX WRITE setDockMarginX NOTIFY stateChanged)
    Q_PROPERTY(int dockMarginY READ dockMarginY WRITE setDockMarginY NOTIFY stateChanged)
    Q_PROPERTY(int workAreaGap READ workAreaGap WRITE setWorkAreaGap NOTIFY stateChanged)

    Q_PROPERTY(int startPopupPos READ startPopupPos WRITE setStartPopupPos NOTIFY stateChanged)
    Q_PROPERTY(int trayPopupPos READ trayPopupPos WRITE setTrayPopupPos NOTIFY stateChanged)
    Q_PROPERTY(bool flyoutGlassEnabled READ flyoutGlassEnabled WRITE setFlyoutGlassEnabled NOTIFY stateChanged)
    Q_PROPERTY(int flyoutStyle READ flyoutStyle WRITE setFlyoutStyle NOTIFY stateChanged)
    Q_PROPERTY(QColor flyoutTintColor READ flyoutTintColor WRITE setFlyoutTintColor NOTIFY stateChanged)
    Q_PROPERTY(qreal flyoutOpacity READ flyoutOpacity WRITE setFlyoutOpacity NOTIFY stateChanged)

public:
    explicit TaskbarManager(QObject *parent = nullptr);
    ~TaskbarManager() override;

    bool autostart() const;
    void setAutostart(bool v);
    bool enabled() const { return m_enabled; }
    void setEnabled(bool v);
    int screenEdge() const { return m_screenEdge; }
    void setScreenEdge(int v);
    int style() const { return m_style; }
    void setStyle(int v);
    QColor tintColor() const { return m_tintColor; }
    void setTintColor(const QColor &c);
    qreal tintOpacity() const { return m_tintOpacity; }
    void setTintOpacity(qreal v);
    bool showBorder() const { return m_showBorder; }
    void setShowBorder(bool v);

    bool maxTriggerEnabled() const { return m_maxTriggerEnabled; }
    void setMaxTriggerEnabled(bool v);
    int maxStyle() const { return m_maxStyle; }
    void setMaxStyle(int v);
    QColor maxTintColor() const { return m_maxTintColor; }
    void setMaxTintColor(const QColor &c);
    qreal maxTintOpacity() const { return m_maxTintOpacity; }
    void setMaxTintOpacity(qreal v);

    bool centerIcons() const { return m_centerIcons; }
    void setCenterIcons(bool v);
    bool smoothAnimation() const { return m_smoothAnimation; }
    void setSmoothAnimation(bool v);
    int animSpeed() const { return m_animSpeed; }
    void setAnimSpeed(int v);
    int centerOffset() const { return m_centerOffset; }
    void setCenterOffset(int v);
    bool hideStart() const { return m_hideStart; }
    void setHideStart(bool v);
    bool hideSearch() const { return m_hideSearch; }
    void setHideSearch(bool v);
    bool hideTray() const { return m_hideTray; }
    void setHideTray(bool v);

    bool dockMode() const { return m_dockMode; }
    void setDockMode(bool v);
    bool splitThreePills() const { return m_splitThreePills; }
    void setSplitThreePills(bool v);
    bool autoHideDock() const { return m_autoHideDock; }
    void setAutoHideDock(bool v);
    bool showWorkspaces() const { return m_showWorkspaces; }
    void setShowWorkspaces(bool v);
    bool dynamicDockWidth() const { return m_dynamicDockWidth; }
    void setDynamicDockWidth(bool v);
    int dockRadius() const { return m_dockRadius; }
    void setDockRadius(int v);
    int dockMarginX() const { return m_dockMarginX; }
    void setDockMarginX(int v);
    int workAreaGap() const { return m_workAreaGap; }
    void setWorkAreaGap(int v);
    int dockMarginY() const { return m_dockMarginY; }
    void setDockMarginY(int v);

    int startPopupPos() const { return m_startPopupPos; }
    void setStartPopupPos(int v);
    int trayPopupPos() const { return m_trayPopupPos; }
    void setTrayPopupPos(int v);
    bool flyoutGlassEnabled() const { return m_flyoutGlassEnabled; }
    void setFlyoutGlassEnabled(bool v);
    int flyoutStyle() const { return m_flyoutStyle; }
    void setFlyoutStyle(int v);
    QColor flyoutTintColor() const { return m_flyoutTintColor; }
    void setFlyoutTintColor(const QColor &c);
    qreal flyoutOpacity() const { return m_flyoutOpacity; }
    void setFlyoutOpacity(qreal v);

    Q_INVOKABLE void applyPreset(int presetIdx);
    Q_INVOKABLE void resetDefaults();
    Q_INVOKABLE void restoreNativeTaskbar();

signals:
    void stateChanged();

private slots:
    void onTick();

private:
    QString configPath() const;
    void load();
    void save();
    void applyVisibilityFlags();

    QTimer m_timer;
    bool m_enabled = true;
    int m_screenEdge = 0;
    int m_style = 1;
    QColor m_tintColor = QColor("#0a0a0c");
    qreal m_tintOpacity = 0.0;
    bool m_showBorder = false;

    bool m_maxTriggerEnabled = true;
    int m_maxStyle = 3;
    QColor m_maxTintColor = QColor("#0a0a0c");
    qreal m_maxTintOpacity = 0.96;

    bool m_centerIcons = false;
    bool m_smoothAnimation = true;
    int m_animSpeed = 220;
    int m_centerOffset = 0;
    bool m_hideStart = false;
    bool m_hideSearch = false;
    bool m_hideTray = false;

    bool m_dockMode = true;
    bool m_splitThreePills = true;
    bool m_showWorkspaces = true;
    bool m_autoHideDock = false;
    bool m_dynamicDockWidth = true;
    int m_dockRadius = 0;
    int m_dockMarginX = 0;
    int m_dockMarginY = 0;
    int m_workAreaGap = -4;

    int m_startPopupPos = 0;
    int m_trayPopupPos = 0;
    bool m_flyoutGlassEnabled = true;
    int m_flyoutStyle = 2;
    QColor m_flyoutTintColor = QColor("#0a0a0c");
    qreal m_flyoutOpacity = 0.72;

    double m_currentTaskListX = -1.0;
    int m_lastIconsSpan = 0;
    int m_tickCounter = 0;
};