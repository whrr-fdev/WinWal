#pragma once
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QQuickImageProvider>
#include <QImage>
#include <QTimer>
#include <QProcess>

class WallThumbProvider : public QQuickImageProvider {
public:
    WallThumbProvider();
    static void clearCache();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};

class WallpapersManager : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool daemonRunning READ daemonRunning WRITE setDaemonRunning NOTIFY daemonChanged)
    Q_PROPERTY(QString autoMode READ autoMode WRITE setAutoMode NOTIFY daemonChanged)
    Q_PROPERTY(int autoInterval READ autoInterval WRITE setAutoInterval NOTIFY daemonChanged)
    Q_PROPERTY(bool randomAnim READ randomAnim WRITE setRandomAnim NOTIFY animChanged)
    Q_PROPERTY(QVariantList categories READ categories NOTIFY categoriesChanged)
    Q_PROPERTY(QVariantList wallpapers READ wallpapers NOTIFY wallpapersChanged)
    Q_PROPERTY(QVariantList presets READ presets NOTIFY presetsChanged)
    Q_PROPERTY(int activePresetIndex READ activePresetIndex NOTIFY activePresetChanged)
    Q_PROPERTY(bool downloading READ downloading NOTIFY downloadStatusChanged)
    Q_PROPERTY(QString downloadStatus READ downloadStatus NOTIFY downloadStatusChanged)

public:
    explicit WallpapersManager(QObject *parent = nullptr);

    bool daemonRunning() const { return m_daemonRunning; }
    void setDaemonRunning(bool v);

    QString autoMode() const { return m_autoMode; }
    void setAutoMode(const QString &v);

    int autoInterval() const { return m_autoInterval; }
    void setAutoInterval(int v);

    bool randomAnim() const { return m_randomAnim; }
    void setRandomAnim(bool v);

    QVariantList categories() const { return m_categories; }
    QVariantList wallpapers() const { return m_wallpapers; }
    QVariantList presets() const { return m_presets; }
    int activePresetIndex() const { return m_activePresetIndex; }
    bool downloading() const { return m_downloading; }
    QString downloadStatus() const { return m_downloadStatus; }

    Q_INVOKABLE void applyWallpaper(const QString &path);
    Q_INVOKABLE void applyRandomWallpaper();
    Q_INVOKABLE void clearWallpaper();
    void shutdownRestoreDesktop();
    Q_INVOKABLE void applyPreset(int idx);
    Q_INVOKABLE void savePreset(int idx, const QString &name, const QString &wall, qreal actOp, qreal inactOp, bool blurEn, qreal blurSz);
    Q_INVOKABLE void deletePreset(int idx);
    Q_INVOKABLE QVariantMap snapshotCurrentSettings();
    Q_INVOKABLE void refreshWallpapers();
    Q_INVOKABLE void downloadGithubWallpapers();

signals:
    void daemonChanged();
    void animChanged();
    void categoriesChanged();
    void wallpapersChanged();
    void presetsChanged();
    void activePresetChanged();
    void downloadStatusChanged();

private:
    void scanWallpapers();
    void loadDaemonSettings();
    void loadPresets();
    void savePresetsToFile();
    void applyWallpaperInternal(const QString &path, bool fromPreset);
    void updateDaemonTimer();
    void onAutoWallpaperTick();

    bool m_daemonRunning = false;
    QString m_autoMode = "all";
    int m_autoInterval = 120;
    bool m_randomAnim = true;
    int m_activePresetIndex = -1;
    bool m_downloading = false;
    QString m_downloadStatus;
    QProcess *m_dlProcess = nullptr;

    QVariantList m_categories;
    QVariantList m_wallpapers;
    QVariantList m_presets;
    QTimer m_daemonTimer;
};