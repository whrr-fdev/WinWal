#pragma once
#include <QObject>
#include <QFileSystemWatcher>
#include <QColor>
#include <QTimer>

class ThemeEngine : public QObject {
    Q_OBJECT
    Q_PROPERTY(int cornerRadius READ cornerRadius NOTIFY cornerRadiusChanged)
    Q_PROPERTY(bool isRounded READ isRounded NOTIFY isRoundedChanged)
    Q_PROPERTY(qreal windowBlur READ windowBlur WRITE setWindowBlur NOTIFY windowBlurChanged)
    Q_PROPERTY(qreal glassRefraction READ glassRefraction WRITE setGlassRefraction NOTIFY glassRefractionChanged)
    Q_PROPERTY(bool controlsLiquidGlass READ controlsLiquidGlass WRITE setControlsLiquidGlass NOTIFY controlsLiquidGlassChanged)
    Q_PROPERTY(QColor accentColor READ accentColor NOTIFY accentColorChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor NOTIFY backgroundColorChanged)

public:
    explicit ThemeEngine(QObject *parent = nullptr);

    int cornerRadius() const { return m_cornerRadius; }
    bool isRounded() const { return m_cornerRadius > 0; }
    qreal windowBlur() const { return m_windowBlur; }
    void setWindowBlur(qreal val);
    qreal glassRefraction() const { return m_glassRefraction; }
    void setGlassRefraction(qreal val);
    bool controlsLiquidGlass() const { return m_controlsLiquidGlass; }
    void setControlsLiquidGlass(bool val);
    QColor accentColor() const { return m_accentColor; }
    QColor backgroundColor() const { return m_backgroundColor; }

    Q_INVOKABLE void applyEffectiveBlur(qreal val);
    Q_INVOKABLE void toggleThemeState();
    Q_INVOKABLE void setRoundedState(bool rounded);
    Q_INVOKABLE void reloadTheme();

signals:
    void cornerRadiusChanged();
    void isRoundedChanged();
    void windowBlurChanged();
    void glassRefractionChanged();
    void controlsLiquidGlassChanged();
    void accentColorChanged();
    void backgroundColorChanged();

private:
    void checkThemeFile();
    QFileSystemWatcher *m_watcher;
    QTimer *m_muteTimer;
    QTimer *m_blurTimer = nullptr;
    qreal m_pendingBlur = 0.10;
    bool m_ignoreWatcher = false;
    int m_cornerRadius = 8;
    qreal m_windowBlur = 0.10;
    qreal m_glassRefraction = 0.0;
    bool m_controlsLiquidGlass = false;
    QColor m_accentColor = QColor("#ffffff");
    QColor m_backgroundColor = QColor(14, 14, 16, 230);
};
